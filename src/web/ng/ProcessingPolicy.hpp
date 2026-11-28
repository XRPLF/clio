#pragma once

namespace web::ng {

/**
 * @brief Requests processing policy.
 */
enum class ProcessingPolicy {
    Sequential,  ///< Requests from a connection are processed one at a time, in order
    Parallel     ///< Requests from a connection may be processed concurrently
};

}  // namespace web::ng
