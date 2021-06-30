#include <handlers/Context.h>
#include <handlers/Status.h>

#include <unordered_map>
#include <functional>
#include <iostream>

#ifndef RIPPLE_REPORTING_HANDLERS_H
#define RIPPLE_REPORTING_HANDLERS_H

namespace RPC 
{

// Maximum and minimum supported API versions
// Defaults to APIVersionIfUnspecified
constexpr unsigned int APIVersionIfUnspecified = 1;
constexpr unsigned int ApiMinimumSupportedVersion = 1;
constexpr unsigned int ApiMaximumSupportedVersion = 1;
constexpr unsigned int APINumberVersionSupported =
    ApiMaximumSupportedVersion - ApiMinimumSupportedVersion + 1;

static_assert(ApiMinimumSupportedVersion >= APIVersionIfUnspecified);
static_assert(ApiMaximumSupportedVersion >= ApiMinimumSupportedVersion);

Result
buildResponse(Context const& ctx);

} // namespace RPC
#endif // RIPPLE_REPORTING_HANDLERS_H
