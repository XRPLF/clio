#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/HandlerFor.hpp>
#include <rpcspec/handlers/transaction_entry/Types.hpp>
#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace rpc {

/**
 * @brief The transaction_entry method retrieves information on a single transaction from a specific
 * ledger version.
 *
 * For more details see: https://xrpl.org/transaction_entry.html
 */
class TransactionEntryHandler
    : public rpc::spec::HandlerFor<rpc::spec::handlers::transaction_entry::Input> {
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
};

}  // namespace rpc
