#include <rpc/Context.h>
#include <rpc/Status.h>

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

// Indicates the level of administrative permission to grant.
// NOTE, this does not currently affect RPCs in clio, and will be implimented later.
enum class Role { GUEST, USER, IDENTIFIED, ADMIN, PROXY, FORBID };


struct Handler
{
    using Method = std::function<Status(Context&, boost::json::object&)>;

    const char* name;
    Method method;
    Role role;
};

Status
buildResponse(Context& ctx, boost::json::object& result);

} // namespace RPC
#endif // RIPPLE_REPORTING_HANDLERS_H
