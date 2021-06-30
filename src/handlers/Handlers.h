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

struct Handler
{
    using Method = std::function<Status(Context&, boost::json::object&)>;

    const char* name;
    std::uint32_t cost;
    Method method;
};

std::pair<Status, std::uint32_t>
buildResponse(Context& ctx, boost::json::object& result);

} // namespace RPC
#endif // RIPPLE_REPORTING_HANDLERS_H
