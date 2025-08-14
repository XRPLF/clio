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
        JS(permissioned_domain),
        JS(oracle),
        JS(credential),
        JS(vault),
        JS(nunl),
        JS(delegate)
    };

    static_assert(std::size(kTYPES_LIST) == kTYPES.size());
    static_assert(std::ranges::all_of(kTYPES_LIST, [&kTYPES](std::string_view type) {
        return std::ranges::find(kTYPES, type) != std::cend(kTYPES);
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
        ripple::ltMPTOKEN,
        ripple::ltPERMISSIONED_DOMAIN
    };

    static_assert(std::size(kDELETION_BLOCKERS) == kTESTED_TYPES.size());
    static_assert(std::ranges::any_of(kTESTED_TYPES, [](auto const& type) {
        return std::find(std::cbegin(kDELETION_BLOCKERS), std::cend(kDELETION_BLOCKERS), type) !=
            std::cend(kDELETION_BLOCKERS);
    }));
}

struct LedgerTypeParam {
    std::string input;
    ripple::LedgerEntryType expected;
};

class AccountOwnedLedgerTypeFromStrTest : public ::testing::TestWithParam<LedgerTypeParam> {};

TEST_P(AccountOwnedLedgerTypeFromStrTest, Test)
{
    auto const& param = GetParam();
    auto result = util::LedgerTypes::getAccountOwnedLedgerTypeFromStr(param.input);
    EXPECT_EQ(result, param.expected);
}

INSTANTIATE_TEST_SUITE_P(
    LedgerUtilsTests,
    AccountOwnedLedgerTypeFromStrTest,
    ::testing::Values(
        // Using RPC name with exact match
        LedgerTypeParam{"account", ripple::ltACCOUNT_ROOT},
        LedgerTypeParam{"check", ripple::ltCHECK},
        LedgerTypeParam{"deposit_preauth", ripple::ltDEPOSIT_PREAUTH},
        LedgerTypeParam{"escrow", ripple::ltESCROW},
        LedgerTypeParam{"offer", ripple::ltOFFER},
        LedgerTypeParam{"payment_channel", ripple::ltPAYCHAN},
        LedgerTypeParam{"signer_list", ripple::ltSIGNER_LIST},
        LedgerTypeParam{"state", ripple::ltRIPPLE_STATE},
        LedgerTypeParam{"ticket", ripple::ltTICKET},
        LedgerTypeParam{"nft_offer", ripple::ltNFTOKEN_OFFER},
        LedgerTypeParam{"nft_page", ripple::ltNFTOKEN_PAGE},
        LedgerTypeParam{"amm", ripple::ltAMM},
        LedgerTypeParam{"bridge", ripple::ltBRIDGE},
        LedgerTypeParam{"xchain_owned_claim_id", ripple::ltXCHAIN_OWNED_CLAIM_ID},
        LedgerTypeParam{"xchain_owned_create_account_claim_id", ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID},
        LedgerTypeParam{"did", ripple::ltDID},
        LedgerTypeParam{"oracle", ripple::ltORACLE},
        LedgerTypeParam{"credential", ripple::ltCREDENTIAL},
        LedgerTypeParam{"mpt_issuance", ripple::ltMPTOKEN_ISSUANCE},
        LedgerTypeParam{"mptoken", ripple::ltMPTOKEN},
        LedgerTypeParam{"permissioned_domain", ripple::ltPERMISSIONED_DOMAIN},
        LedgerTypeParam{"vault", ripple::ltVAULT},
        LedgerTypeParam{"delegate", ripple::ltDELEGATE},

        // Using canonical name with exact match
        LedgerTypeParam{"AccountRoot", ripple::ltACCOUNT_ROOT},
        LedgerTypeParam{"Check", ripple::ltCHECK},
        LedgerTypeParam{"DepositPreauth", ripple::ltDEPOSIT_PREAUTH},
        LedgerTypeParam{"Escrow", ripple::ltESCROW},
        LedgerTypeParam{"Offer", ripple::ltOFFER},
        LedgerTypeParam{"PayChannel", ripple::ltPAYCHAN},
        LedgerTypeParam{"SignerList", ripple::ltSIGNER_LIST},
        LedgerTypeParam{"RippleState", ripple::ltRIPPLE_STATE},
        LedgerTypeParam{"Ticket", ripple::ltTICKET},
        LedgerTypeParam{"NFTokenOffer", ripple::ltNFTOKEN_OFFER},
        LedgerTypeParam{"NFTokenPage", ripple::ltNFTOKEN_PAGE},
        LedgerTypeParam{"AMM", ripple::ltAMM},
        LedgerTypeParam{"Bridge", ripple::ltBRIDGE},
        LedgerTypeParam{"XChainOwnedClaimID", ripple::ltXCHAIN_OWNED_CLAIM_ID},
        LedgerTypeParam{"XChainOwnedCreateAccountClaimID", ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID},
        LedgerTypeParam{"DID", ripple::ltDID},
        LedgerTypeParam{"Oracle", ripple::ltORACLE},
        LedgerTypeParam{"Credential", ripple::ltCREDENTIAL},
        LedgerTypeParam{"MPTokenIssuance", ripple::ltMPTOKEN_ISSUANCE},
        LedgerTypeParam{"MPToken", ripple::ltMPTOKEN},
        LedgerTypeParam{"PermissionedDomain", ripple::ltPERMISSIONED_DOMAIN},
        LedgerTypeParam{"Vault", ripple::ltVAULT},
        LedgerTypeParam{"Delegate", ripple::ltDELEGATE},

        // With canonical name in mixedcase
        LedgerTypeParam{"mPtOKenIssuance", ripple::ltMPTOKEN_ISSUANCE},
        // With canonical name in lowercase
        LedgerTypeParam{"mptokenissuance", ripple::ltMPTOKEN_ISSUANCE},
        // With RPC name with inexact match
        LedgerTypeParam{"MPT_Issuance", ripple::ltANY},

        // With invalid input
        LedgerTypeParam{"", ripple::ltANY},
        LedgerTypeParam{"1234", ripple::ltANY},
        LedgerTypeParam{"unknown", ripple::ltANY}
    )
);
