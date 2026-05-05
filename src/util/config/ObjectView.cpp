#include "util/config/ObjectView.hpp"

#include "util/Assert.hpp"
#include "util/config/ArrayView.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ValueView.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace util::config {

ObjectView::ObjectView(std::string_view prefix, ClioConfigDefinition const& clioConfig)
    : prefix_{prefix}, clioConfig_{clioConfig}
{
}

ObjectView::ObjectView(
    std::string_view prefix,
    std::size_t arrayIndex,
    ClioConfigDefinition const& clioConfig
)
    : prefix_{prefix}, arrayIndex_{arrayIndex}, clioConfig_{clioConfig}
{
}

bool
ObjectView::containsKey(std::string_view key) const
{
    return clioConfig_.get().contains(getFullKey(key));
}

ValueView
ObjectView::getValueView(std::string_view key) const
{
    auto const fullKey = getFullKey(key);
    if (arrayIndex_.has_value()) {
        return clioConfig_.get().getArray(fullKey).valueAt(*arrayIndex_);
    }
    return clioConfig_.get().getValueView(fullKey);
}

ObjectView
ObjectView::getObject(std::string_view key) const
{
    auto const fullKey = getFullKey(key);
    if (startsWithKey(fullKey) && !arrayIndex_.has_value()) {
        return clioConfig_.get().getObject(fullKey);
    }
    if (startsWithKey(fullKey) && arrayIndex_.has_value()) {
        return ObjectView(fullKey, *arrayIndex_, clioConfig_);
    }
    ASSERT(false, "Key {} does not exist in object", fullKey);
    std::unreachable();
}

ArrayView
ObjectView::getArray(std::string_view key) const
{
    auto fullKey = getFullKey(key);
    if (!fullKey.contains(".[]"))
        fullKey += ".[]";

    ASSERT(
        clioConfig_.get().hasItemsWithPrefix(fullKey), "Key {} does not exist in object", fullKey
    );
    return clioConfig_.get().getArray(fullKey);
}

std::string
ObjectView::getFullKey(std::string_view key) const
{
    return fmt::format("{}.{}", prefix_, key);
}

bool
ObjectView::startsWithKey(std::string_view key) const
{
    return std::ranges::any_of(clioConfig_.get(), [&key](auto const& pair) {
        return pair.first.starts_with(key);
    });
}

}  // namespace util::config
