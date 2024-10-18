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

#pragma once

#include "rpc/JS.hpp"

#include <fmt/core.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <array>
#include <string>
#include <unordered_set>
#include <vector>

namespace util {

class LedgerTypes;

namespace impl {
class LedgerTypeAttribute {
    enum class LedgerCategory {
        Invalid,
        AccountOwned,    // The ledger object is owned by account
        Chain,           // The ledger object is shared across the chain
        DeletionBlocker  // The ledger object is owned by account and it blocks deletion
    };
    ripple::LedgerEntryType type = ripple::ltANY;
    char const* name = nullptr;
    LedgerCategory category = LedgerCategory::Invalid;

    constexpr LedgerTypeAttribute(char const* name, ripple::LedgerEntryType type, LedgerCategory category)
        : type(type), name(name), category(category)
    {
    }

public:
    static constexpr LedgerTypeAttribute
    ChainLedgerType(char const* name, ripple::LedgerEntryType type)
    {
        return LedgerTypeAttribute(name, type, LedgerCategory::Chain);
    }

    static constexpr LedgerTypeAttribute
    AccountOwnedLedgerType(char const* name, ripple::LedgerEntryType type)
    {
        return LedgerTypeAttribute(name, type, LedgerCategory::AccountOwned);
    }

    static constexpr LedgerTypeAttribute
    DeletionBlockerLedgerType(char const* name, ripple::LedgerEntryType type)
    {
        return LedgerTypeAttribute(name, type, LedgerCategory::DeletionBlocker);
    }
    friend class util::LedgerTypes;
};
}  // namespace impl

/**
 * @brief A helper class that provides lists of different ledger type catagory.
 *
 */
class LedgerTypes {
    using LedgerTypeAttribute = impl::LedgerTypeAttribute;
    using LedgerTypeAttributeList = LedgerTypeAttribute[];

    static constexpr LedgerTypeAttributeList const LEDGER_TYPES{
        LedgerTypeAttribute::AccountOwnedLedgerType(JS(account), ripple::ltACCOUNT_ROOT),
        LedgerTypeAttribute::ChainLedgerType(JS(amendments), ripple::ltAMENDMENTS),
        LedgerTypeAttribute::DeletionBlockerLedgerType(JS(check), ripple::ltCHECK),
        LedgerTypeAttribute::AccountOwnedLedgerType(JS(deposit_preauth), ripple::ltDEPOSIT_PREAUTH),
        // dir node belongs to account, but can not be filtered from account_objects
        LedgerTypeAttribute::ChainLedgerType(JS(directory), ripple::ltDIR_NODE),
        LedgerTypeAttribute::DeletionBlockerLedgerType(JS(escrow), ripple::ltESCROW),
        LedgerTypeAttribute::ChainLedgerType(JS(fee), ripple::ltFEE_SETTINGS),
        LedgerTypeAttribute::ChainLedgerType(JS(hashes), ripple::ltLEDGER_HASHES),
        LedgerTypeAttribute::AccountOwnedLedgerType(JS(offer), ripple::ltOFFER),
        LedgerTypeAttribute::DeletionBlockerLedgerType(JS(payment_channel), ripple::ltPAYCHAN),
        LedgerTypeAttribute::AccountOwnedLedgerType(JS(signer_list), ripple::ltSIGNER_LIST),
        LedgerTypeAttribute::DeletionBlockerLedgerType(JS(state), ripple::ltRIPPLE_STATE),
        LedgerTypeAttribute::AccountOwnedLedgerType(JS(ticket), ripple::ltTICKET),
        LedgerTypeAttribute::AccountOwnedLedgerType(JS(nft_offer), ripple::ltNFTOKEN_OFFER),
        LedgerTypeAttribute::DeletionBlockerLedgerType(JS(nft_page), ripple::ltNFTOKEN_PAGE),
        LedgerTypeAttribute::AccountOwnedLedgerType(JS(amm), ripple::ltAMM),
        LedgerTypeAttribute::DeletionBlockerLedgerType(JS(bridge), ripple::ltBRIDGE),
        LedgerTypeAttribute::DeletionBlockerLedgerType(JS(xchain_owned_claim_id), ripple::ltXCHAIN_OWNED_CLAIM_ID),
        LedgerTypeAttribute::DeletionBlockerLedgerType(
            JS(xchain_owned_create_account_claim_id),
            ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID
        ),
        LedgerTypeAttribute::AccountOwnedLedgerType(JS(did), ripple::ltDID),
        LedgerTypeAttribute::AccountOwnedLedgerType(JS(oracle), ripple::ltORACLE),
        LedgerTypeAttribute::ChainLedgerType(JS(nunl), ripple::ltNEGATIVE_UNL),
        LedgerTypeAttribute::DeletionBlockerLedgerType(JS(mpt_issuance), ripple::ltMPTOKEN_ISSUANCE),
        LedgerTypeAttribute::DeletionBlockerLedgerType(JS(mptoken), ripple::ltMPTOKEN),
    };

public:
    /**
     * @brief Returns a list of all ledger entry type as string.
     * @return A list of all ledger entry type as string.
     */
    static constexpr auto
    GetLedgerEntryTypeStrList()
    {
        std::array<char const*, std::size(LEDGER_TYPES)> res{};
        std::transform(std::begin(LEDGER_TYPES), std::end(LEDGER_TYPES), std::begin(res), [](auto const& item) {
            return item.name;
        });
        return res;
    }

