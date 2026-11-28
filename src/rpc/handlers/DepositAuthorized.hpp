#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/deposit_authorized/Types.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/STArray.h>

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

/**
 * @brief Handles the `deposit_authorized` command
 *
 * The deposit_authorized command indicates whether one account is authorized to send payments
 * directly to another. See Deposit Authorization for information on how to require authorization to
 * deliver money to your account.
 *
 * For more details see: https://xrpl.org/deposit_authorized.html
 */
class DepositAuthorizedHandler
    : public rpc::HandlerFor<rpc::spec::handlers::deposit_authorized::Input> {
    // dependencies
    std::shared_ptr<BackendInterface> const sharedPtrBackend_;

public:
    // Note: `ledger_current_index` is omitted because it only makes sense for rippled
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        bool depositAuthorized = true;   ///< Whether the source may pay the destination
        std::string sourceAccount;       ///< Address of the sending account
        std::string destinationAccount;  ///< Address of the receiving account
        std::string ledgerHash;          ///< Hash of the ledger used
        uint32_t ledgerIndex{};          ///< Sequence of the ledger used
        /** @brief Credentials from the request, if any */
        std::optional<std::vector<xrpl::uint256>> credentials;

        // validated should be sent via framework
        bool validated = true;  ///< Whether the ledger is validated
    };

    /**
     * @brief The result type of the command: either the Output or an error status
     */
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new DepositAuthorizedHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    DepositAuthorizedHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the DepositAuthorized command
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
