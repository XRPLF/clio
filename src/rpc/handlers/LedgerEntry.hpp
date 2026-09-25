#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/ledger_entry/Types.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

/**
 * @brief The ledger_entry method returns a single ledger object from the XRP Ledger in its raw
 * format.
 *
 * For more details see: https://xrpl.org/ledger_entry.html
 */
class LedgerEntryHandler : public rpc::HandlerFor<rpc::spec::handlers::ledger_entry::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string index;
        uint32_t ledgerIndex;
        std::string ledgerHash;
        std::optional<boost::json::object> node;
        std::optional<std::string> nodeBinary;
        std::optional<uint32_t> deletedLedgerIndex;
        bool validated = true;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new LedgerEntryHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    LedgerEntryHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the LedgerEntry command
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
