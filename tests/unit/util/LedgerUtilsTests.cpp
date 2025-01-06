//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "rpc/JS.hpp"
#include "util/LedgerUtils.hpp"

#include <gtest/gtest.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <iterator>
#include <string_view>

TEST(LedgerUtilsTests, LedgerObjectTypeList)
{
    constexpr auto kTYPES = util::LedgerTypes::getLedgerEntryTypeStrList();
    static constexpr char const* kTYPES_LIST[] = {
        JS(account),
        JS(amendments),
        JS(check),
        JS(deposit_preauth),
        JS(directory),
        JS(escrow),
        JS(fee),
        JS(hashes),
        JS(offer),
        JS(payment_channel),
        JS(signer_list),
        JS(state),
        JS(ticket),
        JS(nft_offer),
        JS(nft_page),
        JS(amm),
        JS(bridge),
        JS(xchain_owned_claim_id),
        JS(xchain_owned_create_account_claim_id),
        JS(did),
        JS(mpt_issuance),
        JS(mptoken),
        JS(oracle),
        JS(credential),
        JS(nunl)
    };

    static_assert(std::size(kTYPES_LIST) == kTYPES.size());
    static_assert(std::ranges::all_of(kTYPES_LIST, [&kTYPES](std::string_view type) {
        return std::ranges::find(kTYPES, type) != std::cend(kTYPES);
    }));
}

TEST(LedgerUtilsTests, AccountOwnedTypeList)
{
    constexpr auto kACCOUNT_OWNED = util::LedgerTypes::getAccountOwnedLedgerTypeStrList();
    static constexpr char const* kCORRECT_TYPES[] = {
        JS(account),
        JS(check),
        JS(deposit_preauth),
        JS(escrow),
        JS(offer),
        JS(payment_channel),
        JS(signer_list),
        JS(state),
        JS(ticket),
        JS(nft_offer),
        JS(nft_page),
        JS(amm),
        JS(bridge),
        JS(xchain_owned_claim_id),
        JS(xchain_owned_create_account_claim_id),
        JS(did),
        JS(oracle),
        JS(credential),
        JS(mpt_issuance),
        JS(mptoken)
    };

    static_assert(std::size(kCORRECT_TYPES) == kACCOUNT_OWNED.size());
    static_assert(std::ranges::all_of(kCORRECT_TYPES, [&kACCOUNT_OWNED](std::string_view type) {
        return std::ranges::find(kACCOUNT_OWNED, type) != std::cend(kACCOUNT_OWNED);
    }));
}

TEST(LedgerUtilsTests, StrToType)
{
    EXPECT_EQ(util::LedgerTypes::getLedgerEntryTypeFromStr("mess"), ripple::ltANY);
    EXPECT_EQ(util::LedgerTypes::getLedgerEntryTypeFromStr("tomato"), ripple::ltANY);
    EXPECT_EQ(util::LedgerTypes::getLedgerEntryTypeFromStr("account"), ripple::ltACCOUNT_ROOT);

    constexpr auto kTYPES = util::LedgerTypes::getLedgerEntryTypeStrList();
    std::ranges::for_each(kTYPES, [](auto const& typeStr) {
        EXPECT_NE(util::LedgerTypes::getLedgerEntryTypeFromStr(typeStr), ripple::ltANY);
    });
}

TEST(LedgerUtilsTests, DeletionBlockerTypes)
{
    constexpr auto kTESTED_TYPES = util::LedgerTypes::getDeletionBlockerLedgerTypes();

    static constexpr ripple::LedgerEntryType kDELETION_BLOCKERS[] = {
        ripple::ltCHECK,
        ripple::ltESCROW,
        ripple::ltNFTOKEN_PAGE,
        ripple::ltPAYCHAN,
        ripple::ltRIPPLE_STATE,
        ripple::ltXCHAIN_OWNED_CLAIM_ID,
        ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID,
        ripple::ltBRIDGE,
        ripple::ltMPTOKEN_ISSUANCE,
        ripple::ltMPTOKEN
    };

    static_assert(std::size(kDELETION_BLOCKERS) == kTESTED_TYPES.size());
    static_assert(std::ranges::any_of(kTESTED_TYPES, [](auto const& type) {
        return std::find(std::cbegin(kDELETION_BLOCKERS), std::cend(kDELETION_BLOCKERS), type) !=
            std::cend(kDELETION_BLOCKERS);
    }));
}
