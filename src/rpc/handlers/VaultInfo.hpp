#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/MetaProcessors.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace rpc {

/**
 * @brief The vault_info command retrieves information about a vault, currency, shares etc.
 */
class VaultInfoHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief Construct a new VaultInfo object
     *
     * @param sharedPtrBackend The backend to use
     */
    VaultInfoHandler(std::shared_ptr<BackendInterface> sharedPtrBackend);

    /**
     * @brief A struct to hold the input data for the command
     */
    struct Input {
        std::optional<std::string> vaultID;
        std::optional<std::string> owner;
        std::optional<uint32_t> tnxSequence;
        std::optional<uint32_t> ledgerIndex;
    };

    /**
     * @brief A struct to hold the output data for the command
     */
    struct Output {
        boost::json::value vault;
        uint32_t ledgerIndex{};
        bool validated = true;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Returns the API specification for the command
     *
     * @param apiVersion The api version to return the spec for
     * @return The spec for the given apiVersion
     */
    static RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion)
    {
        static auto const kRPC_SPEC = RpcSpec{
            {JS(vault_id),
             meta::WithCustomError{
                 validation::CustomValidators::uint256HexStringValidator,
                 Status(ClioError::RpcMalformedRequest)
             }},
            {JS(owner),
             meta::WithCustomError{
                 validation::CustomValidators::accountBase58Validator,
                 Status(ClioError::RpcMalformedRequest, "OwnerNotHexString")
             }},
            {JS(seq),
             meta::WithCustomError{
                 validation::Type<uint32_t>{}, Status(ClioError::RpcMalformedRequest)
             }},
            {JS(ledger_index), validation::CustomValidators::ledgerIndexValidator},
        };

        return kRPC_SPEC;
    }

    /**
     * @brief Process the VaultInfo command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    Result
    process(Input const& input, Context const& ctx) const;

private:
    /**
     * @brief Convert the Output to a JSON object
     *
     * @param jv The JSON object to convert to
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
