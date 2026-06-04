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

#include <fmt/format.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
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

    xrpl::LedgerEntryType type_ = xrpl::ltANY;
    char const* name_ = nullptr;
    char const* rpcName_ = nullptr;
    LedgerCategory category_ = LedgerCategory::Invalid;

    constexpr LedgerTypeAttribute(
        char const* name,
        char const* rpcName,
        xrpl::LedgerEntryType type,
        LedgerCategory category
    )
        : type_{type}, name_{name}, rpcName_{rpcName}, category_{category}
    {
    }

public:
    static constexpr LedgerTypeAttribute
    chainLedgerType(char const* name, char const* rpcName, xrpl::LedgerEntryType type)
    {
        return LedgerTypeAttribute(name, rpcName, type, LedgerCategory::Chain);
    }

    static constexpr LedgerTypeAttribute
    accountOwnedLedgerType(char const* name, char const* rpcName, xrpl::LedgerEntryType type)
    {
        return LedgerTypeAttribute(name, rpcName, type, LedgerCategory::AccountOwned);
    }

    static constexpr LedgerTypeAttribute
    deletionBlockerLedgerType(char const* name, char const* rpcName, xrpl::LedgerEntryType type)
    {
        return LedgerTypeAttribute(name, rpcName, type, LedgerCategory::DeletionBlocker);
    }
    friend class util::LedgerTypes;
};

}  // namespace impl

/**
 * @brief A helper class that provides lists of different ledger type category.
 *
 */
class LedgerTypes {
    using LedgerTypeAttribute = impl::LedgerTypeAttribute;
    using LedgerTypeAttributeList = LedgerTypeAttribute[];

    static constexpr LedgerTypeAttributeList const kLEDGER_TYPES{
        LedgerTypeAttribute::accountOwnedLedgerType(JS(AccountRoot), JS(account), xrpl::ltACCOUNT_ROOT),
        LedgerTypeAttribute::chainLedgerType(JS(Amendments), JS(amendments), xrpl::ltAMENDMENTS),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(Check), JS(check), xrpl::ltCHECK),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(DepositPreauth), JS(deposit_preauth), xrpl::ltDEPOSIT_PREAUTH),
        // dir node belongs to account, but can not be filtered from account_objects
        LedgerTypeAttribute::chainLedgerType(JS(DirectoryNode), JS(directory), xrpl::ltDIR_NODE),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(Escrow), JS(escrow), xrpl::ltESCROW),
        LedgerTypeAttribute::chainLedgerType(JS(FeeSettings), JS(fee), xrpl::ltFEE_SETTINGS),
        LedgerTypeAttribute::chainLedgerType(JS(LedgerHashes), JS(hashes), xrpl::ltLEDGER_HASHES),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(Offer), JS(offer), xrpl::ltOFFER),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(PayChannel), JS(payment_channel), xrpl::ltPAYCHAN),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(SignerList), JS(signer_list), xrpl::ltSIGNER_LIST),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(RippleState), JS(state), xrpl::ltRIPPLE_STATE),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(Ticket), JS(ticket), xrpl::ltTICKET),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(NFTokenOffer), JS(nft_offer), xrpl::ltNFTOKEN_OFFER),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(NFTokenPage), JS(nft_page), xrpl::ltNFTOKEN_PAGE),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(AMM), JS(amm), xrpl::ltAMM),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(Bridge), JS(bridge), xrpl::ltBRIDGE),
        LedgerTypeAttribute::deletionBlockerLedgerType(
            JS(XChainOwnedClaimID),
            JS(xchain_owned_claim_id),
            xrpl::ltXCHAIN_OWNED_CLAIM_ID
        ),
        LedgerTypeAttribute::deletionBlockerLedgerType(
            JS(XChainOwnedCreateAccountClaimID),
            JS(xchain_owned_create_account_claim_id),
            xrpl::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID
        ),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(DID), JS(did), xrpl::ltDID),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(Oracle), JS(oracle), xrpl::ltORACLE),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(Credential), JS(credential), xrpl::ltCREDENTIAL),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(Vault), JS(vault), xrpl::ltVAULT),
        LedgerTypeAttribute::chainLedgerType(JS(NegativeUNL), JS(nunl), xrpl::ltNEGATIVE_UNL),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(MPTokenIssuance), JS(mpt_issuance), xrpl::ltMPTOKEN_ISSUANCE),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(MPToken), JS(mptoken), xrpl::ltMPTOKEN),
        LedgerTypeAttribute::deletionBlockerLedgerType(
            JS(PermissionedDomain),
            JS(permissioned_domain),
            xrpl::ltPERMISSIONED_DOMAIN
        ),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(Delegate), JS(delegate), xrpl::ltDELEGATE),
    };

public:
    /**
     * @brief Returns a list of all ledger entry type as string.
     * @return A list of all ledger entry type as string.
     */
    static constexpr auto
    getLedgerEntryTypeStrList()
    {
        std::array<char const*, std::size(kLEDGER_TYPES)> res{};
        std::ranges::transform(kLEDGER_TYPES, std::begin(res), [](auto const& item) { return item.rpcName_; });
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
        constexpr auto kFILTER = [](auto const& item) {
            return item.category_ == LedgerTypeAttribute::LedgerCategory::DeletionBlocker;
        };

        constexpr auto kDELETION_BLOCKERS_COUNT =
            std::count_if(std::begin(kLEDGER_TYPES), std::end(kLEDGER_TYPES), kFILTER);
        std::array<xrpl::LedgerEntryType, kDELETION_BLOCKERS_COUNT> res{};
        auto it = std::begin(res);
        std::ranges::for_each(kLEDGER_TYPES, [&](auto const& item) {
            if (kFILTER(item)) {
                *it = item.type_;
                ++it;
            }
        });
        return res;
    }

    /**
     * @brief Returns the xrpl::LedgerEntryType from the given string.
     *
     * @param entryName The name or canonical name (case-insensitive) of the ledger entry type for all categories
     * @return The xrpl::LedgerEntryType of the given string, returns ltANY if not found.
     */
    static xrpl::LedgerEntryType
    getLedgerEntryTypeFromStr(std::string const& entryName);

    /**
     * @brief Returns the xrpl::LedgerEntryType from the given string.
     *
     * @param entryName The name or canonical name (case-insensitive) of the ledger entry type for account owned
     * category
     * @return The xrpl::LedgerEntryType of the given string, returns ltANY if not found.
     */
    static xrpl::LedgerEntryType
    getAccountOwnedLedgerTypeFromStr(std::string const& entryName);

private:
    static std::optional<std::reference_wrapper<impl::LedgerTypeAttribute const>>
    getLedgerTypeAttributeFromStr(std::string const& entryName);
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