    /**
     * @brief Returns a list of all account owned ledger entry type as string.
     *
     * @return A list of all account owned ledger entry type as string.
     */
    static constexpr auto
    GetAccountOwnedLedgerTypeStrList()
    {
        auto constexpr filter = [](auto const& item) {
            return item.category != LedgerTypeAttribute::LedgerCategory::Chain;
        };

        auto constexpr accountOwnedCount = std::count_if(std::begin(LEDGER_TYPES), std::end(LEDGER_TYPES), filter);
        std::array<char const*, accountOwnedCount> res{};
        auto it = std::begin(res);
        std::for_each(std::begin(LEDGER_TYPES), std::end(LEDGER_TYPES), [&](auto const& item) {
            if (filter(item)) {
                *it = item.name;
                ++it;
            }
        });
        return res;
    }

    /**
     * @brief Returns a list of all account deletion blocker's type as string.
     *
     * @return A list of all account deletion blocker's type as string.
     */
    static constexpr auto
    GetDeletionBlockerLedgerTypes()
    {
        auto constexpr filter = [](auto const& item) {
            return item.category == LedgerTypeAttribute::LedgerCategory::DeletionBlocker;
        };

        auto constexpr deletionBlockersCount = std::count_if(std::begin(LEDGER_TYPES), std::end(LEDGER_TYPES), filter);
        std::array<ripple::LedgerEntryType, deletionBlockersCount> res{};
        auto it = std::begin(res);
        std::for_each(std::begin(LEDGER_TYPES), std::end(LEDGER_TYPES), [&](auto const& item) {
            if (filter(item)) {
                *it = item.type;
                ++it;
            }
        });
        return res;
    }

    /**
     * @brief Returns the ripple::LedgerEntryType from the given string.
     *
     * @param entryName The name of the ledger entry type
     * @return The ripple::LedgerEntryType of the given string, returns ltANY if not found.
     */
    static ripple::LedgerEntryType
    GetLedgerEntryTypeFromStr(std::string const& entryName);
};

/**
 * @brief Deserializes a ripple::LedgerHeader from ripple::Slice of data.
 *
 * @param data The slice to deserialize
 * @return The deserialized ripple::LedgerHeader
 */
inline ripple::LedgerHeader
deserializeHeader(ripple::Slice data)
{
    return ripple::deserializeHeader(data, /* hasHash = */ true);
}

/**
 * @brief A helper function that converts a ripple::LedgerHeader to a string representation.
 *
 * @param info The ledger header
 * @return The string representation of the supplied ledger header
 */
inline std::string
toString(ripple::LedgerHeader const& info)
{
    return fmt::format(
        "LedgerHeader {{Sequence: {}, Hash: {}, TxHash: {}, AccountHash: {}, ParentHash: {}}}",
        info.seq,
        ripple::strHex(info.hash),
        strHex(info.txHash),
        ripple::strHex(info.accountHash),
        strHex(info.parentHash)
    );
}

}  // namespace util
