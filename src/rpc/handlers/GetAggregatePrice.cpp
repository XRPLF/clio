//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "rpc/handlers/GetAggregatePrice.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <ripple/basics/Number.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/Issue.h>
#include <ripple/protocol/LedgerFormats.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STAmount.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/tokens.h>

#include <cstdint>
#include <functional>
#include <iterator>
#include <numeric>
#include <optional>
#include <string>
#include <variant>

namespace rpc {

GetAggregatePriceHandler::Result
GetAggregatePriceHandler::process(GetAggregatePriceHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);

    // sorted descending by lastUpdateTime, ascending by AssetPrice
    using TimestampPricesBiMap = boost::bimaps::bimap<
        boost::bimaps::multiset_of<std::uint32_t, std::greater<std::uint32_t>>,
        boost::bimaps::multiset_of<ripple::STAmount>>;

    TimestampPricesBiMap timestampPricesBiMap;

    for (auto const& oracle : input.oracles) {
        auto const account =
            ripple::parseBase58<ripple::AccountID>(boost::json::value_to<std::string>(oracle.as_object().at(JS(account))
            ));
        auto const docId = boost::json::value_to<std::uint64_t>(oracle.as_object().at(JS(oracle_document_id)));

        auto const oracleIndex = ripple::keylet::oracle(*account, docId).key;

        auto const oracleObject = sharedPtrBackend_->fetchLedgerObject(oracleIndex, lgrInfo.seq, ctx.yield);

        if (not oracleObject)
            continue;

        ripple::STLedgerEntry const oracleSle{
            ripple::SerialIter{oracleObject->data(), oracleObject->size()}, oracleIndex
        };

        tracebackOracleObject(ctx.yield, oracleSle, [&](auto const& node) {
            auto const& series = node.getFieldArray(ripple::sfPriceDataSeries);
            // find the token pair entry with the price
            if (auto iter = std::find_if(
                    series.begin(),
                    series.end(),
                    [&](ripple::STObject const& o) -> bool {
                        return o.getFieldCurrency(ripple::sfBaseAsset).getText() == input.baseAsset and
                            o.getFieldCurrency(ripple::sfQuoteAsset).getText() == input.quoteAsset and
                            o.isFieldPresent(ripple::sfAssetPrice);
                    }
                );
                iter != series.end()) {
                auto const price = iter->getFieldU64(ripple::sfAssetPrice);
                auto const scale =
                    iter->isFieldPresent(ripple::sfScale) ? -static_cast<int>(iter->getFieldU8(ripple::sfScale)) : 0;
                timestampPricesBiMap.insert(TimestampPricesBiMap::value_type(
                    node.getFieldU32(ripple::sfLastUpdateTime), ripple::STAmount{ripple::noIssue(), price, scale}
                ));
                return true;
            }
            return false;
        });
    }

    if (timestampPricesBiMap.empty())
        return Error{Status{ripple::rpcOBJECT_NOT_FOUND}};

    auto const latestTime = timestampPricesBiMap.left.begin()->first;

    Output out;
    out.time = latestTime;

    if (input.timeThreshold) {
        auto const oldestTime = timestampPricesBiMap.left.rbegin()->first;
        auto const upperBound = latestTime > *input.timeThreshold ? (latestTime - *input.timeThreshold) : oldestTime;
        if (upperBound > oldestTime) {
            timestampPricesBiMap.left.erase(
                timestampPricesBiMap.left.upper_bound(upperBound), timestampPricesBiMap.left.end()
            );
        }

        if (timestampPricesBiMap.empty())
            return Error{Status{ripple::rpcOBJECT_NOT_FOUND}};
    }

