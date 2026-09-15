#pragma once

#include <fmt/format.h>
#include <rpcspec/LedgerTypes.hpp>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace util {

/**
 * @brief A helper class that provides lists of different ledger type category.
 */
class LedgerTypes {
    static constexpr auto const& kLedgerTypes = rpc::spec::kLedgerTypesTable;

public:
    /**
     * @brief Returns a list of all ledger entry type as string.
     * @return A list of all ledger entry type as string.
     */
    static constexpr auto
    getLedgerEntryTypeStrList()
    {
        std::array<std::string_view, std::size(kLedgerTypes)> res{};
        std::ranges::transform(kLedgerTypes, std::begin(res), [](auto const& item) {
            return item.rpcName;
        });
        return res;
    }

    /**
     * @brief Returns a list of all account deletion blocker's type as string.
     *
     * @return A list of all account deletion blocker's type as string.
     */
    static constexpr auto
    getDeletionBlockerLedgerTypes()
    {
        constexpr auto kFilter = [](auto const& item) {
            return item.category == rpc::spec::LedgerCategory::DeletionBlocker;
        };

        constexpr auto kDeletionBlockersCount =
            std::count_if(std::begin(kLedgerTypes), std::end(kLedgerTypes), kFilter);
        std::array<xrpl::LedgerEntryType, kDeletionBlockersCount> res{};
        auto it = std::begin(res);
        std::ranges::for_each(kLedgerTypes, [&](auto const& item) {
            if (kFilter(item)) {
                *it = item.type;
                ++it;
            }
        });
        return res;
    }
};

/**
 * @brief Deserializes a xrpl::LedgerHeader from xrpl::Slice of data.
 *
 * @param data The slice to deserialize
 * @return The deserialized xrpl::LedgerHeader
 */
inline xrpl::LedgerHeader
deserializeHeader(xrpl::Slice data)
{
    return xrpl::deserializeHeader(data, /* hasHash = */ true);
}

/**
 * @brief A helper function that converts a xrpl::LedgerHeader to a string representation.
 *
 * @param info The ledger header
 * @return The string representation of the supplied ledger header
 */
inline std::string
toString(xrpl::LedgerHeader const& info)
{
    return fmt::format(
        "LedgerHeader {{Sequence: {}, Hash: {}, TxHash: {}, AccountHash: {}, ParentHash: {}}}",
        info.seq,
        xrpl::strHex(info.hash),
        strHex(info.txHash),
        xrpl::strHex(info.accountHash),
        strHex(info.parentHash)
    );
}

}  // namespace util
