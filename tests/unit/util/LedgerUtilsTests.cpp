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
    constexpr auto kTypes = util::LedgerTypes::getLedgerEntryTypeStrList();
    static constexpr char const* kTypesList[] = {
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
        JS(loan_broker),
        JS(loan),
        JS(nunl),
        JS(delegate)
    };

    static_assert(std::size(kTypesList) == kTypes.size());
    static_assert(std::ranges::all_of(kTypesList, [&kTypes](std::string_view type) {
        return std::ranges::find(kTypes, type) != std::cend(kTypes);
    }));
}

TEST(LedgerUtilsTests, StrToType)
{
    EXPECT_EQ(util::LedgerTypes::getLedgerEntryTypeFromStr("mess"), xrpl::ltANY);
    EXPECT_EQ(util::LedgerTypes::getLedgerEntryTypeFromStr("tomato"), xrpl::ltANY);
    EXPECT_EQ(util::LedgerTypes::getLedgerEntryTypeFromStr("account"), xrpl::ltACCOUNT_ROOT);
    EXPECT_EQ(util::LedgerTypes::getLedgerEntryTypeFromStr("AccoUnt"), xrpl::ltANY);
    EXPECT_EQ(util::LedgerTypes::getLedgerEntryTypeFromStr("AccountRoot"), xrpl::ltACCOUNT_ROOT);
    EXPECT_EQ(util::LedgerTypes::getLedgerEntryTypeFromStr("ACCOUNTRoot"), xrpl::ltACCOUNT_ROOT);

    constexpr auto kTypes = util::LedgerTypes::getLedgerEntryTypeStrList();
    std::ranges::for_each(kTypes, [](auto const& typeStr) {
        EXPECT_NE(util::LedgerTypes::getLedgerEntryTypeFromStr(typeStr), xrpl::ltANY);
    });
}

TEST(LedgerUtilsTests, DeletionBlockerTypes)
{
    constexpr auto kTestedTypes = util::LedgerTypes::getDeletionBlockerLedgerTypes();

    static constexpr xrpl::LedgerEntryType kDeletionBlockers[] = {
        xrpl::ltCHECK,
        xrpl::ltESCROW,
        xrpl::ltNFTOKEN_PAGE,
        xrpl::ltPAYCHAN,
        xrpl::ltRIPPLE_STATE,
        xrpl::ltXCHAIN_OWNED_CLAIM_ID,
        xrpl::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID,
        xrpl::ltBRIDGE,
        xrpl::ltMPTOKEN_ISSUANCE,
        xrpl::ltMPTOKEN,
        xrpl::ltPERMISSIONED_DOMAIN,
        xrpl::ltLOAN
    };

    static_assert(std::size(kDeletionBlockers) == kTestedTypes.size());
    static_assert(std::ranges::any_of(kTestedTypes, [](auto const& type) {
        return std::find(std::cbegin(kDeletionBlockers), std::cend(kDeletionBlockers), type) !=
            std::cend(kDeletionBlockers);
    }));
}

struct LedgerEntryTypeParam {
    std::string input;
    xrpl::LedgerEntryType expected;
};

static LedgerEntryTypeParam const kChainTestCases[] = {
    // Using RPC name with exact match
    {.input = "amendments", .expected = xrpl::ltAMENDMENTS},
    {.input = "directory", .expected = xrpl::ltDIR_NODE},
    {.input = "fee", .expected = xrpl::ltFEE_SETTINGS},
    {.input = "hashes", .expected = xrpl::ltLEDGER_HASHES},
    {.input = "nunl", .expected = xrpl::ltNEGATIVE_UNL},

    // Using canonical name with exact match
    {.input = "Amendments", .expected = xrpl::ltAMENDMENTS},
    {.input = "DirectoryNode", .expected = xrpl::ltDIR_NODE},
    {.input = "FeeSettings", .expected = xrpl::ltFEE_SETTINGS},
    {.input = "LedgerHashes", .expected = xrpl::ltLEDGER_HASHES},
    {.input = "NegativeUNL", .expected = xrpl::ltNEGATIVE_UNL}
};

