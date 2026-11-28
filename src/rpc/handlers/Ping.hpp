#pragma once

#include "rpc/common/Types.hpp"

namespace rpc {

/**
 * @brief The ping command returns an acknowledgement, so that clients can test the connection
 * status and latency.
 *
 * For more details see https://xrpl.org/ping.html
 */
class PingHandler {
public:
    /**
     * @brief The command has no output data
     */
    using Output = VoidOutput;
    /**
     * @brief The result type of the command: either the Output or an error status
     */
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Process the Ping command
     *
     * @param ctx The context of the request
     * @return The result of the operation
     */
    static Result
    process([[maybe_unused]] Context const& ctx)
    {
        return Output{};
    }
};

}  // namespace rpc
