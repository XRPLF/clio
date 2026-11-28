#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/noripple_check/Types.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

/**
 * @brief Handles the `noripple_check` command
 *
 * The noripple_check command provides a quick way to check the status of the Default Ripple field
 * for an account and the No Ripple flag of its trust lines, compared with the recommended settings.
 *
 * For more details see: https://xrpl.org/noripple_check.html
 */
class NoRippleCheckHandler : public rpc::HandlerFor<rpc::spec::handlers::noripple_check::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    static constexpr auto kLimitMin = 1;        ///< Minimum allowed value of the `limit` parameter
    static constexpr auto kLimitMax = 500;      ///< Maximum allowed value of the `limit` parameter
    static constexpr auto kLimitDefault = 300;  ///< Default value of the `limit` parameter

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string ledgerHash;             ///< Hash of the ledger used
        uint32_t ledgerIndex{};             ///< Sequence of the ledger used
        std::vector<std::string> problems;  ///< Human-readable descriptions of problems found
        // TODO: use better type than json
        /** @brief Transactions fixing the problems, if requested */
        std::optional<boost::json::array> transactions;
        bool validated = true;  ///< Whether the ledger is validated
    };

    /**
     * @brief The result type of the command: either the Output or an error status
     */
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new NoRippleCheckHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    NoRippleCheckHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the NoRippleCheck command
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
