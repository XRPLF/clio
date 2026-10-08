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
    static constexpr auto kLimitBinary = rpc::spec::handlers::ledger_data::kLimitBinary;
    static constexpr auto kLimitJson = rpc::spec::handlers::ledger_data::kLimitJson;

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        uint32_t ledgerIndex{};
        std::string ledgerHash;
        std::optional<boost::json::object> header;
        boost::json::array states;
        std::optional<std::string> marker;
        std::optional<uint32_t> diffMarker;
        std::optional<bool> cacheFull;
        bool validated = true;
    };

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
