//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "rpc/handlers/Ledger.h"

#include "data/Types.h"
#include "rpc/Errors.h"
#include "rpc/JS.h"
#include "rpc/RPCHelpers.h"
#include "rpc/common/Types.h"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/kind.hpp>
#include <boost/json/object.hpp>
#include <boost/json/string.hpp>
#include <boost/json/value.hpp>
#include <ripple/basics/chrono.h>
#include <ripple/basics/strHex.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/TxFormats.h>
#include <ripple/protocol/jss.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <variant>

namespace rpc {
LedgerHandler::Result
LedgerHandler::process(LedgerHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);
    Output output;

    output.header = toJson(lgrInfo, input.binary, ctx.apiVersion);

    if (input.transactions) {
        output.header[JS(transactions)] = boost::json::value(boost::json::array_kind);
        boost::json::array& jsonTxs = output.header.at(JS(transactions)).as_array();

        if (input.expand) {
            auto txns = sharedPtrBackend_->fetchAllTransactionsInLedger(lgrInfo.seq, ctx.yield);

            auto const expandTxJsonV1 = [&](data::TransactionAndMetadata const& tx) {
                if (!input.binary) {
                    auto [txn, meta] = toExpandedJson(tx, ctx.apiVersion);
                    txn[JS(metaData)] = std::move(meta);
                    return txn;
                }
                return toJsonWithBinaryTx(tx, ctx.apiVersion);
            };

            auto const isoTimeStr = ripple::to_string_iso(lgrInfo.closeTime);

            auto const expandTxJsonV2 = [&](data::TransactionAndMetadata const& tx) {
                auto [txn, meta] = toExpandedJson(tx, ctx.apiVersion);
                if (!input.binary) {
                    boost::json::object entry;
                    entry[JS(validated)] = true;

                    if (ctx.apiVersion < 2u) {
                        entry[JS(ledger_index)] = std::to_string(lgrInfo.seq);
                    } else {
                        entry[JS(ledger_index)] = lgrInfo.seq;
                    }

                    entry[JS(close_time_iso)] = isoTimeStr;
                    entry[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
                    if (txn.contains(JS(hash))) {
                        entry[JS(hash)] = txn.at(JS(hash));
                        txn.erase(JS(hash));
                    }
                    entry[JS(tx_json)] = std::move(txn);
                    entry[JS(meta)] = std::move(meta);
                    return entry;
                }

                auto entry = toJsonWithBinaryTx(tx, ctx.apiVersion);
                if (txn.contains(JS(hash)))
                    entry[JS(hash)] = txn.at(JS(hash));
                return entry;
            };

            std::transform(
                std::move_iterator(txns.begin()),
                std::move_iterator(txns.end()),
                std::back_inserter(jsonTxs),
                [&](auto obj) {
                    boost::json::object entry = ctx.apiVersion < 2u ? expandTxJsonV1(obj) : expandTxJsonV2(obj);

                    if (input.ownerFunds) {
                        // check the type of tx
                        auto const [tx, meta] = rpc::deserializeTxPlusMeta(obj);
                        if (tx and tx->isFieldPresent(ripple::sfTransactionType) and
                            tx->getTxnType() == ripple::ttOFFER_CREATE) {
                            auto const account = tx->getAccountID(ripple::sfAccount);
                            auto const amount = tx->getFieldAmount(ripple::sfTakerGets);

                            // If the offer create is not self funded then add the
                            // owner balance
                            if (account != amount.getIssuer()) {
                                auto const ownerFunds = accountHolds(
                                    *sharedPtrBackend_,
                                    lgrInfo.seq,
                                    account,
                                    amount.getAsset(),
                                    amount.getIssuer(),
                                    false,  // fhIGNORE_FREEZE from rippled
                                    ctx.yield
                                );
                                entry[JS(owner_funds)] = ownerFunds.getText();
                            }
                        }
                    }
                    return entry;
                }
            );
        } else {
            auto hashes = sharedPtrBackend_->fetchAllTransactionHashesInLedger(lgrInfo.seq, ctx.yield);
            std::transform(
                std::move_iterator(hashes.begin()),
                std::move_iterator(hashes.end()),
                std::back_inserter(jsonTxs),
                [](auto hash) { return boost::json::string(ripple::strHex(hash)); }
            );
        }
    }

    if (input.diff) {
        output.header["diff"] = boost::json::value(boost::json::array_kind);

        boost::json::array& jsonDiff = output.header.at("diff").as_array();
        auto diff = sharedPtrBackend_->fetchLedgerDiff(lgrInfo.seq, ctx.yield);

        for (auto const& obj : diff) {
            boost::json::object entry;
            entry["object_id"] = ripple::strHex(obj.key);

            if (input.binary) {
                entry["object"] = ripple::strHex(obj.blob);
            } else if (!obj.blob.empty()) {
                ripple::STLedgerEntry const sle{ripple::SerialIter{obj.blob.data(), obj.blob.size()}, obj.key};
                entry["object"] = toJson(sle);
            } else {
                entry["object"] = "";
            }

            jsonDiff.push_back(std::move(entry));
        }
    }

    output.ledgerHash = ripple::strHex(lgrInfo.hash);
    output.ledgerIndex = lgrInfo.seq;

    return output;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, LedgerHandler::Output const& output)
{
    jv = boost::json::object{
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(ledger), output.header},
    };
}

LedgerHandler::Input
tag_invoke(boost::json::value_to_tag<LedgerHandler::Input>, boost::json::value const& jv)
{
    auto input = LedgerHandler::Input{};
    auto const& jsonObject = jv.as_object();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = jv.at(JS(ledger_hash)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(jv.at(JS(ledger_index)).as_string().c_str());
        }
    }

    if (jsonObject.contains(JS(transactions)))
        input.transactions = jv.at(JS(transactions)).as_bool();

    if (jsonObject.contains(JS(binary)))
        input.binary = jv.at(JS(binary)).as_bool();

    if (jsonObject.contains(JS(expand)))
        input.expand = jv.at(JS(expand)).as_bool();

    if (jsonObject.contains(JS(owner_funds)))
        input.ownerFunds = jv.at(JS(owner_funds)).as_bool();

    if (jsonObject.contains("diff"))
        input.diff = jv.at("diff").as_bool();

    return input;
}

}  // namespace rpc