static LedgerEntryTypeParam const kAccountOwnedTestCases[] = {
    // Using RPC name with exact match
    {.input = "account", .expected = xrpl::ltACCOUNT_ROOT},
    {.input = "check", .expected = xrpl::ltCHECK},
    {.input = "deposit_preauth", .expected = xrpl::ltDEPOSIT_PREAUTH},
    {.input = "escrow", .expected = xrpl::ltESCROW},
    {.input = "offer", .expected = xrpl::ltOFFER},
    {.input = "payment_channel", .expected = xrpl::ltPAYCHAN},
    {.input = "signer_list", .expected = xrpl::ltSIGNER_LIST},
    {.input = "state", .expected = xrpl::ltRIPPLE_STATE},
    {.input = "ticket", .expected = xrpl::ltTICKET},
    {.input = "nft_offer", .expected = xrpl::ltNFTOKEN_OFFER},
    {.input = "nft_page", .expected = xrpl::ltNFTOKEN_PAGE},
    {.input = "amm", .expected = xrpl::ltAMM},
    {.input = "bridge", .expected = xrpl::ltBRIDGE},
    {.input = "xchain_owned_claim_id", .expected = xrpl::ltXCHAIN_OWNED_CLAIM_ID},
    {.input = "xchain_owned_create_account_claim_id",
     .expected = xrpl::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID},
    {.input = "did", .expected = xrpl::ltDID},
    {.input = "oracle", .expected = xrpl::ltORACLE},
    {.input = "credential", .expected = xrpl::ltCREDENTIAL},
    {.input = "mpt_issuance", .expected = xrpl::ltMPTOKEN_ISSUANCE},
    {.input = "mptoken", .expected = xrpl::ltMPTOKEN},
    {.input = "permissioned_domain", .expected = xrpl::ltPERMISSIONED_DOMAIN},
    {.input = "vault", .expected = xrpl::ltVAULT},
    {.input = "loan_broker", .expected = xrpl::ltLOAN_BROKER},
    {.input = "loan", .expected = xrpl::ltLOAN},
    {.input = "delegate", .expected = xrpl::ltDELEGATE},

    // Using canonical name with exact match
    {.input = "AccountRoot", .expected = xrpl::ltACCOUNT_ROOT},
    {.input = "Check", .expected = xrpl::ltCHECK},
    {.input = "DepositPreauth", .expected = xrpl::ltDEPOSIT_PREAUTH},
    {.input = "Escrow", .expected = xrpl::ltESCROW},
    {.input = "Offer", .expected = xrpl::ltOFFER},
    {.input = "PayChannel", .expected = xrpl::ltPAYCHAN},
    {.input = "SignerList", .expected = xrpl::ltSIGNER_LIST},
    {.input = "RippleState", .expected = xrpl::ltRIPPLE_STATE},
    {.input = "Ticket", .expected = xrpl::ltTICKET},
    {.input = "NFTokenOffer", .expected = xrpl::ltNFTOKEN_OFFER},
    {.input = "NFTokenPage", .expected = xrpl::ltNFTOKEN_PAGE},
    {.input = "AMM", .expected = xrpl::ltAMM},
    {.input = "Bridge", .expected = xrpl::ltBRIDGE},
    {.input = "XChainOwnedClaimID", .expected = xrpl::ltXCHAIN_OWNED_CLAIM_ID},
    {.input = "XChainOwnedCreateAccountClaimID",
     .expected = xrpl::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID},
    {.input = "DID", .expected = xrpl::ltDID},
    {.input = "Oracle", .expected = xrpl::ltORACLE},
    {.input = "Credential", .expected = xrpl::ltCREDENTIAL},
    {.input = "MPTokenIssuance", .expected = xrpl::ltMPTOKEN_ISSUANCE},
    {.input = "MPToken", .expected = xrpl::ltMPTOKEN},
    {.input = "PermissionedDomain", .expected = xrpl::ltPERMISSIONED_DOMAIN},
    {.input = "Vault", .expected = xrpl::ltVAULT},
    {.input = "LoanBroker", .expected = xrpl::ltLOAN_BROKER},
    {.input = "Loan", .expected = xrpl::ltLOAN},
    {.input = "Delegate", .expected = xrpl::ltDELEGATE}
};

static LedgerEntryTypeParam const kCaseInsensitiveTestCases[] = {
    // With canonical name in mixedcase
    {.input = "mPtOKenIssuance", .expected = xrpl::ltMPTOKEN_ISSUANCE},
    // With canonical name in lowercase
    {.input = "mptokenissuance", .expected = xrpl::ltMPTOKEN_ISSUANCE},
};

static LedgerEntryTypeParam const kInvalidTestCases[] = {
    {.input = "", .expected = xrpl::ltANY},
    {.input = "1234", .expected = xrpl::ltANY},
    {.input = "unknown", .expected = xrpl::ltANY},
    // With RPC name with inexact match
    {.input = "MPT_Issuance", .expected = xrpl::ltANY}
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
        v.insert(v.end(), std::begin(kChainTestCases), std::end(kChainTestCases));
        v.insert(v.end(), std::begin(kAccountOwnedTestCases), std::end(kAccountOwnedTestCases));
        v.insert(
            v.end(), std::begin(kCaseInsensitiveTestCases), std::end(kCaseInsensitiveTestCases)
        );
        v.insert(v.end(), std::begin(kInvalidTestCases), std::end(kInvalidTestCases));
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
        v.insert(v.end(), std::begin(kAccountOwnedTestCases), std::end(kAccountOwnedTestCases));
        v.insert(
            v.end(), std::begin(kCaseInsensitiveTestCases), std::end(kCaseInsensitiveTestCases)
        );
        v.insert(v.end(), std::begin(kInvalidTestCases), std::end(kInvalidTestCases));
        v.push_back({"amendments", xrpl::ltANY});  // chain type should return ltANY
        return v;
    }())
);
