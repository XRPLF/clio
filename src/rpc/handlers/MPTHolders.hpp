#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/Modifiers.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"
#include "util/AccountUtils.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/jss.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
    static constexpr auto kLimitMin = 1;
    static constexpr auto kLimitMax = 100;
    static constexpr auto kLimitDefault = 50;
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
     * @brief A struct to hold the input data for the command
     */
    struct Input {
        std::string mptID;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        std::optional<std::string> marker;
        std::optional<uint32_t> limit;
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
     * @brief Returns the API specification for the command
     *
     * @param apiVersion The api version to return the spec for
     * @return The spec for the given apiVersion
     */
    static RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion)
    {
        // Optional filter: when present, return only these accounts' MPToken state for the
        // issuance.
        static auto const kAccountsValidator = validation::CustomValidator{
            [](boost::json::value const& value, std::string_view key) -> MaybeError {
                if (!value.is_array()) {
                    return Error{
                        Status{RippledError::RpcInvalidParams, std::string{key} + "NotArray"}
                    };
                }

                auto const& accounts = value.as_array();
                if (accounts.empty() || accounts.size() > static_cast<std::size_t>(kMaxAccounts)) {
                    return Error{
                        Status{RippledError::RpcInvalidParams, std::string{key} + "Malformed"}
                    };
                }

                for (auto const& account : accounts) {
                    if (!account.is_string()) {
                        return Error{Status{
                            RippledError::RpcInvalidParams, std::string{key} + "'sItemNotString"
                        }};
                    }

                    if (!util::parseBase58Wrapper<xrpl::AccountID>(
                            boost::json::value_to<std::string>(account)
                        )) {
                        return Error{Status{
                            RippledError::RpcInvalidParams, std::string{key} + "'sItemMalformed"
                        }};
                    }
                }

                return MaybeError{};
            }
        };

        static auto const kRpcSpec = RpcSpec{
            {JS(mpt_issuance_id),
             validation::Required{},
             validation::CustomValidators::uint192HexStringValidator},
            {JS(ledger_hash), validation::CustomValidators::uint256HexStringValidator},
            {JS(ledger_index), validation::CustomValidators::ledgerIndexValidator},
            {JS(limit),
             validation::Type<uint32_t>{},
             validation::Min(1u),
             modifiers::Clamp<int32_t>{kLimitMin, kLimitMax}},
            {JS(marker), validation::CustomValidators::uint160HexStringValidator},
            {JS(accounts), kAccountsValidator},
        };

        return kRpcSpec;
    }

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

    /**
     * @brief Convert a JSON object to Input type
     *
     * @param jv The JSON object to convert
     * @return Input parsed from the JSON object
     */
    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);
};
}  // namespace rpc
