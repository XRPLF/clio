#include "util/JsonUtils.hpp"

#include <xrpl/protocol/TxFormats.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <unordered_set>

namespace util {

/**
 * @brief Get the transaction types in lowercase
 *
 * @return The transaction types in lowercase
 */
[[nodiscard]] std::unordered_set<std::string> const&
getTxTypesInLowercase()
{
    static std::unordered_set<std::string> const kTypesKeysInLowercase = []() {
        std::unordered_set<std::string> keys;
        std::transform(
            xrpl::TxFormats::getInstance().begin(),
            xrpl::TxFormats::getInstance().end(),
            std::inserter(keys, keys.begin()),
            [](auto const& pair) { return util::toLower(pair.getName()); }
        );
        return keys;
    }();

    return kTypesKeysInLowercase;
}
}  // namespace util
