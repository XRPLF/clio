#include "rpc/handlers/MPTokenIssuanceHistory.hpp"

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

#include <atomic>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

namespace {

/**
 * @brief The migrator status that reports a completed backfill.
 *
 * This is a literal to keep migration headers out of RPC; it must match the string form of
 * `migration::MigratorStatus::Status::Migrated`.
 */
constexpr auto kMigratedStatus = "Migrated";

}  // namespace

MPTokenIssuanceHistoryHandler::Result
MPTokenIssuanceHistoryHandler::process(
    MPTokenIssuanceHistoryHandler::Input const& input,
    Context const& ctx
) const
{
    if (auto const available = verifyHistoryAvailable(ctx); not available.has_value())
        return Error{available.error()};

    auto const range = resolveSequenceRange(input, ctx);
    if (not range.has_value())
        return Error{range.error()};

    auto const mptIssuanceID = xrpl::uint192{input.mptIssuanceID.c_str()};

    auto const [page, timeDiff] =
        util::timed([&] { return fetchTransactions(input, ctx, mptIssuanceID, *range); });
    LOG(log_.info()) << "db fetch took " << timeDiff
                     << " milliseconds - num blobs = " << page.txns.size();

    Output response;
    response.mptIssuanceID = xrpl::to_string(mptIssuanceID);
    response.ledgerIndexMin = range->min;
    response.ledgerIndexMax = range->max;
    response.limit = input.limit;

    processTransactionsPage(input, ctx, *range, page, response);

    return response;
}

MaybeError
MPTokenIssuanceHistoryHandler::verifyHistoryAvailable(Context const& ctx) const
{
    if (migrated_->load(std::memory_order_relaxed))
        return {};

    auto const statusString = sharedPtrBackend_->fetchMigratorStatus(kMigratorName, ctx.yield);
    if (statusString.has_value() and *statusString == kMigratedStatus) {
        migrated_->store(true, std::memory_order_relaxed);
        return {};
    }

    // Fail closed: partial history must never be served.
    return Error{Status{
        RippledError::RpcNotReady,
        "mptoken_issuance_history is not available on this server because the required "
        "transaction-history backfill has not completed."
    }};
}

std::expected<MPTokenIssuanceHistoryHandler::SequenceRange, Status>
MPTokenIssuanceHistoryHandler::resolveSequenceRange(Input const& input, Context const& ctx) const
{
    auto const ledgerRange = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(ledgerRange.has_value(), "MPTokenIssuanceHistory's ledger range must be available");

    auto const [dbMinSeq, dbMaxSeq] = *ledgerRange;  // NOLINT(bugprone-unchecked-optional-access)
    auto resolved = SequenceRange{.min = dbMinSeq, .max = dbMaxSeq};

    if (input.ledgerIndexMin.has_value()) {
        if (dbMaxSeq < input.ledgerIndexMin || dbMinSeq > input.ledgerIndexMin)
            return Error{Status{RippledError::RpcLgrIdxMalformed, "ledgerSeqMinOutOfRange"}};

        resolved.min = *input.ledgerIndexMin;
    }

    if (input.ledgerIndexMax.has_value()) {
        if (dbMaxSeq < input.ledgerIndexMax || dbMinSeq > input.ledgerIndexMax)
            return Error{Status{RippledError::RpcLgrIdxMalformed, "ledgerSeqMaxOutOfRange"}};

        resolved.max = *input.ledgerIndexMax;
    }

    if (resolved.min > resolved.max)
        return Error{Status{RippledError::RpcLgrIdxsInvalid}};

    if (input.ledgerHash.has_value() || input.ledgerIndex.has_value()) {
        // rippled does not have this check
        if (input.ledgerIndexMax.has_value() || input.ledgerIndexMin.has_value())
            return Error{Status{RippledError::RpcInvalidParams, "containsLedgerSpecifierAndRange"}};

        auto const expectedLgrInfo = getLedgerHeaderFromHashOrSeq(
            *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, dbMaxSeq
        );

        if (not expectedLgrInfo.has_value())
            return Error{expectedLgrInfo.error()};

        resolved.max = resolved.min = expectedLgrInfo->seq;
    }

    return resolved;
}

data::TransactionsAndCursor
MPTokenIssuanceHistoryHandler::fetchTransactions(
    Input const& input,
    Context const& ctx,
    xrpl::uint192 const& mptIssuanceID,
    SequenceRange range
) const
{
    // Construct the database cursor as {ledgerSequence, transactionIndex}.
    auto const startCursor = [&]() -> data::TransactionsCursor {
        if (input.marker.has_value())
            return {input.marker->ledger, input.marker->seq};

        // Forward iteration starts at the first possible transaction in the lowest ledger.
        if (input.forward)
            return {range.min, 0};

        // Reverse iteration starts after all possible transactions in the highest ledger.
        return {range.max, std::numeric_limits<int32_t>::max()};
    }();

    auto const limit = input.limit.value_or(kLimitDefault);

    // tx_type is applied post-fetch, as account_tx does.
    if (input.account.has_value()) {
        auto const account = accountFromStringStrict(*input.account);
        if (not account.has_value()) {
            ASSERT(false, "Account must be decodable after spec validation");
            std::unreachable();
        }
        return sharedPtrBackend_->fetchAccountMPTokenIssuanceTransactions(
            mptIssuanceID, *account, limit, input.forward, startCursor, ctx.yield
        );
    }

    return sharedPtrBackend_->fetchMPTokenIssuanceTransactions(
        mptIssuanceID, limit, input.forward, startCursor, ctx.yield
    );
}

