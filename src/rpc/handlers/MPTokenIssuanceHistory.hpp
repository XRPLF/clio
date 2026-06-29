#pragma once

#include "data/BackendInterface.hpp"
#include "migration/MigrationInspectorInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/MetaProcessors.hpp"
#include "rpc/common/Modifiers.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"
#include "util/TxUtils.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/jss.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

/**
 * @brief The mptoken_issuance_history command returns past transactions associated with the queried
 * MPTokenIssuance, optionally filtered by an affected account and/or transaction type.
 *
 * This is a Clio-only method (sibling of nft_history). Because complete history requires the MPT
 * transaction-history backfill, the handler is gated on that migrator's status: until the node
 * reports `Migrated`, every request returns a `notReady` error rather than partial history.
 */
class MPTokenIssuanceHistoryHandler {
    util::Logger log_{"RPC"};
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<migration::MigrationInspectorInterface const> migrationInspector_;
    // Shared across copies of the handler so the terminal `Migrated` result is cached process-wide.
    // Status is monotonic (NotMigrated -> Migrated), so once set the gate check is skipped
    // entirely.
    std::shared_ptr<std::atomic_bool> migrated_ = std::make_shared<std::atomic_bool>(false);

public:
    static constexpr auto kLimitMin = 1;
    static constexpr auto kLimitMax = 100;
    static constexpr auto kLimitDefault = 50;

    // Must match migration::cassandra::MPTTransactionHistoryMigrator::kName. Kept as a literal to
    // avoid pulling the Cassandra migration headers into the RPC layer.
    static constexpr char const* kMigratorName = "MPTTransactionHistoryMigrator";

    /**
     * @brief A struct to hold the marker data
     */
    struct Marker {
        uint32_t ledger;
        uint32_t seq;
    };

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string mptIssuanceID;
        uint32_t ledgerIndexMin{0};
        uint32_t ledgerIndexMax{0};
        std::optional<uint32_t> limit;
        std::optional<Marker> marker;
        // TODO: use a better type than json
        boost::json::array transactions;
        // validated should be sent via framework
        bool validated = true;
    };

    /**
     * @brief A struct to hold the input data for the command
     */
    struct Input {
        std::string mptIssuanceID;
        std::optional<std::string> account;
        std::optional<std::string> transactionTypeInLowercase;
        // You must use at least one of the following fields in your request:
        // ledger_index, ledger_hash, ledger_index_min, or ledger_index_max.
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        std::optional<int32_t> ledgerIndexMin;
        std::optional<int32_t> ledgerIndexMax;
        bool binary = false;
        bool forward = false;
        std::optional<uint32_t> limit;
        std::optional<Marker> marker;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new MPTokenIssuanceHistoryHandler object
     *
     * @param sharedPtrBackend The backend to use
     * @param migrationInspector The migration inspector used to gate on backfill completion
     */
    MPTokenIssuanceHistoryHandler(
        std::shared_ptr<BackendInterface> sharedPtrBackend,
        std::shared_ptr<migration::MigrationInspectorInterface const> migrationInspector
    )
        : sharedPtrBackend_(std::move(sharedPtrBackend))
        , migrationInspector_(std::move(migrationInspector))
    {
    }

    /**
     * @brief Returns the API specification for the command
     *
     * @param apiVersion The api version to return the spec for
     * @return The spec for the given apiVersion
     */
    static RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion)
    {
        auto const& typesKeysInLowercase = util::getTxTypesInLowercase();
        static auto const kRpcSpec = RpcSpec{
            {JS(mpt_issuance_id),
             validation::Required{},
             validation::CustomValidators::uint192HexStringValidator},
            {JS(account), validation::CustomValidators::accountValidator},
            {
                "tx_type",
                validation::Type<std::string>{},
                modifiers::ToLower{},
                validation::OneOf<std::string>(
                    typesKeysInLowercase.cbegin(), typesKeysInLowercase.cend()
                ),
            },
            {JS(ledger_hash), validation::CustomValidators::uint256HexStringValidator},
            {JS(ledger_index), validation::CustomValidators::ledgerIndexValidator},
            {JS(ledger_index_min), validation::Type<int32_t>{}},
            {JS(ledger_index_max), validation::Type<int32_t>{}},
            {JS(binary), validation::Type<bool>{}},
            {JS(forward), validation::Type<bool>{}},
            {JS(limit),
             validation::Type<uint32_t>{},
             validation::Min(1u),
             modifiers::Clamp<int32_t>{kLimitMin, kLimitMax}},
            {JS(marker),
             meta::WithCustomError{
                 validation::Type<boost::json::object>{},
                 Status{RippledError::RpcInvalidParams, "invalidMarker"}
             },
             meta::Section{
                 {JS(ledger), validation::Required{}, validation::Type<uint32_t>{}},
                 {JS(seq), validation::Required{}, validation::Type<uint32_t>{}},
             }},
        };

        return kRpcSpec;
    }

    /**
     * @brief Process the MPTokenIssuanceHistory command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const;

private:
    /**
     * @brief Convert the Output to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param output The output to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);

    /**
     * @brief Convert a JSON object to Input type
     *
     * @param jv The JSON object to convert
     * @return Input parsed from the JSON object
     */
    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);

    /**
     * @brief Convert the Marker to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param marker The marker to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Marker const& marker);
};

}  // namespace rpc
