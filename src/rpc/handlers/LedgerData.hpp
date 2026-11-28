#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/ledger_data/Types.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerFormats.h>

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

/**
 * @brief The ledger_data method retrieves contents of the specified ledger. You can iterate through
 * several calls to retrieve the entire contents of a single ledger version.
 *
 * For more details see: https://xrpl.org/ledger_data.html
 */
class LedgerDataHandler : public rpc::HandlerFor<rpc::spec::handlers::ledger_data::Input> {
    // dependencies
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    util::Logger log_{"RPC"};

public:
    // constants
    /**
     * @brief Maximum number of objects returned per page in binary mode
     */
    static constexpr auto kLimitBinary = rpc::spec::handlers::ledger_data::kLimitBinary;
    /**
     * @brief Maximum number of objects returned per page in JSON mode
     */
    static constexpr auto kLimitJson = rpc::spec::handlers::ledger_data::kLimitJson;

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        uint32_t ledgerIndex{};  ///< Sequence of the ledger used
        std::string ledgerHash;  ///< Hash of the ledger used
        /**
         * @brief Ledger header, only included in the first page
         */
        std::optional<boost::json::object> header;
        boost::json::array states;          ///< Ledger objects in this page
        std::optional<std::string> marker;  ///< Hex object key to resume from, if more pages remain
        /**
         * @brief Ledger sequence to resume from when paging out of order
         */
        std::optional<uint32_t> diffMarker;
        std::optional<bool> cacheFull;  ///< Whether the ledger cache is fully loaded
        bool validated = true;          ///< Whether the ledger is validated
    };

    /**
     * @brief The result type of the command: either the Output or an error status
     */
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new LedgerDataHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    LedgerDataHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the LedgerData command
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
