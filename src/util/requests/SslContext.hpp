#pragma once

#include <expected>
#include <string>

namespace util::requests {

/**
 * @brief Create and cache the shared client SSL context.
 *
 * Intended to be called once during application startup so that an unrecoverable SSL
 * misconfiguration (e.g. a missing system root certificate bundle) is reported immediately instead
 * of causing every outgoing request to fail later. The context (including the potentially expensive
 * parse of the root certificate bundle) is created once and reused for the lifetime of the process.
 *
 * @return An error message if the context could not be created
 */
std::expected<void, std::string>
initClientSslContext();

}  // namespace util::requests
