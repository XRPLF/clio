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
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace rpc {

/**
 * @brief The transaction_entry method retrieves information on a single transaction from a specific
 * ledger version.
 *
 * For more details see: https://xrpl.org/transaction_entry.html
 */
class TransactionEntryHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::optional<xrpl::LedgerHeader> ledgerHeader;
        // TODO: use a better type for this
        boost::json::object metadata;
        boost::json::object tx;
        // validated should be sent via framework
        bool validated = true;
        uint32_t apiVersion;
    };

    /**
     * @brief A struct to hold the input data for the command
     */
    struct Input {
        std::string txHash;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new Transaction Entry Handler object
     *
     * @param sharedPtrBackend The backend to use
     */
    TransactionEntryHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
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
        static auto const kRpcSpec = RpcSpec{
            {JS(tx_hash),
             meta::WithCustomError{
                 validation::Required{}, Status(ClioError::RpcFieldNotFoundTransaction)
             },
             validation::CustomValidators::uint256HexStringValidator},
            {JS(ledger_hash), validation::CustomValidators::uint256HexStringValidator},
            {JS(ledger_index), validation::CustomValidators::ledgerIndexValidator},
        };

        return kRpcSpec;
    }

    /**
     * @brief Process the transaction_entry command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the command
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
