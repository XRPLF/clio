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

#pragma once

#include "data/BackendInterface.h"
#include "data/Types.h"
#include "etl/ETLService.h"
#include "rpc/Errors.h"
#include "rpc/JS.h"
#include "rpc/RPCHelpers.h"
#include "rpc/common/JsonBool.h"
#include "rpc/common/Specs.h"
#include "rpc/common/Types.h"
#include "rpc/common/Validators.h"
#include "util/JsonUtils.h"

#include <boost/asio/spawn.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/strHex.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

template <typename ETLServiceType>
class BaseTxHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<ETLServiceType const> etl_;

public:
    struct Output {
        uint32_t date = 0u;
        std::string hash{};
        uint32_t ledgerIndex = 0u;
        std::optional<boost::json::object> meta{};
        std::optional<boost::json::object> tx{};
        std::optional<std::string> metaStr{};
        std::optional<std::string> txStr{};
        std::optional<std::string> ctid{};                   // ctid when binary=true
        std::optional<ripple::LedgerHeader> ledgerHeader{};  // ledger hash when apiVersion >= 2
        uint32_t apiVersion = 0u;
        bool validated = true;
    };

    struct Input {
        std::optional<std::string> transaction;
        std::optional<std::string> ctid;
        bool binary = false;
        std::optional<uint32_t> minLedger;
        std::optional<uint32_t> maxLedger;
    };

    using Result = HandlerReturnType<Output>;

    BaseTxHandler(
        std::shared_ptr<BackendInterface> const& sharedPtrBackend,
        std::shared_ptr<ETLServiceType const> const& etl
    )
        : sharedPtrBackend_(sharedPtrBackend), etl_(etl)
    {
    }

    static RpcSpecConstRef
    spec(uint32_t apiVersion)
    {
        static RpcSpec const rpcSpecForV1 = {
            {JS(transaction), validation::Uint256HexStringValidator},
            {JS(min_ledger), validation::Type<uint32_t>{}},
            {JS(max_ledger), validation::Type<uint32_t>{}},
            {JS(ctid), validation::Type<std::string>{}},
        };

        static auto const rpcSpec = RpcSpec{rpcSpecForV1, {{JS(binary), validation::Type<bool>{}}}};

        return apiVersion == 1 ? rpcSpecForV1 : rpcSpec;
    }

    Result
    process(Input input, Context const& ctx) const
    {
        if (input.ctid && input.transaction)  // ambiguous identifier
            return Error{Status{RippledError::rpcINVALID_PARAMS}};

        if (!input.ctid && !input.transaction)  // at least one identifier must be supplied
            return Error{Status{RippledError::rpcINVALID_PARAMS}};

        static auto constexpr maxLedgerRange = 1000u;
        auto const rangeSupplied = input.minLedger && input.maxLedger;

        if (rangeSupplied) {
            if (*input.minLedger > *input.maxLedger)
                return Error{Status{RippledError::rpcINVALID_LGR_RANGE}};

            if (*input.maxLedger - *input.minLedger > maxLedgerRange)
                return Error{Status{RippledError::rpcEXCESSIVE_LGR_RANGE}};
        }

        std::optional<uint32_t> currentNetId = std::nullopt;
        if (auto const& etlState = etl_->getETLState(); etlState.has_value())
            currentNetId = etlState->networkID;

        std::optional<data::TransactionAndMetadata> dbResponse;

        if (input.ctid) {
            auto const ctid = rpc::decodeCTID(*input.ctid);
            if (!ctid)
                return Error{Status{RippledError::rpcINVALID_PARAMS}};

            auto const [lgrSeq, txnIdx, netId] = *ctid;
            // when current network id is available, let us check the network id from parameter
            if (currentNetId && netId != *currentNetId) {
                return Error{Status{
                    RippledError::rpcWRONG_NETWORK,
                    fmt::format(
                        "Wrong network. You should submit this request to a node running on NetworkID: {}", netId
                    )
                }};
            }

            dbResponse = fetchTxViaCtid(lgrSeq, txnIdx, ctx.yield);
        } else {
            dbResponse = sharedPtrBackend_->fetchTransaction(ripple::uint256{input.transaction->c_str()}, ctx.yield);
        }

        auto output = BaseTxHandler::Output{.apiVersion = ctx.apiVersion};

        if (!dbResponse) {
            if (rangeSupplied && input.transaction)  // ranges not for ctid
            {
                auto const range = sharedPtrBackend_->fetchLedgerRange();
                auto const searchedAll =
                    range->maxSequence >= *input.maxLedger && range->minSequence <= *input.minLedger;
                boost::json::object extra;
                extra["searched_all"] = searchedAll;

                return Error{Status{RippledError::rpcTXN_NOT_FOUND, std::move(extra)}};
            }

            return Error{Status{RippledError::rpcTXN_NOT_FOUND}};
        }

        auto const [txn, meta] = toExpandedJson(*dbResponse, ctx.apiVersion, NFTokenjson::ENABLE, currentNetId);

        if (!input.binary) {
            output.tx = txn;
            output.meta = meta;
        } else {
            output.txStr = ripple::strHex(dbResponse->transaction);
            output.metaStr = ripple::strHex(dbResponse->metadata);

            // input.transaction might be not available, get hash via tx object
            if (txn.contains(JS(hash)))
                output.hash = txn.at(JS(hash)).as_string();

            // append ctid here to mimic rippled 1.12 behavior: return ctid even binary=true
            // rippled will change it in the future, ctid should be part of tx json which not available in binary
            // mode
            auto const txnIdx = boost::json::value_to<uint64_t>(meta.at("TransactionIndex"));
            if (txnIdx <= 0xFFFFU && dbResponse->ledgerSequence < 0x0FFF'FFFFUL && currentNetId &&
                *currentNetId <= 0xFFFFU) {
                output.ctid = rpc::encodeCTID(
                    dbResponse->ledgerSequence, static_cast<uint16_t>(txnIdx), static_cast<uint16_t>(*currentNetId)
                );
            }
        }

        output.date = dbResponse->date;
        output.ledgerIndex = dbResponse->ledgerSequence;

        // fetch ledger hash
        if (ctx.apiVersion > 1u)
            output.ledgerHeader = sharedPtrBackend_->fetchLedgerBySequence(dbResponse->ledgerSequence, ctx.yield);

        return output;
    }

