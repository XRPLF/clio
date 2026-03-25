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
