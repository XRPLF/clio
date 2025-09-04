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
#include <string>
#include <string_view>
#include <vector>

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
    EXPECT_EQ(util::LedgerTypes::getLedgerEntryTypeFromStr("AccoUnt"), ripple::ltANY);
    EXPECT_EQ(util::LedgerTypes::getLedgerEntryTypeFromStr("AccountRoot"), ripple::ltACCOUNT_ROOT);
    EXPECT_EQ(util::LedgerTypes::getLedgerEntryTypeFromStr("ACCOUNTRoot"), ripple::ltACCOUNT_ROOT);

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

struct LedgerEntryTypeParam {
    std::string input;
    ripple::LedgerEntryType expected;
};

static LedgerEntryTypeParam const kCHAIN_TEST_CASES[] = {
    // Using RPC name with exact match
    {.input = "amendments", .expected = ripple::ltAMENDMENTS},
    {.input = "directory", .expected = ripple::ltDIR_NODE},
    {.input = "fee", .expected = ripple::ltFEE_SETTINGS},
    {.input = "hashes", .expected = ripple::ltLEDGER_HASHES},
    {.input = "nunl", .expected = ripple::ltNEGATIVE_UNL},

    // Using canonical name with exact match
    {.input = "Amendments", .expected = ripple::ltAMENDMENTS},
    {.input = "DirectoryNode", .expected = ripple::ltDIR_NODE},
    {.input = "FeeSettings", .expected = ripple::ltFEE_SETTINGS},
    {.input = "LedgerHashes", .expected = ripple::ltLEDGER_HASHES},
    {.input = "NegativeUNL", .expected = ripple::ltNEGATIVE_UNL}
};

static LedgerEntryTypeParam const kACCOUNT_OWNED_TEST_CASES[] = {
    // Using RPC name with exact match
    {.input = "account", .expected = ripple::ltACCOUNT_ROOT},
    {.input = "check", .expected = ripple::ltCHECK},
    {.input = "deposit_preauth", .expected = ripple::ltDEPOSIT_PREAUTH},
    {.input = "escrow", .expected = ripple::ltESCROW},
    {.input = "offer", .expected = ripple::ltOFFER},
    {.input = "payment_channel", .expected = ripple::ltPAYCHAN},
    {.input = "signer_list", .expected = ripple::ltSIGNER_LIST},
    {.input = "state", .expected = ripple::ltRIPPLE_STATE},
    {.input = "ticket", .expected = ripple::ltTICKET},
    {.input = "nft_offer", .expected = ripple::ltNFTOKEN_OFFER},
    {.input = "nft_page", .expected = ripple::ltNFTOKEN_PAGE},
    {.input = "amm", .expected = ripple::ltAMM},
    {.input = "bridge", .expected = ripple::ltBRIDGE},
    {.input = "xchain_owned_claim_id", .expected = ripple::ltXCHAIN_OWNED_CLAIM_ID},
    {.input = "xchain_owned_create_account_claim_id", .expected = ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID},
    {.input = "did", .expected = ripple::ltDID},
    {.input = "oracle", .expected = ripple::ltORACLE},
    {.input = "credential", .expected = ripple::ltCREDENTIAL},
    {.input = "mpt_issuance", .expected = ripple::ltMPTOKEN_ISSUANCE},
    {.input = "mptoken", .expected = ripple::ltMPTOKEN},
    {.input = "permissioned_domain", .expected = ripple::ltPERMISSIONED_DOMAIN},
    {.input = "vault", .expected = ripple::ltVAULT},
    {.input = "delegate", .expected = ripple::ltDELEGATE},

    // Using canonical name with exact match
    {.input = "AccountRoot", .expected = ripple::ltACCOUNT_ROOT},
    {.input = "Check", .expected = ripple::ltCHECK},
    {.input = "DepositPreauth", .expected = ripple::ltDEPOSIT_PREAUTH},
    {.input = "Escrow", .expected = ripple::ltESCROW},
    {.input = "Offer", .expected = ripple::ltOFFER},
    {.input = "PayChannel", .expected = ripple::ltPAYCHAN},
    {.input = "SignerList", .expected = ripple::ltSIGNER_LIST},
    {.input = "RippleState", .expected = ripple::ltRIPPLE_STATE},
    {.input = "Ticket", .expected = ripple::ltTICKET},
    {.input = "NFTokenOffer", .expected = ripple::ltNFTOKEN_OFFER},
    {.input = "NFTokenPage", .expected = ripple::ltNFTOKEN_PAGE},
    {.input = "AMM", .expected = ripple::ltAMM},
    {.input = "Bridge", .expected = ripple::ltBRIDGE},
    {.input = "XChainOwnedClaimID", .expected = ripple::ltXCHAIN_OWNED_CLAIM_ID},
    {.input = "XChainOwnedCreateAccountClaimID", .expected = ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID},
    {.input = "DID", .expected = ripple::ltDID},
    {.input = "Oracle", .expected = ripple::ltORACLE},
    {.input = "Credential", .expected = ripple::ltCREDENTIAL},
    {.input = "MPTokenIssuance", .expected = ripple::ltMPTOKEN_ISSUANCE},
    {.input = "MPToken", .expected = ripple::ltMPTOKEN},
    {.input = "PermissionedDomain", .expected = ripple::ltPERMISSIONED_DOMAIN},
    {.input = "Vault", .expected = ripple::ltVAULT},
    {.input = "Delegate", .expected = ripple::ltDELEGATE}
};

