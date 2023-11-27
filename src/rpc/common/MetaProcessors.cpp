//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "rpc/common/MetaProcessors.h"

#include "rpc/Errors.h"
#include "rpc/common/Types.h"

#include <boost/json/value.hpp>
#include <ripple/protocol/ErrorCodes.h>

#include <string_view>

namespace rpc::meta {

[[nodiscard]] MaybeError
Section::verify(boost::json::value& value, std::string_view key) const
{
    if (not value.is_object() or not value.as_object().contains(key.data()))
        return {};  // ignore. field does not exist, let 'required' fail instead

    auto& res = value.as_object().at(key.data());

    // if it is not a json object, let other validators fail
    if (!res.is_object())
        return {};

    for (auto const& spec : specs) {
        if (auto const ret = spec.process(res); not ret)
            return Error{ret.error()};
    }

    return {};
}

[[nodiscard]] MaybeError
ValidateArrayAt::verify(boost::json::value& value, std::string_view key) const
{
    if (not value.is_object() or not value.as_object().contains(key.data()))
        return {};  // ignore. field does not exist, let 'required' fail instead

    if (not value.as_object().at(key.data()).is_array())
        return Error{Status{RippledError::rpcINVALID_PARAMS}};

    auto& arr = value.as_object().at(key.data()).as_array();
    if (idx_ >= arr.size())
        return Error{Status{RippledError::rpcINVALID_PARAMS}};

    auto& res = arr.at(idx_);
    for (auto const& spec : specs_) {
        if (auto const ret = spec.process(res); not ret)
            return Error{ret.error()};
    }

    return {};
}

}  // namespace rpc::meta
