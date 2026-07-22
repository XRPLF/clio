#include "util/config/Array.hpp"

#include "util/Assert.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Error.hpp"
#include "util/config/Types.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace util::config {

Array::Array(ConfigValue arg) : itemPattern_{std::move(arg)}
{
}

std::string_view
Array::prefix(std::string_view key)
{
    static constexpr std::string_view kArraySuffix = ".[]";
    ASSERT(key.contains(kArraySuffix), "Provided key is not an array key: {}", key);

    return key.substr(0, key.rfind(kArraySuffix) + kArraySuffix.size());
}

std::optional<Error>
Array::addValue(Value value, std::optional<std::string_view> key)
{
    auto newItem = itemPattern_;

    if (auto const maybeError = newItem.setValue(value, key); maybeError.has_value())
        return maybeError;
    elements_.emplace_back(std::move(newItem));
    return std::nullopt;
}

std::optional<Error>
Array::addNull(std::optional<std::string_view> key)
{
    if (not itemPattern_.isOptional() and not itemPattern_.hasValue()) {
        return Error{
            key.value_or("Unknown_key"),
            "value for the array (or object field inside array) is required"
        };
    }

    elements_.push_back(itemPattern_);
    return std::nullopt;
}

size_t
Array::size() const
{
    return elements_.size();
}

ConfigValue const&
Array::at(std::size_t idx) const
{
    ASSERT(idx < elements_.size(), "Index is out of scope");
    return elements_[idx];
}

ConfigValue const&
Array::getArrayPattern() const
{
    return itemPattern_;
}

std::vector<ConfigValue>::const_iterator
Array::begin() const
{
    return elements_.begin();
}

std::vector<ConfigValue>::const_iterator
Array::end() const
{
    return elements_.end();
}

}  // namespace util::config
