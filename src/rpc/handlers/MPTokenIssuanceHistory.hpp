#pragma once

#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "rpc/common/Types.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/HandlerFor.hpp>
#include <rpcspec/handlers/mptoken_issuance_history/Types.hpp>
#include <xrpl/basics/base_uint.h>

#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

/**
 * @brief The mptoken_issuance_history command returns past transactions associated with the queried
 * MPTokenIssuance, optionally filtered by an affected account and/or transaction type.
 *
 * @note This is a Clio-only method. Requests fail with `notReady` until the issuance-history
 * backfill reports `Migrated`, so partial history is never served.
 */
class MPTokenIssuanceHistoryHandler
    : public rpc::spec::HandlerFor<rpc::spec::handlers::mptoken_issuance_history::Input> {
    util::Logger log_{"RPC"};
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

    /**
     * @brief Whether the issuance-history backfill has completed.
     *
     * The status is monotonic, so the terminal result is cached across handler copies.
     */
    std::shared_ptr<std::atomic_bool> migrated_ = std::make_shared<std::atomic_bool>(false);

public:
    static constexpr auto kLimitMin = rpc::spec::handlers::mptoken_issuance_history::kLimitMin;
    static constexpr auto kLimitMax = rpc::spec::handlers::mptoken_issuance_history::kLimitMax;
    static constexpr auto kLimitDefault =
        rpc::spec::handlers::mptoken_issuance_history::kLimitDefault;

    /**
     * @brief The name used to query the issuance-history migrator's status.
     *
     * This is a literal to keep Cassandra migration headers out of RPC.
     */
    static constexpr char const* kMigratorName = "MPTokenIssuanceHistoryMigrator";

    /**
     * @brief A struct to hold the marker data.
     */
    using Marker = rpc::spec::handlers::mptoken_issuance_history::Marker;

    /**
     * @brief A struct to hold the output data of the command.
     */
    struct Output {
        std::string mptIssuanceID;
        uint32_t ledgerIndexMin{0};
        uint32_t ledgerIndexMax{0};
        std::optional<uint32_t> limit;
        std::optional<Marker> marker;
        /** @todo Use a domain-specific type instead of JSON. */
        boost::json::array transactions;
        /** @todo Send validated through the RPC framework. */
        bool validated = true;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new MPTokenIssuanceHistoryHandler object.
     *
     * @param sharedPtrBackend The backend to use.
     */
    explicit MPTokenIssuanceHistoryHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the MPTokenIssuanceHistory command.
     *
     * @param input The input data for the command.
     * @param ctx The context of the request.
     * @return The result of the operation.
     */
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const;

private:
    /**
     * @brief The inclusive range of ledger sequences a request is restricted to.
     */
    struct SequenceRange {
        uint32_t min;
        uint32_t max;
    };

    /**
     * @brief Check that the transaction-history backfill has completed.
     *
     * The terminal Migrated status is cached, so the backend is only consulted until the backfill
     * reports completion.
     *
     * @param ctx The context of the request.
     * @return An empty result if the history is complete; an error otherwise.
     */
    [[nodiscard]] MaybeError
    verifyHistoryAvailable(Context const& ctx) const;

    /**
     * @brief Resolve the ledger sequence range to search from the request's ledger specifiers.
     *
     * Starts from the server's full ledger range, then narrows it by ledger_index_min /
     * ledger_index_max, or collapses it to a single sequence if ledger_hash / ledger_index is
     * given.
     *
     * @param input The input data for the command.
     * @param ctx The context of the request.
     * @return The resolved range if the specifiers are valid; an error otherwise.
     */
    [[nodiscard]] std::expected<SequenceRange, Status>
    resolveSequenceRange(Input const& input, Context const& ctx) const;

    /**
     * @brief Fetch one page of transactions for the issuance, optionally restricted to an account.
     *
     * When the request has no marker, a forward page starts at the lower bound of @p range and a
     * reverse page starts after the highest possible transaction at its upper bound.
     *
     * @param input The input data for the command.
     * @param ctx The context of the request.
     * @param mptIssuanceID The MPTokenIssuance ID to fetch transactions for.
     * @param range The resolved ledger sequence range.
     * @return The fetched transactions and the cursor to resume from.
     */
    [[nodiscard]] data::TransactionsAndCursor
    fetchTransactions(
        Input const& input,
        Context const& ctx,
        xrpl::uint192 const& mptIssuanceID,
        SequenceRange range
    ) const;

    /**
     * @brief Process a fetched transaction page into the response.
     *
     * Appends transactions that pass the range and type filters. The response marker is taken from
     * the fetched page, then cleared if the page runs past the requested range because there is
     * nothing left to page through.
     *
     * @param input The input data for the command.
     * @param ctx The context of the request.
     * @param range The resolved ledger sequence range.
     * @param page The transactions fetched from the database and the cursor to resume from.
     * @param [out] response The response to append to.
     */
    void
    processTransactionsPage(
        Input const& input,
        Context const& ctx,
        SequenceRange range,
        data::TransactionsAndCursor const& page,
        Output& response
    ) const;

    /**
     * @brief Convert one transaction to its JSON representation, unless its type is filtered out.
     *
     * @param txnPlusMeta The transaction and its metadata.
     * @param input The input data for the command.
     * @param ctx The context of the request.
     * @return The JSON representation, or nullopt if the transaction's type does not match the
     * requested one.
     */
    [[nodiscard]] std::optional<boost::json::object>
    transactionToJsonIfTypeMatches(
        data::TransactionAndMetadata const& txnPlusMeta,
        Input const& input,
        Context const& ctx
    ) const;

    /**
     * @brief Assemble the API-specific JSON of an expanded transaction and its metadata.
     *
     * For API v2 and later, the transaction is enriched with ledger information when the
     * corresponding ledger header is available.
     *
     * @param txn The expanded transaction JSON.
     * @param meta The expanded metadata JSON.
     * @param txnPlusMeta The transaction and its metadata the JSON was expanded from.
     * @param ctx The context of the request.
     * @return The JSON representation of the transaction.
     */
    [[nodiscard]] boost::json::object
    expandedTransactionToJson(
        boost::json::object txn,
        boost::json::object meta,
        data::TransactionAndMetadata const& txnPlusMeta,
        Context const& ctx
    ) const;

    /**
     * @brief Convert the Output to a JSON object.
     *
     * @param [out] jv The JSON object to convert to.
     * @param output The output to convert.
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);
};

}  // namespace rpc

namespace rpc::spec::handlers::mptoken_issuance_history {

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Marker const& marker);

}  // namespace rpc::spec::handlers::mptoken_issuance_history