    auto const getStats = [](TimestampPricesBiMap::right_const_iterator begin,
                             TimestampPricesBiMap::right_const_iterator end) -> Stats {
        ripple::STAmount avg{ripple::noIssue(), 0, 0};
        ripple::Number sd{0};
        std::uint16_t const size = std::distance(begin, end);
        avg = std::accumulate(begin, end, avg, [&](ripple::STAmount const& acc, auto const& it) {
            return acc + it.first;
        });
        avg = divide(avg, ripple::STAmount{ripple::noIssue(), size, 0}, ripple::noIssue());
        if (size > 1) {
            sd = std::accumulate(begin, end, sd, [&](ripple::Number const& acc, auto const& it) {
                return acc + (it.first - avg) * (it.first - avg);
            });
            sd = root2(sd / (size - 1));
        }
        return {avg, sd, size};
    };

    out.extireStats = getStats(timestampPricesBiMap.right.begin(), timestampPricesBiMap.right.end());

    auto itAdvance = [&](auto it, int distance) {
        std::advance(it, distance);
        return it;
    };

    if (input.trim) {
        auto const trimCount = timestampPricesBiMap.size() * (*input.trim) / 100;

        auto const [avg, sd, size] = getStats(
            itAdvance(timestampPricesBiMap.right.begin(), trimCount),
            itAdvance(timestampPricesBiMap.right.end(), -trimCount)
        );
    }

    auto const median = [&, size = out.extireStats.size]() {
        auto const middle = size / 2;
        if ((size % 2) == 0) {
            static ripple::STAmount two{ripple::noIssue(), 2, 0};
            auto it = itAdvance(timestampPricesBiMap.right.begin(), middle - 1);
            auto const& a1 = it->first;
            auto const& a2 = (++it)->first;
            return divide(a1 + a2, two, ripple::noIssue());
        }
        return itAdvance(timestampPricesBiMap.right.begin(), middle)->first;
    }();
    out.median = median.getText();

    return out;
}

void
GetAggregatePriceHandler::tracebackOracleObject(
    boost::asio::yield_context yield,
    ripple::STObject oracleObject,
    std::function<bool(ripple::STObject const&)> const& callback
) const
{
    auto constexpr maxHistory = 3;

    ripple::STObject const* ptrOracleObject = &oracleObject;
    ripple::STObject const* ptrCurrentObject = ptrOracleObject;

    bool isNew = false;
    bool noOracleFound = false;
    auto history = 0;

    while (true) {
        if (noOracleFound)
            return;

        if (not callback(*ptrOracleObject))
            return;

        if (++history > maxHistory)
            return;

        auto const prevTxIndex = ptrCurrentObject->getFieldH256(ripple::sfPreviousTxnID);

        auto const prevTx = sharedPtrBackend_->fetchTransaction(prevTxIndex, yield);

        if (not prevTx)
            return;

        noOracleFound = true;
        auto [_, meta] = deserializeTxPlusMeta(*prevTx);

        for (ripple::STObject const& node : meta->getFieldArray(ripple::sfAffectedNodes)) {
            if (node.getFieldU16(ripple::sfLedgerEntryType) != ripple::ltORACLE) {
                continue;
            }
            noOracleFound = false;
            ptrCurrentObject = &node;
            isNew = node.isFieldPresent(ripple::sfNewFields);
            // if a meta is for the new and this is the first
            // look-up then it's the meta for the tx that
            // created the current object; i.e. there is no
            // historical data
            if (isNew and history == 1)
                return;

            ptrOracleObject = isNew ? &dynamic_cast<ripple::STObject const&>(node.peekAtField(ripple::sfNewFields))
                                    : &dynamic_cast<ripple::STObject const&>(node.peekAtField(ripple::sfFinalFields));
            break;
        }
    }
}

GetAggregatePriceHandler::Input
tag_invoke(boost::json::value_to_tag<GetAggregatePriceHandler::Input>, boost::json::value const& jv)
{
    auto input = GetAggregatePriceHandler::Input{};
    auto const& jsonObject = jv.as_object();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(boost::json::value_to<std::string>(jv.at(JS(ledger_index))));
        }
    }

    input.oracles = jsonObject.at(JS(oracles)).as_array();

    return input;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, GetAggregatePriceHandler::Output const& output)
{
    jv = boost::json::object{
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
    };
}

}  // namespace rpc
