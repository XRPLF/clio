#include "rpc/handlers/MPTokenIssuanceHistory.hpp"

#include "data/Types.hpp"
#include "migration/MigratiorStatus.hpp"
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
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

MPTokenIssuanceHistoryHandler::Result
MPTokenIssuanceHistoryHandler::process(
    MPTokenIssuanceHistoryHandler::Input const& input,
    Context const& ctx
) const
{
    // Fail closed unless the backfill is done: partial history must never be served.
    if (not migrated_->load(std::memory_order_relaxed)) {
        auto const statusString = sharedPtrBackend_->fetchMigratorStatus(kMigratorName, ctx.yield);
        if (statusString.has_value() and
            migration::MigratorStatus::fromString(*statusString) ==
                migration::MigratorStatus::Status::Migrated) {
            migrated_->store(true, std::memory_order_relaxed);
        } else {
            return Error{Status{
                RippledError::RpcNotReady,
                "mptoken_issuance_history is unavailable until the MPT transaction-history "
                "backfill "
                "completes on this node. Run: ./clio_server --migrate "
                "MPTTransactionHistoryMigrator "
                "CONFIG"
            }};
        }
    }

    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "MPTokenIssuanceHistory's ledger range must be available");

    auto [minIndex, maxIndex] = *range;  // NOLINT(bugprone-unchecked-optional-access)

    if (input.ledgerIndexMin) {
        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        if (range->maxSequence < input.ledgerIndexMin || range->minSequence > input.ledgerIndexMin)
            return Error{Status{RippledError::RpcLgrIdxMalformed, "ledgerSeqMinOutOfRange"}};
        // NOLINTEND(bugprone-unchecked-optional-access)

        minIndex = *input.ledgerIndexMin;
    }

    if (input.ledgerIndexMax) {
        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        if (range->maxSequence < input.ledgerIndexMax || range->minSequence > input.ledgerIndexMax)
            return Error{Status{RippledError::RpcLgrIdxMalformed, "ledgerSeqMaxOutOfRange"}};
        // NOLINTEND(bugprone-unchecked-optional-access)

        maxIndex = *input.ledgerIndexMax;
    }

    if (minIndex > maxIndex)
        return Error{Status{RippledError::RpcLgrIdxsInvalid}};

    if (input.ledgerHash || input.ledgerIndex) {
        // rippled does not have this check
        if (input.ledgerIndexMax || input.ledgerIndexMin) {
            return Error{Status{RippledError::RpcInvalidParams, "containsLedgerSpecifierAndRange"}};
        }

        auto const expectedLgrInfo = getLedgerHeaderFromHashOrSeq(
            *sharedPtrBackend_,
            ctx.yield,
            input.ledgerHash,
            input.ledgerIndex,
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
    auto const mptIssuanceID = xrpl::uint192{input.mptIssuanceID.c_str()};

    // tx_type is applied post-fetch below, as account_tx does.
    auto const [txnsAndCursor, timeDiff] = util::timed([&]() -> data::TransactionsAndCursor {
        if (input.account) {
            auto const account = accountFromStringStrict(*input.account);
            ASSERT(account.has_value(), "Account must be decodable after spec validation");
            return sharedPtrBackend_->fetchAccountMPTokenIssuanceTransactions(
                mptIssuanceID, *account, limit, input.forward, cursor, ctx.yield
            );
        }
        return sharedPtrBackend_->fetchMPTokenIssuanceTransactions(
            mptIssuanceID, limit, input.forward, cursor, ctx.yield
        );
    });
    LOG(log_.info()) << "db fetch took " << timeDiff
                     << " milliseconds - num blobs = " << txnsAndCursor.txns.size();

    Output response;
    auto const [blobs, retCursor] = txnsAndCursor;

    if (retCursor)
        response.marker = {.ledger = retCursor->ledgerSequence, .seq = retCursor->transactionIndex};

    for (auto const& txnPlusMeta : blobs) {
        // A hash with no matching Transactions row yields a default-constructed record in-position.
        // Skip it before the range check so it neither shortens the page nor disturbs the marker.
        if (txnPlusMeta.transaction.empty() || txnPlusMeta.metadata.empty()) {
            LOG(log_.warn()) << "Skipping index entry with no matching transaction record; "
                                "mpt_issuance_id = "
                             << input.mptIssuanceID;
            continue;
        }

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

        // tx_type needs the expanded form to read TransactionType, even when binary is set
        if (!input.binary || input.transactionTypeInLowercase.has_value()) {
            auto [txn, meta] = toExpandedJson(txnPlusMeta, ctx.apiVersion);

            if (txn.contains(JS(TransactionType)) && input.transactionTypeInLowercase.has_value() &&
                util::toLower(boost::json::value_to<std::string>(txn[JS(TransactionType)])) !=
                    *input.transactionTypeInLowercase)
                continue;

            if (!input.binary) {
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
                obj[JS(validated)] = true;
                response.transactions.push_back(std::move(obj));
                continue;
            }
        }

        // binary is true
        obj = toJsonWithBinaryTx(txnPlusMeta, ctx.apiVersion);
        obj[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
        obj[JS(date)] = txnPlusMeta.date;
        obj[JS(validated)] = true;
        response.transactions.push_back(std::move(obj));
    }

    response.limit = input.limit;
    response.mptIssuanceID = xrpl::to_string(mptIssuanceID);
    response.ledgerIndexMin = minIndex;
    response.ledgerIndexMax = maxIndex;

    return response;
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

    if (output.marker)
        jv.as_object()[JS(marker)] = boost::json::value_from(*(output.marker));

    if (output.limit)
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
