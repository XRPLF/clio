#include "rpc/common/MetaProcessors.hpp"

#include "rpc/Errors.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/value.hpp>

#include <string_view>

namespace rpc::meta {

[[nodiscard]] MaybeError
Section::verify(boost::json::value& value, std::string_view key) const
{
    if (not value.is_object() or not value.as_object().contains(key))
        return {};  // ignore. field does not exist, let 'required' fail instead

    auto& res = value.as_object().at(key);

    // if it is not a json object, let other validators fail
    if (!res.is_object())
        return {};

    for (auto const& spec : specs_) {
        if (auto const ret = spec.process(res); not ret)
            return Error{ret.error()};
    }

    return {};
}

[[nodiscard]] MaybeError
ValidateArrayAt::verify(boost::json::value& value, std::string_view key) const
{
    if (not value.is_object() or not value.as_object().contains(key))
        return {};  // ignore. field does not exist, let 'required' fail instead

    if (not value.as_object().at(key).is_array())
        return Error{Status{RippledError::RpcInvalidParams}};

    auto& arr = value.as_object().at(key).as_array();
    if (idx_ >= arr.size())
        return Error{Status{RippledError::RpcInvalidParams}};

    auto& res = arr.at(idx_);
    for (auto const& spec : specs_) {
        if (auto const ret = spec.process(res); not ret)
            return Error{ret.error()};
    }

    return {};
}

}  // namespace rpc::meta
