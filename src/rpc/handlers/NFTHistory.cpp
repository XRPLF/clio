#include "rpc/handlers/NFTHistory.hpp"

#include "data/Types.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/Profiler.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/nft_history/Types.hpp>
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

    auto [minIndex, maxIndex] = *range;  // NOLINT(bugprone-unchecked-optional-access)

    if (input.ledgerIndexMin) {
        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        if (range->maxSequence < input.ledgerIndexMin || range->minSequence > input.ledgerIndexMin)
            return Error{Status{XrpldError::RpcLgrIdxMalformed, "ledgerSeqMinOutOfRange"}};
        // NOLINTEND(bugprone-unchecked-optional-access)

        minIndex = static_cast<uint32_t>(*input.ledgerIndexMin);
    }

    if (input.ledgerIndexMax) {
        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        if (range->maxSequence < input.ledgerIndexMax || range->minSequence > input.ledgerIndexMax)
            return Error{Status{XrpldError::RpcLgrIdxMalformed, "ledgerSeqMaxOutOfRange"}};
        // NOLINTEND(bugprone-unchecked-optional-access)

        maxIndex = static_cast<uint32_t>(*input.ledgerIndexMax);
    }

    if (minIndex > maxIndex)
        return Error{Status{XrpldError::RpcLgrIdxsInvalid}};

    if (not input.ledger.isUnspecified()) {
        // rippled does not have this check
        if (input.ledgerIndexMax || input.ledgerIndexMin) {
            return Error{Status{XrpldError::RpcInvalidParams, "containsLedgerSpecifierAndRange"}};
        }

        auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
            *sharedPtrBackend_,
            ctx.yield,
            input.ledger,
            range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
        );

        if (not expectedLgrInfo.has_value())
            return Error{expectedLgrInfo.error()};

        maxIndex = minIndex = expectedLgrInfo->seq;
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

    auto const limit = input.limit.value_or(kLimitDefault);
    auto const& tokenID = input.nftID;

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
                    obj[JS(close_time_iso)] = xrpl::toStringIso(lgrInfo->closeTime);
                    obj[JS(ledger_hash)] = xrpl::strHex(lgrInfo->hash);
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
    response.nftID = xrpl::to_string(tokenID);
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

}  // namespace rpc

namespace rpc::spec::handlers::nft_history {

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Marker const& marker)
{
    jv = {
        {JS(ledger), marker.ledger},
        {JS(seq), marker.seq},
    };
}

}  // namespace rpc::spec::handlers::nft_history
