#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/HandlerFor.hpp>
#include <rpcspec/RpcSpecView.hpp>
#include <rpcspec/handlers/mpt_holders/Types.hpp>
#include <xrpl/protocol/AccountID.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

/**
 * @brief The mpt_holders command asks the Clio server for holders of a particular
 * MPTokenIssuance.
 *
 * When `accounts` is provided, those accounts are looked up directly by
 * `keylet::mptoken` instead of scanning the holder index. Duplicate accounts are
 * collapsed in first-seen order and non-holders are omitted. This filtered mode is
 * not paginated, so `marker` and `limit` are rejected.
 */
class MPTHoldersHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    static constexpr auto kLimitMin = rpc::spec::handlers::mpt_holders::kLimitMin;
    static constexpr auto kLimitMax = rpc::spec::handlers::mpt_holders::kLimitMax;
    static constexpr auto kLimitDefault = rpc::spec::handlers::mpt_holders::kLimitDefault;
    static constexpr auto kMaxAccounts = 100;

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        boost::json::array mpts;
        uint32_t ledgerIndex;
        std::string mptID;
        bool validated = true;
        uint32_t limit;
        std::optional<std::string> marker;
    };

    /**
     * @brief Input for mpt_holders: the shared rpc-spec fields (`mpt_issuance_id`, `marker`,
     * `limit`, `ledger`), plus Clio's own `accounts` filter, which has no equivalent in the
     * shared spec and is validated/parsed separately in @ref parseInput.
     */
    struct Input : rpc::spec::handlers::mpt_holders::Input {
        std::optional<std::vector<xrpl::AccountID>> accounts;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new MPTHoldersHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    MPTHoldersHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Validate and parse the request, including the `accounts` filter that the shared
     * rpc-spec does not know about.
     *
     * @param jv The raw request JSON
     * @param apiVersion The API version to validate against
     * @return The parsed Input, or a Status describing the validation failure
     */
    [[nodiscard]] static std::expected<Input, Status>
    parseInput(boost::json::value jv, uint32_t apiVersion);

    /**
     * @brief The handler's spec, for warnings/schema-dump purposes.
     *
     * @note This reflects only the shared rpc-spec fields; `accounts` is validated in
     * @ref parseInput and is not represented here.
     *
     * @param apiVersion The API version to select the spec for
     * @return A view over the selected version's spec
     */
    [[nodiscard]] static rpc::spec::RpcSpecView
    spec(uint32_t apiVersion);

    /**
     * @brief Process the MPTHolders command
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
};
}  // namespace rpc
