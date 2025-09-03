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

struct LedgerEntryTypeParam {
    std::string input;
    ripple::LedgerEntryType expected;
};

static LedgerEntryTypeParam const kChainTestCases[] = {
    // Using RPC name with exact match
    {"amendments", ripple::ltAMENDMENTS},
    {"directory", ripple::ltDIR_NODE},
    {"fee", ripple::ltFEE_SETTINGS},
    {"hashes", ripple::ltLEDGER_HASHES},
    {"nunl", ripple::ltNEGATIVE_UNL},

    // Using canonical name with exact match
    {"Amendments", ripple::ltAMENDMENTS},
    {"DirectoryNode", ripple::ltDIR_NODE},
    {"FeeSettings", ripple::ltFEE_SETTINGS},
    {"LedgerHashes", ripple::ltLEDGER_HASHES},
    {"NegativeUNL", ripple::ltNEGATIVE_UNL}
};

static LedgerEntryTypeParam const kAccountOwnedTestCases[] = {
    // Using RPC name with exact match
    {"account", ripple::ltACCOUNT_ROOT},
    {"check", ripple::ltCHECK},
    {"deposit_preauth", ripple::ltDEPOSIT_PREAUTH},
    {"escrow", ripple::ltESCROW},
    {"offer", ripple::ltOFFER},
    {"payment_channel", ripple::ltPAYCHAN},
    {"signer_list", ripple::ltSIGNER_LIST},
    {"state", ripple::ltRIPPLE_STATE},
    {"ticket", ripple::ltTICKET},
    {"nft_offer", ripple::ltNFTOKEN_OFFER},
    {"nft_page", ripple::ltNFTOKEN_PAGE},
    {"amm", ripple::ltAMM},
    {"bridge", ripple::ltBRIDGE},
    {"xchain_owned_claim_id", ripple::ltXCHAIN_OWNED_CLAIM_ID},
    {"xchain_owned_create_account_claim_id", ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID},
    {"did", ripple::ltDID},
    {"oracle", ripple::ltORACLE},
    {"credential", ripple::ltCREDENTIAL},
    {"mpt_issuance", ripple::ltMPTOKEN_ISSUANCE},
    {"mptoken", ripple::ltMPTOKEN},
    {"permissioned_domain", ripple::ltPERMISSIONED_DOMAIN},
    {"vault", ripple::ltVAULT},
    {"delegate", ripple::ltDELEGATE},

    // Using canonical name with exact match
    {"AccountRoot", ripple::ltACCOUNT_ROOT},
    {"Check", ripple::ltCHECK},
    {"DepositPreauth", ripple::ltDEPOSIT_PREAUTH},
    {"Escrow", ripple::ltESCROW},
    {"Offer", ripple::ltOFFER},
    {"PayChannel", ripple::ltPAYCHAN},
    {"SignerList", ripple::ltSIGNER_LIST},
    {"RippleState", ripple::ltRIPPLE_STATE},
    {"Ticket", ripple::ltTICKET},
    {"NFTokenOffer", ripple::ltNFTOKEN_OFFER},
    {"NFTokenPage", ripple::ltNFTOKEN_PAGE},
    {"AMM", ripple::ltAMM},
    {"Bridge", ripple::ltBRIDGE},
    {"XChainOwnedClaimID", ripple::ltXCHAIN_OWNED_CLAIM_ID},
    {"XChainOwnedCreateAccountClaimID", ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID},
    {"DID", ripple::ltDID},
    {"Oracle", ripple::ltORACLE},
    {"Credential", ripple::ltCREDENTIAL},
    {"MPTokenIssuance", ripple::ltMPTOKEN_ISSUANCE},
    {"MPToken", ripple::ltMPTOKEN},
    {"PermissionedDomain", ripple::ltPERMISSIONED_DOMAIN},
    {"Vault", ripple::ltVAULT},
    {"Delegate", ripple::ltDELEGATE}
};

static LedgerEntryTypeParam const kCaseInsensitiveTestCases[] = {
    // With canonical name in mixedcase
    {"mPtOKenIssuance", ripple::ltMPTOKEN_ISSUANCE},
    // With canonical name in lowercase
    {"mptokenissuance", ripple::ltMPTOKEN_ISSUANCE},
};

static LedgerEntryTypeParam const kInvalidTestCases[] = {
    {"", ripple::ltANY},
    {"1234", ripple::ltANY},
    {"unknown", ripple::ltANY},
    // With RPC name with inexact match
    {"MPT_Issuance", ripple::ltANY}
};

class LedgerEntryTypeFromStrTest : public ::testing::TestWithParam<LedgerEntryTypeParam> {};

TEST_P(LedgerEntryTypeFromStrTest, GetLedgerEntryTypeFromStr)
{
    auto const& param = GetParam();
    auto const result = util::LedgerTypes::getLedgerEntryTypeFromStr(param.input);
    EXPECT_EQ(result, param.expected);
}

INSTANTIATE_TEST_SUITE_P(LedgerUtilsTests, LedgerEntryTypeFromStrTest, ::testing::ValuesIn([]() {
                             std::vector<LedgerEntryTypeParam> v;
                             v.insert(v.end(), std::begin(kChainTestCases), std::end(kChainTestCases));
                             v.insert(v.end(), std::begin(kAccountOwnedTestCases), std::end(kAccountOwnedTestCases));
                             v.insert(
                                 v.end(), std::begin(kCaseInsensitiveTestCases), std::end(kCaseInsensitiveTestCases)
                             );
                             v.insert(v.end(), std::begin(kInvalidTestCases), std::end(kInvalidTestCases));
                             return v;
                         }()));

class AccountOwnedLedgerTypeFromStrTest : public ::testing::TestWithParam<LedgerEntryTypeParam> {};

TEST_P(AccountOwnedLedgerTypeFromStrTest, GetAccountOwnedLedgerTypeFromStr)
{
    auto const& param = GetParam();
    auto const result = util::LedgerTypes::getAccountOwnedLedgerTypeFromStr(param.input);
    EXPECT_EQ(result, param.expected);
}

INSTANTIATE_TEST_SUITE_P(LedgerUtilsTests, AccountOwnedLedgerTypeFromStrTest, ::testing::ValuesIn([]() {
                             std::vector<LedgerEntryTypeParam> v;
                             v.insert(v.end(), std::begin(kAccountOwnedTestCases), std::end(kAccountOwnedTestCases));
                             v.insert(
                                 v.end(), std::begin(kCaseInsensitiveTestCases), std::end(kCaseInsensitiveTestCases)
                             );
                             v.insert(v.end(), std::begin(kInvalidTestCases), std::end(kInvalidTestCases));
                             v.push_back({"amendments", ripple::ltANY});  // chain type should return ltANY
                             return v;
                         }()));
