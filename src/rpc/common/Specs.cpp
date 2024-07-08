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

#include "rpc/common/Specs.hpp"

#include "rpc/Errors.hpp"
#include "rpc/common/Checkers.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/array.hpp>
#include <boost/json/value.hpp>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rpc {

[[nodiscard]] MaybeError
FieldSpec::process(boost::json::value& value) const
{
    return processor_(value);
}

[[nodiscard]] check::Warnings
FieldSpec::check(boost::json::value const& value) const
{
    return checker_(value);
}

[[nodiscard]] MaybeError
RpcSpec::process(boost::json::value& value) const
{
    for (auto const& field : fields_) {
        if (auto ret = field.process(value); not ret)
            return Error{ret.error()};
    }

    return {};
}

[[nodiscard]] boost::json::array
RpcSpec::check(boost::json::value const& value) const
{
    std::unordered_map<WarningCode, std::vector<std::string>> warnings;
    for (auto const& field : fields_) {
        auto fieldWarnings = field.check(value);
        for (auto& fw : fieldWarnings) {
            warnings[fw.warningCode].push_back(std::move(fw.extraMessage));
        }
    }

    boost::json::array result;
    for (auto const& [code, messages] : warnings) {
        auto warningObject = makeWarning(code);
        auto& warningMessage = warningObject["message"].as_string();
        for (auto const& message : messages) {
            warningMessage.append(" ").append(message);
        }
        result.push_back(std::move(warningObject));
    }
    return result;
}

}  // namespace rpc