static LedgerEntryTypeParam const kCASE_INSENSITIVE_TEST_CASES[] = {
    // With canonical name in mixedcase
    {.input = "mPtOKenIssuance", .expected = ripple::ltMPTOKEN_ISSUANCE},
    // With canonical name in lowercase
    {.input = "mptokenissuance", .expected = ripple::ltMPTOKEN_ISSUANCE},
};

static LedgerEntryTypeParam const kINVALID_TEST_CASES[] = {
    {.input = "", .expected = ripple::ltANY},
    {.input = "1234", .expected = ripple::ltANY},
    {.input = "unknown", .expected = ripple::ltANY},
    // With RPC name with inexact match
    {.input = "MPT_Issuance", .expected = ripple::ltANY}
};

class LedgerEntryTypeFromStrTest : public ::testing::TestWithParam<LedgerEntryTypeParam> {};

TEST_P(LedgerEntryTypeFromStrTest, GetLedgerEntryTypeFromStr)
{
    auto const& param = GetParam();
    auto const result = util::LedgerTypes::getLedgerEntryTypeFromStr(param.input);
    EXPECT_EQ(result, param.expected) << param.input;
}

INSTANTIATE_TEST_SUITE_P(
    LedgerUtilsTests,
    LedgerEntryTypeFromStrTest,
    ::testing::ValuesIn([]() {
        std::vector<LedgerEntryTypeParam> v;
        v.insert(v.end(), std::begin(kCHAIN_TEST_CASES), std::end(kCHAIN_TEST_CASES));
        v.insert(v.end(), std::begin(kACCOUNT_OWNED_TEST_CASES), std::end(kACCOUNT_OWNED_TEST_CASES));
        v.insert(v.end(), std::begin(kCASE_INSENSITIVE_TEST_CASES), std::end(kCASE_INSENSITIVE_TEST_CASES));
        v.insert(v.end(), std::begin(kINVALID_TEST_CASES), std::end(kINVALID_TEST_CASES));
        return v;
    }())
);

class AccountOwnedLedgerTypeFromStrTest : public ::testing::TestWithParam<LedgerEntryTypeParam> {};

TEST_P(AccountOwnedLedgerTypeFromStrTest, GetAccountOwnedLedgerTypeFromStr)
{
    auto const& param = GetParam();
    auto const result = util::LedgerTypes::getAccountOwnedLedgerTypeFromStr(param.input);
    EXPECT_EQ(result, param.expected);
}

INSTANTIATE_TEST_SUITE_P(
    LedgerUtilsTests,
    AccountOwnedLedgerTypeFromStrTest,
    ::testing::ValuesIn([]() {
        std::vector<LedgerEntryTypeParam> v;
        v.insert(v.end(), std::begin(kACCOUNT_OWNED_TEST_CASES), std::end(kACCOUNT_OWNED_TEST_CASES));
        v.insert(v.end(), std::begin(kCASE_INSENSITIVE_TEST_CASES), std::end(kCASE_INSENSITIVE_TEST_CASES));
        v.insert(v.end(), std::begin(kINVALID_TEST_CASES), std::end(kINVALID_TEST_CASES));
        v.push_back({"amendments", ripple::ltANY});  // chain type should return ltANY
        return v;
    }())
);