void
MPTokenIssuanceHistoryHandler::processTransactionsPage(
    Input const& input,
    Context const& ctx,
    SequenceRange range,
    data::TransactionsAndCursor const& page,
    Output& response
) const
{
    if (page.cursor.has_value()) {
        response.marker = {
            .ledger = page.cursor->ledgerSequence, .seq = page.cursor->transactionIndex
        };
    }

    for (auto const& txnPlusMeta : page.txns) {
        // A hash with no matching Transactions row yields a default-constructed record in-position.
        // Skip it before the range check so it neither shortens the page nor disturbs the marker.
        if (txnPlusMeta.transaction.empty() || txnPlusMeta.metadata.empty()) {
            LOG(log_.warn()) << "Skipping index entry with no matching transaction record; "
                                "mpt_issuance_id = "
                             << input.mptIssuanceID;
            continue;
        }

        // Stop once iteration passes the far edge of the requested range.
        if ((txnPlusMeta.ledgerSequence < range.min && !input.forward) ||
            (txnPlusMeta.ledgerSequence > range.max && input.forward)) {
            response.marker = std::nullopt;
            break;
        }
        if (txnPlusMeta.ledgerSequence > range.max && !input.forward) {
            LOG(log_.debug()) << "Skipping over transactions from incomplete ledger";
            continue;
        }

        if (auto obj = transactionToJsonIfTypeMatches(txnPlusMeta, input, ctx); obj.has_value())
            response.transactions.push_back(std::move(*obj));
    }
}

std::optional<boost::json::object>
MPTokenIssuanceHistoryHandler::transactionToJsonIfTypeMatches(
    data::TransactionAndMetadata const& txnPlusMeta,
    Input const& input,
    Context const& ctx
) const
{
    // The type filter needs the expanded form to read TransactionType, even for binary output.
    if (!input.binary || input.transactionTypeInLowercase.has_value()) {
        auto [txn, meta] = toExpandedJson(txnPlusMeta, ctx.apiVersion);

        if (txn.contains(JS(TransactionType)) && input.transactionTypeInLowercase.has_value() &&
            util::toLower(boost::json::value_to<std::string>(txn[JS(TransactionType)])) !=
                *input.transactionTypeInLowercase)
            return std::nullopt;

        if (!input.binary)
            return expandedTransactionToJson(std::move(txn), std::move(meta), txnPlusMeta, ctx);
    }

    auto obj = toJsonWithBinaryTx(txnPlusMeta, ctx.apiVersion);
    obj[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
    obj[JS(date)] = txnPlusMeta.date;
    obj[JS(validated)] = true;

    return obj;
}

boost::json::object
MPTokenIssuanceHistoryHandler::expandedTransactionToJson(
    boost::json::object txn,
    boost::json::object meta,
    data::TransactionAndMetadata const& txnPlusMeta,
    Context const& ctx
) const
{
    auto const txKey = ctx.apiVersion > 1u ? JS(tx_json) : JS(tx);

    boost::json::object obj;
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
        if (auto const lgrInfo =
                sharedPtrBackend_->fetchLedgerBySequence(txnPlusMeta.ledgerSequence, ctx.yield);
            lgrInfo.has_value()) {
            obj[JS(close_time_iso)] = xrpl::toStringIso(lgrInfo->closeTime);
            obj[JS(ledger_hash)] = xrpl::strHex(lgrInfo->hash);
        }
    }

    obj[JS(validated)] = true;

    return obj;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    MPTokenIssuanceHistoryHandler::Output const& output
)
{
    jv = {
        {JS(mpt_issuance_id), output.mptIssuanceID},
        {JS(ledger_index_min), output.ledgerIndexMin},
        {JS(ledger_index_max), output.ledgerIndexMax},
        {JS(transactions), output.transactions},
        {JS(validated), output.validated},
    };

    if (output.marker.has_value())
        jv.as_object()[JS(marker)] = boost::json::value_from(*(output.marker));

    if (output.limit.has_value())
        jv.as_object()[JS(limit)] = *(output.limit);
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    MPTokenIssuanceHistoryHandler::Marker const& marker
)
{
    jv = {
        {JS(ledger), marker.ledger},
        {JS(seq), marker.seq},
    };
}

MPTokenIssuanceHistoryHandler::Input
tag_invoke(
    boost::json::value_to_tag<MPTokenIssuanceHistoryHandler::Input>,
    boost::json::value const& jv
)
{
    auto const& jsonObject = jv.as_object();
    auto input = MPTokenIssuanceHistoryHandler::Input{};

    input.mptIssuanceID = boost::json::value_to<std::string>(jsonObject.at(JS(mpt_issuance_id)));

    if (jsonObject.contains(JS(account)))
        input.account = boost::json::value_to<std::string>(jsonObject.at(JS(account)));

    if (jsonObject.contains("tx_type")) {
        input.transactionTypeInLowercase =
            boost::json::value_to<std::string>(jsonObject.at("tx_type"));
    }

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
        input.marker = MPTokenIssuanceHistoryHandler::Marker{
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
