#pragma once

#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>

#include <memory>

namespace rpc {

/**
 * @brief The ledger_range command returns the index number of the earliest and latest ledgers that
 * the server has.
 *
 * Not documented in the official rippled API docs.
 */
class LedgerRangeHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        data::LedgerRange range;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new LedgerRangeHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    LedgerRangeHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the LedgerRange command
     *
     * @param ctx The context of the request
     * @return The result of the operation
     */
    [[nodiscard]] Result
    process(Context const& ctx) const;

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
