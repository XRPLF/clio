#include "rpc/handlers/NFTHistory.hpp"

#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"
#include "util/Profiler.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

// TODO: this is currently very similar to account_tx but its own copy for time
// being. we should aim to reuse common logic in some way in the future.
NFTHistoryHandler::Result
NFTHistoryHandler::process(NFTHistoryHandler::Input const& input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "NFTHistory's ledger range must be available");

    auto [minIndex, maxIndex] = *range;

    if (input.ledgerIndexMin) {
        if (range->maxSequence < input.ledgerIndexMin || range->minSequence > input.ledgerIndexMin)
            return Error{Status{RippledError::rpcLGR_IDX_MALFORMED, "ledgerSeqMinOutOfRange"}};

        minIndex = *input.ledgerIndexMin;
    }

    if (input.ledgerIndexMax) {
        if (range->maxSequence < input.ledgerIndexMax || range->minSequence > input.ledgerIndexMax)
            return Error{Status{RippledError::rpcLGR_IDX_MALFORMED, "ledgerSeqMaxOutOfRange"}};

        maxIndex = *input.ledgerIndexMax;
    }

    if (minIndex > maxIndex)
        return Error{Status{RippledError::rpcLGR_IDXS_INVALID}};

    if (input.ledgerHash || input.ledgerIndex) {
        // rippled does not have this check
        if (input.ledgerIndexMax || input.ledgerIndexMin) {
            return Error{
                Status{RippledError::rpcINVALID_PARAMS, "containsLedgerSpecifierAndRange"}
            };
        }

        auto const expectedLgrInfo = getLedgerHeaderFromHashOrSeq(
            *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
        );

        if (not expectedLgrInfo.has_value())
            return Error{expectedLgrInfo.error()};

        maxIndex = minIndex = expectedLgrInfo.value().seq;
    }

    std::optional<data::TransactionsCursor> cursor;

    // if marker exists
    if (input.marker) {
        cursor = {input.marker->ledger, input.marker->seq};
    } else {
        if (input.forward) {
            cursor = {minIndex, 0};
        } else {
            cursor = {maxIndex, std::numeric_limits<int32_t>::max()};
        }
    }

    auto const limit = input.limit.value_or(kLIMIT_DEFAULT);
    auto const tokenID = ripple::uint256{input.nftID.c_str()};

    auto const [txnsAndCursor, timeDiff] = util::timed([&]() {
        return sharedPtrBackend_->fetchNFTTransactions(
            tokenID, limit, input.forward, cursor, ctx.yield
        );
    });
    LOG(log_.info()) << "db fetch took " << timeDiff
                     << " milliseconds - num blobs = " << txnsAndCursor.txns.size();

    Output response;
    auto const [blobs, retCursor] = txnsAndCursor;

    if (retCursor)
        response.marker = {.ledger = retCursor->ledgerSequence, .seq = retCursor->transactionIndex};

    for (auto const& txnPlusMeta : blobs) {
        // over the range
        if ((txnPlusMeta.ledgerSequence < minIndex && !input.forward) ||
            (txnPlusMeta.ledgerSequence > maxIndex && input.forward)) {
            response.marker = std::nullopt;
            break;
        }
        if (txnPlusMeta.ledgerSequence > maxIndex && !input.forward) {
            LOG(log_.debug()) << "Skipping over transactions from incomplete ledger";
            continue;
        }

        boost::json::object obj;

        if (!input.binary) {
            auto [txn, meta] = toExpandedJson(txnPlusMeta, ctx.apiVersion);
            auto const txKey = ctx.apiVersion > 1u ? JS(tx_json) : JS(tx);
            obj[JS(meta)] = std::move(meta);
            obj[txKey] = std::move(txn);
            obj[txKey].as_object()[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
            obj[txKey].as_object()[JS(date)] = txnPlusMeta.date;
            if (ctx.apiVersion > 1u) {
                obj[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
                if (obj[txKey].as_object().contains(JS(hash))) {
                    obj[JS(hash)] = obj[txKey].at(JS(hash));
                    obj[txKey].as_object().erase(JS(hash));
                }
                if (auto const lgrInfo = sharedPtrBackend_->fetchLedgerBySequence(
                        txnPlusMeta.ledgerSequence, ctx.yield
                    );
                    lgrInfo) {
                    obj[JS(close_time_iso)] = ripple::to_string_iso(lgrInfo->closeTime);
                    obj[JS(ledger_hash)] = ripple::strHex(lgrInfo->hash);
                }
            }
        } else {
            obj = toJsonWithBinaryTx(txnPlusMeta, ctx.apiVersion);
            obj[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
            obj[JS(date)] = txnPlusMeta.date;
        }

        obj[JS(validated)] = true;
        response.transactions.push_back(obj);
    }

    response.limit = input.limit;
    response.nftID = ripple::to_string(tokenID);
    response.ledgerIndexMin = minIndex;
    response.ledgerIndexMax = maxIndex;

    return response;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    NFTHistoryHandler::Output const& output
)
{
    jv = {
        {JS(nft_id), output.nftID},
        {JS(ledger_index_min), output.ledgerIndexMin},
        {JS(ledger_index_max), output.ledgerIndexMax},
        {JS(transactions), output.transactions},
        {JS(validated), output.validated},
    };

    if (output.marker)
        jv.as_object()[JS(marker)] = boost::json::value_from(*(output.marker));

    if (output.limit)
        jv.as_object()[JS(limit)] = *(output.limit);
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    NFTHistoryHandler::Marker const& marker
)
{
    jv = {
        {JS(ledger), marker.ledger},
        {JS(seq), marker.seq},
    };
}

NFTHistoryHandler::Input
tag_invoke(boost::json::value_to_tag<NFTHistoryHandler::Input>, boost::json::value const& jv)
{
    auto const& jsonObject = jv.as_object();
    auto input = NFTHistoryHandler::Input{};

    input.nftID = boost::json::value_to<std::string>(jsonObject.at(JS(nft_id)));

    if (jsonObject.contains(JS(ledger_index_min)) &&
        util::integralValueAs<int32_t>(jsonObject.at(JS(ledger_index_min))) != -1)
        input.ledgerIndexMin = util::integralValueAs<uint32_t>(jsonObject.at(JS(ledger_index_min)));

    if (jsonObject.contains(JS(ledger_index_max)) &&
        util::integralValueAs<int32_t>(jsonObject.at(JS(ledger_index_max))) != -1)
        input.ledgerIndexMax = util::integralValueAs<uint32_t>(jsonObject.at(JS(ledger_index_max)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jsonObject.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jsonObject.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    if (jsonObject.contains(JS(binary)))
        input.binary = jsonObject.at(JS(binary)).as_bool();

    if (jsonObject.contains(JS(forward)))
        input.forward = jsonObject.at(JS(forward)).as_bool();

    if (jsonObject.contains(JS(limit)))
        input.limit = util::integralValueAs<uint32_t>(jsonObject.at(JS(limit)));

    if (jsonObject.contains(JS(marker))) {
        input.marker = NFTHistoryHandler::Marker{
            .ledger = util::integralValueAs<uint32_t>(
                jsonObject.at(JS(marker)).as_object().at(JS(ledger))
            ),
            .seq =
                util::integralValueAs<uint32_t>(jsonObject.at(JS(marker)).as_object().at(JS(seq)))
        };
    }

    return input;
}

}  // namespace rpc
