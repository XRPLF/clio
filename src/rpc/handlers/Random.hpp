#pragma once

#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>

#include <string>

namespace rpc {

/**
 * @brief The random command provides a random number to be used as a source of entropy for random
 * number generation by clients.
 *
 * For more details see: https://xrpl.org/random.html
 */
class RandomHandler {
public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string random;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Process the Random command
     *
     * @param ctx The context of the request
     * @return The result of the operation
     */
    static Result
    process(Context const& ctx);

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
