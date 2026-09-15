#include "rpc/handlers/AccountTx.hpp"

#include "data/Types.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/filters/TransactionFilter.hpp"
#include "rpc/filters/impl/DelegateTransactionsFilter.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"
#include "util/MPTIssuanceUtils.hpp"
#include "util/Profiler.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/account_tx/Types.hpp>
#include <xrpl/basics/chrono.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

// TODO: this is currently very similar to nft_history but its own copy for time
// being. we should aim to reuse common logic in some way in the future.
AccountTxHandler::Result
AccountTxHandler::process(AccountTxHandler::Input const& input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "AccountTX's ledger range must be available");

    auto [minIndex, maxIndex] = *range;  // NOLINT(bugprone-unchecked-optional-access)

    if (input.ledgerIndexMin) {
        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        if (ctx.apiVersion > 1u &&
            (input.ledgerIndexMin > range->maxSequence ||
             input.ledgerIndexMin < range->minSequence)) {
            return Error{Status{RippledError::RpcLgrIdxMalformed, "ledgerSeqMinOutOfRange"}};
        }
        // NOLINTEND(bugprone-unchecked-optional-access)

        if (static_cast<std::uint32_t>(*input.ledgerIndexMin) > minIndex)
            minIndex = *input.ledgerIndexMin;
    }

    if (input.ledgerIndexMax) {
        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        if (ctx.apiVersion > 1u &&
            (input.ledgerIndexMax > range->maxSequence ||
             input.ledgerIndexMax < range->minSequence)) {
            return Error{Status{RippledError::RpcLgrIdxMalformed, "ledgerSeqMaxOutOfRange"}};
        }
        // NOLINTEND(bugprone-unchecked-optional-access)

        if (static_cast<std::uint32_t>(*input.ledgerIndexMax) < maxIndex)
            maxIndex = *input.ledgerIndexMax;
    }

    if (minIndex > maxIndex) {
        if (ctx.apiVersion == 1u)
            return Error{Status{RippledError::RpcLgrIdxsInvalid}};

        return Error{Status{RippledError::RpcInvalidLgrRange}};
    }

    if (not input.ledger.isUnspecified()) {
        if (ctx.apiVersion > 1u && (input.ledgerIndexMax || input.ledgerIndexMin)) {
            return Error{Status{RippledError::RpcInvalidParams, "containsLedgerSpecifierAndRange"}};
        }

        if (!input.ledgerIndexMax && !input.ledgerIndexMin) {
            // mimic rippled, when both range and index specified, respect the range.
            // take ledger from ledgerHash or ledgerIndex only when range is not specified
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
    }

    std::optional<data::TransactionsCursor> cursor;

    // if marker exists
    if (input.marker) {
        cursor = {input.marker->ledger, input.marker->seq};
    } else {
        // if forward, start at minIndex - 1, because the SQL query is exclusive, we need to include
        // the 0 transaction index of minIndex
        if (input.forward) {
            cursor = {minIndex - 1, std::numeric_limits<int32_t>::max()};
        } else {
            cursor = {maxIndex, std::numeric_limits<int32_t>::max()};
        }
    }

    std::optional<rpc::DelegateTransactionFilter> txFilter;
    if (input.delegateFilter) {
        txFilter.emplace(*input.delegateFilter, input.account);
    }

    auto const limit = input.limit.value_or(kLimitDefault);
    auto const [txnsAndCursor, timeDiff] = util::timed([&]() {
        return sharedPtrBackend_->fetchAccountTransactions(
            input.account, limit, input.forward, cursor, ctx.yield
        );
    });

    LOG(log_.info()) << "db fetch took " << timeDiff
                     << " milliseconds - num blobs = " << txnsAndCursor.txns.size();

    auto const [blobs, retCursor] = txnsAndCursor;
    Output response;

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

        std::optional<rpc::TransactionFilter::CheckResult> filterResult;
        if (txFilter) {
            filterResult = txFilter->check(txnPlusMeta);
            if (not filterResult.has_value())
                continue;
        }

        boost::json::object obj;

        // Skip all Txns where the specified filter mpt_id doesn't match the query
        if (input.mptIssuanceId) {
            auto const [sttx, txMeta] =
                deserializeTxPlusMeta(txnPlusMeta, txnPlusMeta.ledgerSequence);
            if (!util::referencesMptIssuance(*txMeta, *sttx, *input.mptIssuanceId))
                continue;
        }

        // if binary is false or transactionType is specified, we need to expand the transaction
        if (!input.binary || input.transactionTypeInLowercase.has_value()) {
            auto [txn, meta] = toExpandedJson(txnPlusMeta, ctx.apiVersion, NFTokenjson::ENABLE);

            if (txn.contains(JS(TransactionType)) && input.transactionTypeInLowercase.has_value() &&
                util::toLower(boost::json::value_to<std::string>(txn[JS(TransactionType)])) !=
                    *input.transactionTypeInLowercase)
                continue;

            if (!input.binary) {
                auto const txKey = ctx.apiVersion < 2u ? JS(tx) : JS(tx_json);
                obj[JS(meta)] = std::move(meta);
                obj[txKey] = std::move(txn);

                // Put CTID into tx or tx_json
                if (obj[JS(meta)].as_object().contains("TransactionIndex")) {
                    auto networkID = 0u;
                    if (auto const& etlState = etl_->getETLState(); etlState.has_value())
                        networkID = etlState->networkID;

                    auto const txnIdx = util::integralValueAs<uint16_t>(
                        obj[JS(meta)].as_object().at("TransactionIndex")
                    );
                    if (auto const& ctid =
                            rpc::encodeCTID(txnPlusMeta.ledgerSequence, txnIdx, networkID);
                        ctid)
                        obj[txKey].as_object()[JS(ctid)] = *ctid;
                }

                obj[txKey].as_object()[JS(date)] = txnPlusMeta.date;
                obj[txKey].as_object()[JS(ledger_index)] = txnPlusMeta.ledgerSequence;

                if (ctx.apiVersion < 2u) {
                    obj[txKey].as_object()[JS(inLedger)] = txnPlusMeta.ledgerSequence;
                } else {
                    obj[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
                    if (obj[txKey].as_object().contains(JS(hash))) {
                        obj[JS(hash)] = obj[txKey].as_object()[JS(hash)];
                        obj[txKey].as_object().erase(JS(hash));
                    }
                    if (auto const ledgerHeader = sharedPtrBackend_->fetchLedgerBySequence(
                            txnPlusMeta.ledgerSequence, ctx.yield
                        );
                        ledgerHeader) {
                        obj[JS(ledger_hash)] = xrpl::strHex(ledgerHeader->hash);
                        obj[JS(close_time_iso)] = xrpl::toStringIso(ledgerHeader->closeTime);
                    }
                }

                if (filterResult) {
                    if (filterResult->role == rpc::DelegateFilter::Role::Authorizer) {
                        obj[JS(authorizer)] = xrpl::to_string(filterResult->account);
                    } else {
                        obj[JS(actor)] = xrpl::to_string(filterResult->account);
                    }
                }

                obj[JS(validated)] = true;
                response.transactions.push_back(std::move(obj));
                continue;
            }
        }
        // binary is true
        obj = toJsonWithBinaryTx(txnPlusMeta, ctx.apiVersion);
        obj[JS(validated)] = true;
        obj[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
        response.transactions.push_back(std::move(obj));
    }

    response.limit = input.limit;
    response.account = xrpl::to_string(input.account);
    response.ledgerIndexMin = minIndex;
    response.ledgerIndexMax = maxIndex;

    return response;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountTxHandler::Output const& output
)
{
    jv = {
        {JS(account), output.account},
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

namespace rpc::spec::handlers::account_tx {

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Marker const& marker)
{
    jv = {
        {JS(ledger), marker.ledger},
        {JS(seq), marker.seq},
    };
}

}  // namespace rpc::spec::handlers::account_tx