private:
    std::optional<data::TransactionAndMetadata>
    fetchTxViaCtid(uint32_t ledgerSeq, uint32_t txId, boost::asio::yield_context yield) const
    {
        auto const txs = sharedPtrBackend_->fetchAllTransactionsInLedger(ledgerSeq, yield);

        for (auto const& tx : txs) {
            auto const [txn, meta] = deserializeTxPlusMeta(tx, ledgerSeq);

            if (meta->getIndex() == txId)
                return tx;
        }

        return std::nullopt;
    }

    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output)
    {
        auto const getJsonV1 = [&]() {
            auto obj = boost::json::object{};

            if (output.tx) {
                obj = *output.tx;
                obj[JS(meta)] = *output.meta;
            } else {
                obj[JS(meta)] = *output.metaStr;
                obj[JS(tx)] = *output.txStr;
                obj[JS(hash)] = output.hash;
            }

            obj[JS(validated)] = output.validated;
            obj[JS(date)] = output.date;
            obj[JS(ledger_index)] = output.ledgerIndex;
            obj[JS(inLedger)] = output.ledgerIndex;
            return obj;
        };

        auto const getJsonV2 = [&]() {
            auto obj = boost::json::object{};

            if (output.tx) {
                obj[JS(tx_json)] = *output.tx;
                obj[JS(tx_json)].as_object()[JS(date)] = output.date;
                obj[JS(tx_json)].as_object()[JS(ledger_index)] = output.ledgerIndex;
                // move ctid from tx_json to root
                if (obj[JS(tx_json)].as_object().contains(JS(ctid))) {
                    obj[JS(ctid)] = obj[JS(tx_json)].as_object()[JS(ctid)];
                    obj[JS(tx_json)].as_object().erase(JS(ctid));
                }
                // move hash from tx_json to root
                if (obj[JS(tx_json)].as_object().contains(JS(hash))) {
                    obj[JS(hash)] = obj[JS(tx_json)].as_object()[JS(hash)];
                    obj[JS(tx_json)].as_object().erase(JS(hash));
                }
                obj[JS(meta)] = *output.meta;
            } else {
                obj[JS(meta_blob)] = *output.metaStr;
                obj[JS(tx_blob)] = *output.txStr;
                obj[JS(hash)] = output.hash;
            }

            obj[JS(validated)] = output.validated;
            obj[JS(ledger_index)] = output.ledgerIndex;

            if (output.ledgerHeader) {
                obj[JS(ledger_hash)] = ripple::strHex(output.ledgerHeader->hash);
                obj[JS(close_time_iso)] = ripple::to_string_iso(output.ledgerHeader->closeTime);
            }
            return obj;
        };

        auto obj = output.apiVersion > 1u ? getJsonV2() : getJsonV1();

        if (output.ctid)
            obj[JS(ctid)] = *output.ctid;

        jv = std::move(obj);
    }

    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv)
    {
        auto input = BaseTxHandler::Input{};
        auto const& jsonObject = jv.as_object();

        if (jsonObject.contains(JS(transaction)))
            input.transaction = jv.at(JS(transaction)).as_string().c_str();

        if (jsonObject.contains(JS(ctid))) {
            input.ctid = jv.at(JS(ctid)).as_string().c_str();
            input.ctid = util::toUpper(*input.ctid);
        }

        if (jsonObject.contains(JS(binary)))
            input.binary = boost::json::value_to<JsonBool>(jsonObject.at(JS(binary)));

        if (jsonObject.contains(JS(min_ledger)))
            input.minLedger = jv.at(JS(min_ledger)).as_int64();

        if (jsonObject.contains(JS(max_ledger)))
            input.maxLedger = jv.at(JS(max_ledger)).as_int64();

        return input;
    }
};

/**
 * @brief The tx method retrieves information on a single transaction, by its identifying hash.
 *
 * For more details see: https://xrpl.org/tx.html
 */
using TxHandler = BaseTxHandler<etl::ETLService>;
}  // namespace rpc
