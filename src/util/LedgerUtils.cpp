#include "util/LedgerUtils.hpp"

#include "util/JsonUtils.hpp"

#include <xrpl/protocol/LedgerFormats.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace util {

xrpl::LedgerEntryType
LedgerTypes::getLedgerEntryTypeFromStr(std::string const& entryName)
{
    if (auto const result = getLedgerTypeAttributeFromStr(entryName); result.has_value()) {
        return result->get().type_;
    }
    return xrpl::ltANY;
}

xrpl::LedgerEntryType
LedgerTypes::getAccountOwnedLedgerTypeFromStr(std::string const& entryName)
{
    if (auto const result = getLedgerTypeAttributeFromStr(entryName); result.has_value() &&
        result->get().category_ != LedgerTypeAttribute::LedgerCategory::Chain) {
        return result->get().type_;
    }

    return xrpl::ltANY;
}

std::optional<std::reference_wrapper<impl::LedgerTypeAttribute const>>
LedgerTypes::getLedgerTypeAttributeFromStr(std::string const& entryName)
{
    static std::unordered_map<
        std::string,
        std::reference_wrapper<impl::LedgerTypeAttribute const>> const kNameMap = []() {
        std::unordered_map<std::string, std::reference_wrapper<impl::LedgerTypeAttribute const>>
            map;
        std::ranges::for_each(kLedgerTypes, [&map](auto const& item) {
            map.insert({util::toLower(item.name_), item});
        });
        return map;
    }();

    static std::unordered_map<
        std::string,
        std::reference_wrapper<impl::LedgerTypeAttribute const>> const kRpcNameMap = []() {
        std::unordered_map<std::string, std::reference_wrapper<impl::LedgerTypeAttribute const>>
            map;
        std::ranges::for_each(kLedgerTypes, [&map](auto const& item) {
            map.insert({item.rpcName_, item});
        });
        return map;
    }();

    if (auto const it = kRpcNameMap.find(entryName); it != kRpcNameMap.end()) {
        return it->second;
    }

    auto const entryNameLowercase = util::toLower(entryName);
    if (auto const it = kNameMap.find(entryNameLowercase); it != kNameMap.end()) {
        return it->second;
    }

    return std::nullopt;
}

}  // namespace util
