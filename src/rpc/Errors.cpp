#include <rpc/Errors.h>

#include <algorithm>

using namespace std;

namespace {
template <class... Ts>
struct overloadSet : Ts...
{
    using Ts::operator()...;
};

// explicit deduction guide (not needed as of C++20, but clang be clang)
template <class... Ts>
overloadSet(Ts...) -> overloadSet<Ts...>;
}  // namespace

namespace RPC {

WarningInfo const&
getWarningInfo(WarningCode code)
{
    constexpr static WarningInfo infos[]{
        {warnUNKNOWN, "Unknown warning"},
        {warnRPC_CLIO,
         "This is a clio server. clio only serves validated data. If you "
         "want to talk to rippled, include 'ledger_index':'current' in your "
         "request"},
        {warnRPC_OUTDATED, "This server may be out of date"},
        {warnRPC_RATE_LIMIT, "You are about to be rate limited"}};

    auto matchByCode = [code](auto const& info) { return info.code == code; };
    if (auto it = find_if(begin(infos), end(infos), matchByCode);
        it != end(infos))
        return *it;

    throw(out_of_range("Invalid WarningCode"));
}

boost::json::object
makeWarning(WarningCode code)
{
    boost::json::object json;
    auto const& info = getWarningInfo(code);
    json["id"] = code;
    json["message"] = static_cast<string>(info.message);
    return json;
}

ClioErrorInfo const&
getErrorInfo(ClioError code)
{
    constexpr static ClioErrorInfo infos[]{
        {ClioError::rpcMALFORMED_CURRENCY,
         "malformedCurrency",
         "Malformed currency."},
        {ClioError::rpcMALFORMED_REQUEST,
         "malformedRequest",
         "Malformed request."},
        {ClioError::rpcMALFORMED_OWNER, "malformedOwner", "Malformed owner."},
    };

    auto matchByCode = [code](auto const& info) { return info.code == code; };
    if (auto it = find_if(begin(infos), end(infos), matchByCode);
        it != end(infos))
        return *it;

    throw(out_of_range("Invalid error code"));
}

boost::json::object
makeError(
    RippledError err,
    optional<string_view> customError,
    optional<string_view> customMessage)
{
    boost::json::object json;
    auto const& info = ripple::RPC::get_error_info(err);

    json["error"] = customError.value_or(info.token.c_str()).data();
    json["error_code"] = static_cast<uint32_t>(err);
    json["error_message"] = customMessage.value_or(info.message.c_str()).data();
    json["status"] = "error";
    json["type"] = "response";
    return json;
}

boost::json::object
makeError(
    ClioError err,
    optional<string_view> customError,
    optional<string_view> customMessage)
{
    boost::json::object json;
    auto const& info = getErrorInfo(err);

    json["error"] = customError.value_or(info.error).data();
    json["error_code"] = static_cast<uint32_t>(info.code);
    json["error_message"] = customMessage.value_or(info.message).data();
    json["status"] = "error";
    json["type"] = "response";
    return json;
}

boost::json::object
makeError(Status const& status)
{
    auto wrapOptional = [](string_view const& str) {
        return str.empty() ? nullopt : make_optional(str);
    };

    auto res = visit(
        overloadSet{
            [&status, &wrapOptional](RippledError err) {
                if (err == ripple::rpcUNKNOWN)
                {
                    return boost::json::object{
                        {"error", status.message},
                        {"type", "response"},
                        {"status", "error"}};
                }

                return makeError(
                    err,
                    wrapOptional(status.error),
                    wrapOptional(status.message));
            },
            [&status, &wrapOptional](ClioError err) {
                return makeError(
                    err,
                    wrapOptional(status.error),
                    wrapOptional(status.message));
            },
        },
        status.code);
    if (status.extraInfo)
    {
        for (auto& [key, value] : status.extraInfo.value())
        {
            res[key] = value;
        }
    }
    return res;
}

}  // namespace RPC
