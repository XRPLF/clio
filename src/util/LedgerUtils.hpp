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

    ripple::LedgerEntryType type_ = ripple::ltANY;
    char const* name_ = nullptr;
    char const* rpcName_ = nullptr;
    LedgerCategory category_ = LedgerCategory::Invalid;

    constexpr LedgerTypeAttribute(
        char const* name,
        char const* rpcName,
        ripple::LedgerEntryType type,
        LedgerCategory category
    )
        : type_{type}, name_{name}, rpcName_{rpcName}, category_{category}
    {
    }

public:
    static constexpr LedgerTypeAttribute
    chainLedgerType(char const* name, char const* rpcName, ripple::LedgerEntryType type)
    {
        return LedgerTypeAttribute(name, rpcName, type, LedgerCategory::Chain);
    }

    static constexpr LedgerTypeAttribute
    accountOwnedLedgerType(char const* name, char const* rpcName, ripple::LedgerEntryType type)
    {
        return LedgerTypeAttribute(name, rpcName, type, LedgerCategory::AccountOwned);
    }

    static constexpr LedgerTypeAttribute
    deletionBlockerLedgerType(char const* name, char const* rpcName, ripple::LedgerEntryType type)
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
        LedgerTypeAttribute::accountOwnedLedgerType(
            JS(AccountRoot),
            JS(account),
            ripple::ltACCOUNT_ROOT
        ),
        LedgerTypeAttribute::chainLedgerType(JS(Amendments), JS(amendments), ripple::ltAMENDMENTS),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(Check), JS(check), ripple::ltCHECK),
        LedgerTypeAttribute::accountOwnedLedgerType(
            JS(DepositPreauth),
            JS(deposit_preauth),
            ripple::ltDEPOSIT_PREAUTH
        ),
        // dir node belongs to account, but can not be filtered from account_objects
        LedgerTypeAttribute::chainLedgerType(JS(DirectoryNode), JS(directory), ripple::ltDIR_NODE),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(Escrow), JS(escrow), ripple::ltESCROW),
        LedgerTypeAttribute::chainLedgerType(JS(FeeSettings), JS(fee), ripple::ltFEE_SETTINGS),
        LedgerTypeAttribute::chainLedgerType(JS(LedgerHashes), JS(hashes), ripple::ltLEDGER_HASHES),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(Offer), JS(offer), ripple::ltOFFER),
        LedgerTypeAttribute::deletionBlockerLedgerType(
            JS(PayChannel),
            JS(payment_channel),
            ripple::ltPAYCHAN
        ),
        LedgerTypeAttribute::accountOwnedLedgerType(
            JS(SignerList),
            JS(signer_list),
            ripple::ltSIGNER_LIST
        ),
        LedgerTypeAttribute::deletionBlockerLedgerType(
            JS(RippleState),
            JS(state),
            ripple::ltRIPPLE_STATE
        ),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(Ticket), JS(ticket), ripple::ltTICKET),
        LedgerTypeAttribute::accountOwnedLedgerType(
            JS(NFTokenOffer),
            JS(nft_offer),
            ripple::ltNFTOKEN_OFFER
        ),
        LedgerTypeAttribute::deletionBlockerLedgerType(
            JS(NFTokenPage),
            JS(nft_page),
            ripple::ltNFTOKEN_PAGE
        ),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(AMM), JS(amm), ripple::ltAMM),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(Bridge), JS(bridge), ripple::ltBRIDGE),
        LedgerTypeAttribute::deletionBlockerLedgerType(
            JS(XChainOwnedClaimID),
            JS(xchain_owned_claim_id),
            ripple::ltXCHAIN_OWNED_CLAIM_ID
        ),
        LedgerTypeAttribute::deletionBlockerLedgerType(
            JS(XChainOwnedCreateAccountClaimID),
            JS(xchain_owned_create_account_claim_id),
            ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID
        ),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(DID), JS(did), ripple::ltDID),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(Oracle), JS(oracle), ripple::ltORACLE),
        LedgerTypeAttribute::accountOwnedLedgerType(
            JS(Credential),
            JS(credential),
            ripple::ltCREDENTIAL
        ),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(Vault), JS(vault), ripple::ltVAULT),
        LedgerTypeAttribute::chainLedgerType(JS(NegativeUNL), JS(nunl), ripple::ltNEGATIVE_UNL),
        LedgerTypeAttribute::deletionBlockerLedgerType(
            JS(MPTokenIssuance),
            JS(mpt_issuance),
            ripple::ltMPTOKEN_ISSUANCE
        ),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(MPToken), JS(mptoken), ripple::ltMPTOKEN),
        LedgerTypeAttribute::deletionBlockerLedgerType(
            JS(PermissionedDomain),
            JS(permissioned_domain),
            ripple::ltPERMISSIONED_DOMAIN
        ),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(Delegate), JS(delegate), ripple::ltDELEGATE),
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
        std::ranges::transform(kLEDGER_TYPES, std::begin(res), [](auto const& item) {
            return item.rpcName_;
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
        constexpr auto kFILTER = [](auto const& item) {
            return item.category_ == LedgerTypeAttribute::LedgerCategory::DeletionBlocker;
        };

        constexpr auto kDELETION_BLOCKERS_COUNT =
            std::count_if(std::begin(kLEDGER_TYPES), std::end(kLEDGER_TYPES), kFILTER);
        std::array<ripple::LedgerEntryType, kDELETION_BLOCKERS_COUNT> res{};
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
     * @brief Returns the ripple::LedgerEntryType from the given string.
     *
     * @param entryName The name or canonical name (case-insensitive) of the ledger entry type for
     * all categories
     * @return The ripple::LedgerEntryType of the given string, returns ltANY if not found.
     */
    static ripple::LedgerEntryType
    getLedgerEntryTypeFromStr(std::string const& entryName);

    /**
     * @brief Returns the ripple::LedgerEntryType from the given string.
     *
     * @param entryName The name or canonical name (case-insensitive) of the ledger entry type for
     * account owned category
     * @return The ripple::LedgerEntryType of the given string, returns ltANY if not found.
     */
    static ripple::LedgerEntryType
    getAccountOwnedLedgerTypeFromStr(std::string const& entryName);

private:
    static std::optional<std::reference_wrapper<impl::LedgerTypeAttribute const>>
    getLedgerTypeAttributeFromStr(std::string const& entryName);
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
