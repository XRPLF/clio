#include "data/AmendmentCenter.hpp"
#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/BookOffers.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockAmendmentCenter.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Asset.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/MPTIssue.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/UintTypes.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";

constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kIndex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kIndex2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

constexpr auto kPayS20UsdGetS10XrpBookDir =
    "43B83ADC452B85FCBADA6CAEAC5181C255A213630D58FFD455071AFD498D0000";

constexpr auto kPayS20XrpGetS10UsdBookDir =
    "7B1767D41DBCE79D9585CF9D0262A5FEC45E5206FF524F8B55071AFD498D0000";

constexpr auto kTransferRateX2 = 2000000000;

constexpr auto kDomain = "F10D0CC9A0F9A3CBF585B80BE09A186483668FDBDD39AA7E3370F3649CE134E5";

struct ParameterTestBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

}  // namespace

using namespace rpc;
using namespace data;
using namespace testing;

struct RPCBookOffersHandlerTest : HandlerBaseTest {
    RPCBookOffersHandlerTest()
    {
        backend_->setRange(10, 300);
    }

protected:
    StrictMockAmendmentCenterSharedPtr mockAmendmentCenterPtr_;
};

struct RPCBookOffersParameterTest : RPCBookOffersHandlerTest,
                                    WithParamInterface<ParameterTestBundle> {};

TEST_P(RPCBookOffersParameterTest, CheckError)
{
    auto bundle = GetParam();
    auto const handler = AnyHandler{BookOffersHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output =
            handler.process(boost::json::parse(bundle.testJson), Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), bundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), bundle.expectedErrorMessage);
    });
}

static auto
generateParameterBookOffersTestBundles()
{
    return std::vector<ParameterTestBundle>{
        ParameterTestBundle{
            .testName = "MissingTakerGets",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "USD",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'taker_gets' missing"
        },
        ParameterTestBundle{
            .testName = "MissingTakerPays",
            .testJson = R"JSON({
                "taker_gets": {
                    "currency": "USD",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'taker_pays' missing"
        },
        ParameterTestBundle{
            .testName = "WrongTypeTakerPays",
            .testJson = R"JSON({
                "taker_pays": "wrong",
                "taker_gets": {
                    "currency": "XRP"
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        ParameterTestBundle{
            .testName = "WrongTypeTakerGets",
            .testJson = R"JSON({
                "taker_gets": "wrong",
                "taker_pays": {
                    "currency": "XRP"
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        ParameterTestBundle{
            .testName = "TakerPaysMissingCurrency",
            .testJson = R"JSON({
                "taker_pays": {},
                "taker_gets": {
                    "currency": "XRP"
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Missing field 'taker_pays.currency'."
        },
        ParameterTestBundle{
            .testName = "TakerGetsMissingCurrency",
            .testJson = R"JSON({
                "taker_gets": {},
                "taker_pays": {
                    "currency": "XRP"
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Missing field 'taker_gets.currency'."
        },
        ParameterTestBundle{
            .testName = "TakerGetsWrongCurrency",
            .testJson = R"JSON({
                "taker_gets": {
                    "currency": "CNYY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_pays": {
                    "currency": "XRP"
                }
            })JSON",
            .expectedError = "dstAmtMalformed",
            .expectedErrorMessage = "Destination amount/currency/issuer is malformed."
        },
        ParameterTestBundle{
            .testName = "TakerPaysWrongCurrency",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNYY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "XRP"
                }
            })JSON",
            .expectedError = "srcCurMalformed",
            .expectedErrorMessage = "Source currency is malformed."
        },
        ParameterTestBundle{
            // A present-but-non-string currency is reported by validateTakerJSON as an
            // expectedFieldError ('<field>.currency', not string) before the per-field validators.
            .testName = "TakerGetsCurrencyNotString",
            .testJson = R"JSON({
                "taker_gets": {
                    "currency": 123,
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_pays": {
                    "currency": "XRP"
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'taker_gets.currency', not string."
        },
        ParameterTestBundle{
            .testName = "TakerPaysCurrencyNotString",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": 123,
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "XRP"
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'taker_pays.currency', not string."
        },
        ParameterTestBundle{
            .testName = "TakerGetsWrongIssuer",
            .testJson = R"JSON({
                "taker_gets": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs5"
                },
                "taker_pays": {
                    "currency": "XRP"
                }
            })JSON",
            .expectedError = "dstIsrMalformed",
            .expectedErrorMessage = "Destination issuer is malformed."
        },
        ParameterTestBundle{
            .testName = "TakerPaysWrongIssuer",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs5"
                },
                "taker_gets": {
                    "currency": "XRP"
                }
            })JSON",
            .expectedError = "srcIsrMalformed",
            .expectedErrorMessage = "Source issuer is malformed."
        },
        ParameterTestBundle{
            .testName = "InvalidTaker",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "XRP"
                },
                "taker": "123"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'taker'."
        },
        ParameterTestBundle{
            .testName = "TakerNotString",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "XRP"
                },
                "taker": 123
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'taker'."
        },
        ParameterTestBundle{
            .testName = "Domain_InvalidType",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "XRP"
                },
                "domain": 0
            })JSON",
            .expectedError = "domainMalformed",
            .expectedErrorMessage = "Unable to parse domain."
        },
        ParameterTestBundle{
            .testName = "Domain_InvalidInt",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "XRP"
                },
                "domain": "123"
            })JSON",
            .expectedError = "domainMalformed",
            .expectedErrorMessage = "Unable to parse domain."
        },
        ParameterTestBundle{
            .testName = "Domain_InvalidObject",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "XRP"
                },
                "domain": {}
            })JSON",
            .expectedError = "domainMalformed",
            .expectedErrorMessage = "Unable to parse domain."
        },
        ParameterTestBundle{
            .testName = "LimitNotInt",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "XRP"
                },
                "limit": "123"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        ParameterTestBundle{
            .testName = "LimitNegative",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "XRP"
                },
                "limit": -1
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        ParameterTestBundle{
            .testName = "LimitZero",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "XRP"
                },
                "limit": 0
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        ParameterTestBundle{
            .testName = "LedgerIndexInvalid",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "XRP"
                },
                "ledger_index": "xxx"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed"
        },
        ParameterTestBundle{
            .testName = "LedgerHashInvalid",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "XRP"
                },
                "ledger_hash": "xxx"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed"
        },
        ParameterTestBundle{
            .testName = "LedgerHashNotString",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "XRP"
                },
                "ledger_hash": 123
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString"
        },
        ParameterTestBundle{
            .testName = "GetsPaysXRPWithIssuer",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "XRP",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "CNY",
                    "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                }
            })JSON",
            .expectedError = "srcIsrMalformed",
            .expectedErrorMessage =
                "Unneeded field 'taker_pays.issuer' for XRP currency specification."
        },
        ParameterTestBundle{
            .testName = "PaysCurrencyWithXRPIssuer",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "JPY"
                },
                "taker_gets": {
                    "currency": "CNY",
                    "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                }
            })JSON",
            .expectedError = "srcIsrMalformed",
            .expectedErrorMessage = "Invalid field 'taker_pays.issuer', expected non-XRP issuer."
        },
        ParameterTestBundle{
            .testName = "GetsCurrencyWithXRPIssuer",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "XRP"
                },
                "taker_gets": {
                    "currency": "CNY"
                }
            })JSON",
            .expectedError = "dstIsrMalformed",
            .expectedErrorMessage = "Invalid field 'taker_gets.issuer', expected non-XRP issuer."
        },
        ParameterTestBundle{
            .testName = "GetsXRPWithIssuer",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "XRP",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                }
            })JSON",
            .expectedError = "dstIsrMalformed",
            .expectedErrorMessage =
                "Unneeded field 'taker_gets.issuer' for XRP currency specification."
        },
        ParameterTestBundle{
            .testName = "BadMarket",
            .testJson = R"JSON({
                "taker_pays": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets": {
                    "currency": "CNY",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                }
            })JSON",
            .expectedError = "badMarket",
            .expectedErrorMessage = "No such market."
        },
        ParameterTestBundle{
            .testName = "TakerGetsMptIdAndCurrency",
            .testJson = R"JSON({
                "taker_gets": {
                    "currency": "USD",
                    "mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405DBADD"
                },
                "taker_pays": {
                    "currency": "XRP"
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'taker_gets'."
        },
        ParameterTestBundle{
            .testName = "TakerPaysMptIdAndCurrency",
            .testJson = R"JSON({
                "taker_gets": {
                    "currency": "XRP"
                },
                "taker_pays": {
                    "currency": "USD",
                    "mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405DBADD"
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'taker_pays'."
        },
        ParameterTestBundle{
            .testName = "TakerGetsMptIdAndIssuer",
            .testJson = R"JSON({
                "taker_gets": {
                    "mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405DBADD",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_pays": {
                    "currency": "XRP"
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'taker_gets'."
        },
        ParameterTestBundle{
            .testName = "TakerPaysMptIdAndIssuer",
            .testJson = R"JSON({
                "taker_gets": {
                    "currency": "XRP"
                },
                "taker_pays": {
                    "mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405DBADD",
                    "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                }
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'taker_pays'."
        },
        ParameterTestBundle{
            .testName = "TakerGetsMalformedMptId",
            .testJson = R"JSON({
                "taker_gets": {
                    "mpt_issuance_id": "NOTAHEX"
                },
                "taker_pays": {
                    "currency": "XRP"
                }
            })JSON",
            .expectedError = "dstAmtMalformed",
            .expectedErrorMessage = "Destination amount/currency/issuer is malformed."
        },
        ParameterTestBundle{
            .testName = "TakerPaysMalformedMptId",
            .testJson = R"JSON({
                "taker_gets": {
                    "currency": "XRP"
                },
                "taker_pays": {
                    "mpt_issuance_id": "NOTAHEX"
                }
            })JSON",
            .expectedError = "srcCurMalformed",
            .expectedErrorMessage = "Source currency is malformed."
        },
        ParameterTestBundle{
            .testName = "MPTBadMarket",
            .testJson = R"JSON({
                "taker_gets": {
                    "mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405DBADD"
                },
                "taker_pays": {
                    "mpt_issuance_id": "000004C463C52827307480341125DA0577DEFC38405DBADD"
                }
            })JSON",
            .expectedError = "badMarket",
            .expectedErrorMessage = "No such market."
        },
        // The "account one" issuer (rrrrrrrrrrrrrrrrrrrrBZbvji == xrpl::noAccount()) is rejected,
        // mirroring rippled's parseTakerIssuerJSON "bad issuer account one" check.
        ParameterTestBundle{
            .testName = "TakerGetsIssuerAccountOne",
            .testJson = R"JSON({
                "taker_gets": {
                    "currency": "USD",
                    "issuer": "rrrrrrrrrrrrrrrrrrrrBZbvji"
                },
                "taker_pays": {
                    "currency": "XRP"
                }
            })JSON",
            .expectedError = "dstIsrMalformed",
            .expectedErrorMessage = "Destination issuer is malformed."
        },
        ParameterTestBundle{
            .testName = "TakerPaysIssuerAccountOne",
            .testJson = R"JSON({
                "taker_gets": {
                    "currency": "XRP"
                },
                "taker_pays": {
                    "currency": "USD",
                    "issuer": "rrrrrrrrrrrrrrrrrrrrBZbvji"
                }
            })JSON",
            .expectedError = "srcIsrMalformed",
            .expectedErrorMessage = "Source issuer is malformed."
        }
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCBookOffersHandler,
    RPCBookOffersParameterTest,
    testing::ValuesIn(generateParameterBookOffersTestBundles()),
    tests::util::kNameGenerator
);

struct BookOffersNormalTestBundle {
    std::string testName;
    std::string inputJson;
    std::map<xrpl::uint256, std::optional<xrpl::uint256>> mockedSuccessors;
    std::map<xrpl::uint256, Blob> mockedLedgerObjects;
    uint32_t ledgerObjectCalls;
    std::vector<xrpl::STObject> mockedOffers;
    std::string expectedJson;
    uint32_t amendmentIsEnabledCalls = 0;
};

struct RPCBookOffersNormalPathTest : public RPCBookOffersHandlerTest,
                                     public WithParamInterface<BookOffersNormalTestBundle> {};

TEST_P(RPCBookOffersNormalPathTest, CheckOutput)
{
    auto const& bundle = GetParam();
    auto const seq = 300;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, seq);
    ON_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerHeader));

    EXPECT_CALL(*mockAmendmentCenterPtr_, isEnabled(_, Amendments::fixFrozenLPTokenTransfer, _))
        .Times(bundle.amendmentIsEnabledCalls);
    ON_CALL(*mockAmendmentCenterPtr_, isEnabled(_, Amendments::fixFrozenLPTokenTransfer, _))
        .WillByDefault(Return(false));

    // return valid book dir
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(bundle.mockedSuccessors.size());
    for (auto const& [key, value] : bundle.mockedSuccessors) {
        ON_CALL(*backend_, doFetchSuccessorKey(key, seq, _)).WillByDefault(Return(value));
    }

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(bundle.ledgerObjectCalls);

    for (auto const& [key, value] : bundle.mockedLedgerObjects) {
        ON_CALL(*backend_, doFetchLedgerObject(key, seq, _)).WillByDefault(Return(value));
    }

    std::vector<Blob> bbs;
    std::ranges::transform(
        bundle.mockedOffers,

        std::back_inserter(bbs),
        [](auto const& obj) { return obj.getSerializer().peekData(); }
    );
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const handler = AnyHandler{BookOffersHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output =
            handler.process(boost::json::parse(bundle.inputJson), Context{.yield = yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), boost::json::parse(bundle.expectedJson));
    });
}

static auto
generateNormalPathBookOffersTestBundles()
{
    auto const account = getAccountIdWithString(kAccount);
    auto const account2 = getAccountIdWithString(kAccount2);

    auto const frozenTrustLine = createRippleStateLedgerObject(
        "USD", kAccount, -8, kAccount2, 1000, kAccount, 2000, kIndex1, 2, xrpl::lsfLowFreeze
    );

    auto const gets10USDPays20XRPOffer = createOfferLedgerObject(
        kAccount2,
        10,
        20,
        xrpl::to_string(xrpl::toCurrency("USD")),
        xrpl::to_string(xrpl::xrpCurrency()),
        kAccount,
        toBase58(xrpl::xrpAccount()),
        kPayS20XrpGetS10UsdBookDir
    );

    auto const gets10USDPays20XRPOwnerOffer = createOfferLedgerObject(
        kAccount,
        10,
        20,
        xrpl::to_string(xrpl::toCurrency("USD")),
        xrpl::to_string(xrpl::xrpCurrency()),
        kAccount,
        toBase58(xrpl::xrpAccount()),
        kPayS20XrpGetS10UsdBookDir
    );

    auto const gets10XRPPays20USDOffer = createOfferLedgerObject(
        kAccount2,
        10,
        20,
        xrpl::to_string(xrpl::xrpCurrency()),
        xrpl::to_string(xrpl::toCurrency("USD")),
        toBase58(xrpl::xrpAccount()),
        kAccount,
        kPayS20UsdGetS10XrpBookDir
    );

    auto const gets10XRPPays20USDOfferWithDomain = createOfferLedgerObject(
        kAccount2,
        10,
        20,
        xrpl::to_string(xrpl::xrpCurrency()),
        xrpl::to_string(xrpl::toCurrency("USD")),
        toBase58(xrpl::xrpAccount()),
        kAccount,
        kPayS20UsdGetS10XrpBookDir,
        kDomain
    );

    auto const getsXRPPaysUSDBook = getBookBase(
        rpc::parseBook(
            xrpl::toCurrency("USD"), account, xrpl::xrpCurrency(), xrpl::xrpAccount(), std::nullopt
        )
            .value()
    );
    auto const getsXRPPaysUSDBookWithDomain = getBookBase(
        rpc::parseBook(
            xrpl::toCurrency("USD"), account, xrpl::xrpCurrency(), xrpl::xrpAccount(), kDomain
        )
            .value()
    );
    auto const getsUSDPaysXRPBook = getBookBase(
        rpc::parseBook(
            xrpl::xrpCurrency(), xrpl::xrpAccount(), xrpl::toCurrency("USD"), account, std::nullopt
        )
            .value()
    );

    auto const getsXRPPaysUSDInputJson = fmt::format(
        R"JSON({{
            "taker_gets": {{
                "currency": "XRP"
            }},
            "taker_pays": {{
                "currency": "USD",
                "issuer": "{}"
            }}
        }})JSON",
        kAccount
    );

    auto const getsXRPPaysUSDInputJsonWithDomain = fmt::format(
        R"JSON({{
            "taker_gets": {{
                "currency": "XRP"
            }},
            "taker_pays": {{
                "currency": "USD",
                "issuer": "{}"
            }},
            "domain": "{}"
        }})JSON",
        kAccount,
        kDomain
    );

    auto const paysXRPGetsUSDInputJson = fmt::format(
        R"JSON({{
            "taker_pays": {{
                "currency": "XRP"
            }},
            "taker_gets": {{
                "currency": "USD",
                "issuer": "{}"
            }}
        }})JSON",
        kAccount
    );

    auto const feeLedgerObject = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);

    auto const trustline30Balance = createRippleStateLedgerObject(
        "USD", kAccount, -30, kAccount2, 1000, kAccount, 2000, kIndex1, 2, 0
    );

    auto const trustline8Balance = createRippleStateLedgerObject(
        "USD", kAccount, -8, kAccount2, 1000, kAccount, 2000, kIndex1, 2, 0
    );

    return std::vector<BookOffersNormalTestBundle>{
        BookOffersNormalTestBundle{
            .testName = "PaysUSDGetsXRPNoFrozenOwnerFundEnough",
            .inputJson = getsXRPPaysUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<xrpl::uint256, std::optional<xrpl::uint256>>{
                    {getsXRPPaysUSDBook, xrpl::uint256{kPayS20UsdGetS10XrpBookDir}},
                    {xrpl::uint256{kPayS20UsdGetS10XrpBookDir}, std::optional<xrpl::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<xrpl::uint256, xrpl::Blob>{
                    // book dir object
                    {xrpl::uint256{kPayS20UsdGetS10XrpBookDir},
                     createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1)
                         .getSerializer()
                         .peekData()},
                    // pays issuer account object
                    {xrpl::keylet::account(account).key,
                     createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2)
                         .getSerializer()
                         .peekData()},
                    // owner account object
                    {xrpl::keylet::account(account2).key,
                     createAccountRootObject(kAccount2, 0, 2, 200, 2, kIndex1, 2)
                         .getSerializer()
                         .peekData()},
                    // fee settings: base ->3 inc->2, account2 has 2 objects ,total
                    // reserve ->7
                    // owner_funds should be 193
                    {xrpl::keylet::feeSettings().key, feeLedgerObject}
                },
            .ledgerObjectCalls = 5,
            .mockedOffers = std::vector<xrpl::STObject>{gets10XRPPays20USDOffer},
            .expectedJson = fmt::format(
                R"JSON({{
                    "ledger_hash": "{}",
                    "ledger_index": 300,
                    "offers": [
                        {{
                            "Account": "{}",
                            "BookDirectory": "43B83ADC452B85FCBADA6CAEAC5181C255A213630D58FFD455071AFD498D0000",
                            "BookNode": "0",
                            "Flags": 0,
                            "LedgerEntryType": "Offer",
                            "OwnerNode": "0",
                            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq": 0,
                            "Sequence": 0,
                            "TakerGets": "10",
                            "TakerPays": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "20"
                            }},
                            "index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds": "{}",
                            "quality": "{}"
                        }}
                    ]
                }})JSON",
                kLedgerHash,
                kAccount2,
                193,
                2
            )
        },
        BookOffersNormalTestBundle{
            .testName = "PaysUSDGetsXRPNoFrozenOwnerFundNotEnough",
            .inputJson = getsXRPPaysUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<xrpl::uint256, std::optional<xrpl::uint256>>{
                    {getsXRPPaysUSDBook, xrpl::uint256{kPayS20UsdGetS10XrpBookDir}},
                    {xrpl::uint256{kPayS20UsdGetS10XrpBookDir}, std::optional<xrpl::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<xrpl::uint256, xrpl::Blob>{
                    // book dir object
                    {xrpl::uint256{kPayS20UsdGetS10XrpBookDir},
                     createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1)
                         .getSerializer()
                         .peekData()},
                    // pays issuer account object
                    {xrpl::keylet::account(account).key,
                     createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2)
                         .getSerializer()
                         .peekData()},
                    // owner account object, hold
                    {xrpl::keylet::account(account2).key,
                     createAccountRootObject(kAccount2, 0, 2, 5 + 7, 2, kIndex1, 2)
                         .getSerializer()
                         .peekData()},
                    // fee settings: base ->3 inc->2, account2 has 2 objects
                    // ,total
                    // reserve ->7
                    {xrpl::keylet::feeSettings().key, feeLedgerObject}
                },
            .ledgerObjectCalls = 5,
            .mockedOffers = std::vector<xrpl::STObject>{gets10XRPPays20USDOffer},
            .expectedJson = fmt::format(
                R"JSON({{
                    "ledger_hash": "{}",
                    "ledger_index": 300,
                    "offers": [
                        {{
                            "Account": "{}",
                            "BookDirectory": "43B83ADC452B85FCBADA6CAEAC5181C255A213630D58FFD455071AFD498D0000",
                            "BookNode": "0",
                            "Flags": 0,
                            "LedgerEntryType": "Offer",
                            "OwnerNode": "0",
                            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq": 0,
                            "Sequence": 0,
                            "TakerGets": "10",
                            "TakerPays": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "20"
                            }},
                            "index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds": "{}",
                            "quality": "{}",
                            "taker_gets_funded": "5",
                            "taker_pays_funded": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "10"
                            }}
                        }}
                    ]
                }})JSON",
                kLedgerHash,
                kAccount2,
                5,
                2
            )
        },
        BookOffersNormalTestBundle{
            .testName = "PaysUSDGetsXRPFrozen",
            .inputJson = getsXRPPaysUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<xrpl::uint256, std::optional<xrpl::uint256>>{
                    {getsXRPPaysUSDBook, xrpl::uint256{kPayS20UsdGetS10XrpBookDir}},
                    {xrpl::uint256{kPayS20UsdGetS10XrpBookDir}, std::optional<xrpl::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<xrpl::uint256, xrpl::Blob>{
                    // book dir object
                    {xrpl::uint256{kPayS20UsdGetS10XrpBookDir},
                     createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1)
                         .getSerializer()
                         .peekData()},
                    // pays issuer account object
                    {xrpl::keylet::account(account).key,
                     createAccountRootObject(kAccount, xrpl::lsfGlobalFreeze, 2, 200, 2, kIndex1, 2)
                         .getSerializer()
                         .peekData()}
                },
            .ledgerObjectCalls = 3,
            .mockedOffers = std::vector<xrpl::STObject>{gets10XRPPays20USDOffer},
            .expectedJson = fmt::format(
                R"JSON({{
                    "ledger_hash": "{}",
                    "ledger_index": 300,
                    "offers": [
                        {{
                            "Account": "{}",
                            "BookDirectory": "43B83ADC452B85FCBADA6CAEAC5181C255A213630D58FFD455071AFD498D0000",
                            "BookNode": "0",
                            "Flags": 0,
                            "LedgerEntryType": "Offer",
                            "OwnerNode": "0",
                            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq": 0,
                            "Sequence": 0,
                            "TakerGets": "10",
                            "TakerPays": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "20"
                            }},
                            "index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds": "{}",
                            "quality": "{}",
                            "taker_gets_funded": "0",
                            "taker_pays_funded": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "0"
                            }}
                        }}
                    ]
                }})JSON",
                kLedgerHash,
                kAccount2,
                0,
                2
            )
        },
        BookOffersNormalTestBundle{
            .testName = "PaysUSDGetsXRPFrozenWithDomain",
            .inputJson = getsXRPPaysUSDInputJsonWithDomain,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<xrpl::uint256, std::optional<xrpl::uint256>>{
                    {getsXRPPaysUSDBookWithDomain, xrpl::uint256{kPayS20UsdGetS10XrpBookDir}},
                    {xrpl::uint256{kPayS20UsdGetS10XrpBookDir}, std::optional<xrpl::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<xrpl::uint256, xrpl::Blob>{
                    // book dir object
                    {xrpl::uint256{kPayS20UsdGetS10XrpBookDir},
                     createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1)
                         .getSerializer()
                         .peekData()},
                    // pays issuer account object
                    {xrpl::keylet::account(account).key,
                     createAccountRootObject(kAccount, xrpl::lsfGlobalFreeze, 2, 200, 2, kIndex1, 2)
                         .getSerializer()
                         .peekData()}
                },
            .ledgerObjectCalls = 3,
            .mockedOffers = std::vector<xrpl::STObject>{gets10XRPPays20USDOfferWithDomain},
            .expectedJson = fmt::format(
                R"JSON({{
                    "ledger_hash": "{}",
                    "ledger_index": 300,
                    "offers": [
                        {{
                            "Account": "{}",
                            "BookDirectory": "43B83ADC452B85FCBADA6CAEAC5181C255A213630D58FFD455071AFD498D0000",
                            "BookNode": "0",
                            "DomainID": "F10D0CC9A0F9A3CBF585B80BE09A186483668FDBDD39AA7E3370F3649CE134E5",
                            "Flags": 0,
                            "LedgerEntryType": "Offer",
                            "OwnerNode": "0",
                            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq": 0,
                            "Sequence": 0,
                            "TakerGets": "10",
                            "TakerPays": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "20"
                            }},
                            "index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds": "{}",
                            "quality": "{}",
                            "taker_gets_funded": "0",
                            "taker_pays_funded": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "0"
                            }}
                        }}
                    ]
                }})JSON",
                kLedgerHash,
                kAccount2,
                0,
                2
            )
        },
        BookOffersNormalTestBundle{
            .testName = "GetsUSDPaysXRPFrozen",
            .inputJson = paysXRPGetsUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<xrpl::uint256, std::optional<xrpl::uint256>>{
                    {getsUSDPaysXRPBook, xrpl::uint256{kPayS20XrpGetS10UsdBookDir}},
                    {xrpl::uint256{kPayS20XrpGetS10UsdBookDir}, std::optional<xrpl::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<xrpl::uint256, xrpl::Blob>{
                    // book dir object
                    {xrpl::uint256{kPayS20XrpGetS10UsdBookDir},
                     createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1)
                         .getSerializer()
                         .peekData()},
                    // gets issuer account object
                    {xrpl::keylet::account(account).key,
                     createAccountRootObject(
                         kAccount, xrpl::lsfGlobalFreeze, 2, 200, 2, kIndex1, 2, kTransferRateX2
                     )
                         .getSerializer()
                         .peekData()}
                },
            .ledgerObjectCalls = 3,
            .mockedOffers = std::vector<xrpl::STObject>{gets10USDPays20XRPOffer},
            .expectedJson = fmt::format(
                R"JSON({{
                    "ledger_hash": "{}",
                    "ledger_index": 300,
                    "offers": [
                        {{
                            "Account": "{}",
                            "BookDirectory": "{}",
                            "BookNode": "0",
                            "Flags": 0,
                            "LedgerEntryType": "Offer",
                            "OwnerNode": "0",
                            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq": 0,
                            "Sequence": 0,
                            "TakerPays": "20",
                            "TakerGets": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "10"
                            }},
                            "index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds": "{}",
                            "quality": "{}",
                            "taker_pays_funded": "0",
                            "taker_gets_funded": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "0"
                            }}
                        }}
                    ]
                }})JSON",
                kLedgerHash,
                kAccount2,
                kPayS20XrpGetS10UsdBookDir,
                0,
                2
            )
        },
        BookOffersNormalTestBundle{
            .testName = "PaysXRPGetsUSDWithTransferFee",
            .inputJson = paysXRPGetsUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<xrpl::uint256, std::optional<xrpl::uint256>>{
                    {getsUSDPaysXRPBook, xrpl::uint256{kPayS20XrpGetS10UsdBookDir}},
                    {xrpl::uint256{kPayS20XrpGetS10UsdBookDir}, std::optional<xrpl::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<xrpl::uint256, xrpl::Blob>{
                    // book dir object
                    {xrpl::uint256{kPayS20XrpGetS10UsdBookDir},
                     createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1)
                         .getSerializer()
                         .peekData()},
                    // gets issuer account object, rate is 1/2
                    {xrpl::keylet::account(account).key,
                     createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2, kTransferRateX2)
                         .getSerializer()
                         .peekData()},
                    // trust line between gets issuer and owner,owner has 8 USD
                    {xrpl::keylet::trustLine(account2, account, xrpl::toCurrency("USD")).key,
                     trustline8Balance.getSerializer().peekData()},
                },
            .ledgerObjectCalls = 6,
            .mockedOffers = std::vector<xrpl::STObject>{gets10USDPays20XRPOffer},
            .expectedJson = fmt::format(
                R"JSON({{
                    "ledger_hash": "{}",
                    "ledger_index": 300,
                    "offers": [
                        {{
                            "Account": "{}",
                            "BookDirectory": "{}",
                            "BookNode": "0",
                            "Flags": 0,
                            "LedgerEntryType": "Offer",
                            "OwnerNode": "0",
                            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq": 0,
                            "Sequence": 0,
                            "TakerPays": "20",
                            "TakerGets": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "10"
                            }},
                            "index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds": "{}",
                            "quality": "{}",
                            "taker_gets_funded": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "4"
                            }},
                            "taker_pays_funded": "8"
                        }}
                    ]
                }})JSON",
                kLedgerHash,
                kAccount2,
                kPayS20XrpGetS10UsdBookDir,
                8,
                2
            ),
            .amendmentIsEnabledCalls = 1,
        },
        BookOffersNormalTestBundle{
            .testName = "PaysXRPGetsUSDWithMultipleOffers",
            .inputJson = paysXRPGetsUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<xrpl::uint256, std::optional<xrpl::uint256>>{
                    {getsUSDPaysXRPBook, xrpl::uint256{kPayS20XrpGetS10UsdBookDir}},
                    {xrpl::uint256{kPayS20XrpGetS10UsdBookDir}, std::optional<xrpl::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<xrpl::uint256, xrpl::Blob>{
                    // book dir object
                    {xrpl::uint256{kPayS20XrpGetS10UsdBookDir},
                     createOwnerDirLedgerObject(
                         {xrpl::uint256{kIndex2}, xrpl::uint256{kIndex2}}, kIndex1
                     )
                         .getSerializer()
                         .peekData()},
                    // gets issuer account object
                    {xrpl::keylet::account(account).key,
                     createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2, kTransferRateX2)
                         .getSerializer()
                         .peekData()},
                    // trust line between gets issuer and owner,owner has 30 USD
                    {xrpl::keylet::trustLine(account2, account, xrpl::toCurrency("USD")).key,
                     trustline30Balance.getSerializer().peekData()},
                },
            .ledgerObjectCalls = 6,
            .mockedOffers =
                std::vector<xrpl::STObject>{
                    // After offer1, balance is 30 - 2*10 = 10
                    gets10USDPays20XRPOffer,
                    // offer2 not fully funded, balance is 10, rate is 2, so only
                    // gets 5
                    gets10USDPays20XRPOffer
                },
            .expectedJson = fmt::format(
                R"JSON({{
                    "ledger_hash": "{}",
                    "ledger_index": 300,
                    "offers": [
                        {{
                            "Account": "{}",
                            "BookDirectory": "{}",
                            "BookNode": "0",
                            "Flags": 0,
                            "LedgerEntryType": "Offer",
                            "OwnerNode": "0",
                            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq": 0,
                            "Sequence": 0,
                            "TakerPays": "20",
                            "TakerGets": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "10"
                            }},
                            "index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds": "{}",
                            "quality": "{}"
                        }},
                        {{
                            "Account": "{}",
                            "BookDirectory": "{}",
                            "BookNode": "0",
                            "Flags": 0,
                            "LedgerEntryType": "Offer",
                            "OwnerNode": "0",
                            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq": 0,
                            "Sequence": 0,
                            "TakerPays": "20",
                            "TakerGets": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "10"
                            }},
                            "index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "taker_gets_funded": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "5"
                            }},
                            "taker_pays_funded": "10",
                            "quality": "{}"
                        }}
                    ]
                }})JSON",
                kLedgerHash,
                kAccount2,
                kPayS20XrpGetS10UsdBookDir,
                30,
                2,
                kAccount2,
                kPayS20XrpGetS10UsdBookDir,
                2
            ),
            .amendmentIsEnabledCalls = 1,
        },
        BookOffersNormalTestBundle{
            .testName = "PaysXRPGetsUSDSellingOwnCurrency",
            .inputJson = paysXRPGetsUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<xrpl::uint256, std::optional<xrpl::uint256>>{
                    {getsUSDPaysXRPBook, xrpl::uint256{kPayS20XrpGetS10UsdBookDir}},
                    {xrpl::uint256{kPayS20XrpGetS10UsdBookDir}, std::optional<xrpl::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<xrpl::uint256, xrpl::Blob>{
                    // book dir object
                    {xrpl::uint256{kPayS20XrpGetS10UsdBookDir},
                     createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1)
                         .getSerializer()
                         .peekData()},
                    // gets issuer account object, rate is 1/2
                    {xrpl::keylet::account(account).key,
                     createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2, kTransferRateX2)
                         .getSerializer()
                         .peekData()},
                },
            .ledgerObjectCalls = 3,
            .mockedOffers = std::vector<xrpl::STObject>{gets10USDPays20XRPOwnerOffer},
            .expectedJson = fmt::format(
                R"JSON({{
                    "ledger_hash": "{}",
                    "ledger_index": 300,
                    "offers": [
                        {{
                            "Account": "{}",
                            "BookDirectory": "{}",
                            "BookNode": "0",
                            "Flags": 0,
                            "LedgerEntryType": "Offer",
                            "OwnerNode": "0",
                            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq": 0,
                            "Sequence": 0,
                            "TakerPays": "20",
                            "TakerGets": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "10"
                            }},
                            "index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds": "{}",
                            "quality": "{}"
                        }}
                    ]
                }})JSON",
                kLedgerHash,
                kAccount,
                kPayS20XrpGetS10UsdBookDir,
                10,
                2
            )
        },
        BookOffersNormalTestBundle{
            .testName = "PaysXRPGetsUSDTrustLineFrozen",
            .inputJson = paysXRPGetsUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<xrpl::uint256, std::optional<xrpl::uint256>>{
                    {getsUSDPaysXRPBook, xrpl::uint256{kPayS20XrpGetS10UsdBookDir}},
                    {xrpl::uint256{kPayS20XrpGetS10UsdBookDir}, std::optional<xrpl::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<xrpl::uint256, xrpl::Blob>{
                    // book dir object
                    {xrpl::uint256{kPayS20XrpGetS10UsdBookDir},
                     createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1)
                         .getSerializer()
                         .peekData()},
                    // gets issuer account object, rate is 1/2
                    {xrpl::keylet::account(account).key,
                     createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2, kTransferRateX2)
                         .getSerializer()
                         .peekData()},
                    // trust line between gets issuer and owner,owner has 8 USD
                    {xrpl::keylet::trustLine(account2, account, xrpl::toCurrency("USD")).key,
                     frozenTrustLine.getSerializer().peekData()},
                },
            .ledgerObjectCalls = 6,
            .mockedOffers = std::vector<xrpl::STObject>{gets10USDPays20XRPOffer},
            .expectedJson = fmt::format(
                R"JSON({{
                    "ledger_hash": "{}",
                    "ledger_index": 300,
                    "offers": [
                        {{
                            "Account": "{}",
                            "BookDirectory": "{}",
                            "BookNode": "0",
                            "Flags": 0,
                            "LedgerEntryType": "Offer",
                            "OwnerNode": "0",
                            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq": 0,
                            "Sequence": 0,
                            "TakerPays": "20",
                            "TakerGets": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "10"
                            }},
                            "index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds": "{}",
                            "quality": "{}",
                            "taker_gets_funded": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "0"
                            }},
                            "taker_pays_funded": "0"
                        }}
                    ]
                }})JSON",
                kLedgerHash,
                kAccount2,
                kPayS20XrpGetS10UsdBookDir,
                0,
                2
            ),
        },
        BookOffersNormalTestBundle{
            .testName = "PaysXRPGetsUSDIsDeepFrozen",
            .inputJson = paysXRPGetsUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<xrpl::uint256, std::optional<xrpl::uint256>>{
                    {getsUSDPaysXRPBook, xrpl::uint256{kPayS20XrpGetS10UsdBookDir}},
                    {xrpl::uint256{kPayS20XrpGetS10UsdBookDir}, std::optional<xrpl::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<xrpl::uint256, xrpl::Blob>{
                    // book dir object
                    {xrpl::uint256{kPayS20XrpGetS10UsdBookDir},
                     createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1)
                         .getSerializer()
                         .peekData()},
                    // gets issuer account object, is deep frozen so unfunded
                    {xrpl::keylet::account(account).key,
                     createAccountRootObject(
                         kAccount, xrpl::lsfLowDeepFreeze, 2, 200, 2, kIndex1, 2, kTransferRateX2
                     )
                         .getSerializer()
                         .peekData()},
                },
            .ledgerObjectCalls = 4,
            .mockedOffers = std::vector<xrpl::STObject>{gets10USDPays20XRPOffer},
            .expectedJson = fmt::format(
                R"JSON({{
                    "ledger_hash": "{}",
                    "ledger_index": 300,
                    "offers": [
                        {{
                            "Account": "{}",
                            "BookDirectory": "{}",
                            "BookNode": "0",
                            "Flags": 0,
                            "LedgerEntryType": "Offer",
                            "OwnerNode": "0",
                            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq": 0,
                            "Sequence": 0,
                            "TakerPays": "20",
                            "TakerGets": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "10"
                            }},
                            "index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds": "{}",
                            "quality": "{}",
                            "taker_gets_funded": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "0"
                            }},
                            "taker_pays_funded": "0"
                        }}
                    ]
                }})JSON",
                kLedgerHash,
                kAccount2,
                kPayS20XrpGetS10UsdBookDir,
                0,
                2
            )
        },
        BookOffersNormalTestBundle{
            .testName = "PaysXRPGetsUSDTrustLineFrozenAndIsDeepFrozen",
            .inputJson = paysXRPGetsUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<xrpl::uint256, std::optional<xrpl::uint256>>{
                    {getsUSDPaysXRPBook, xrpl::uint256{kPayS20XrpGetS10UsdBookDir}},
                    {xrpl::uint256{kPayS20XrpGetS10UsdBookDir}, std::optional<xrpl::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<xrpl::uint256, xrpl::Blob>{
                    // book dir object
                    {xrpl::uint256{kPayS20XrpGetS10UsdBookDir},
                     createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1)
                         .getSerializer()
                         .peekData()},
                    // gets issuer account object, is deep frozen so unfunded
                    {xrpl::keylet::account(account).key,
                     createAccountRootObject(
                         kAccount, xrpl::lsfLowDeepFreeze, 2, 200, 2, kIndex1, 2, kTransferRateX2
                     )
                         .getSerializer()
                         .peekData()},
                    {xrpl::keylet::trustLine(account2, account, xrpl::toCurrency("USD")).key,
                     frozenTrustLine.getSerializer().peekData()},

                },
            .ledgerObjectCalls = 6,
            .mockedOffers = std::vector<xrpl::STObject>{gets10USDPays20XRPOffer},
            .expectedJson = fmt::format(
                R"JSON({{
                    "ledger_hash": "{}",
                    "ledger_index": 300,
                    "offers": [
                        {{
                            "Account": "{}",
                            "BookDirectory": "{}",
                            "BookNode": "0",
                            "Flags": 0,
                            "LedgerEntryType": "Offer",
                            "OwnerNode": "0",
                            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq": 0,
                            "Sequence": 0,
                            "TakerPays": "20",
                            "TakerGets": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "10"
                            }},
                            "index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds": "{}",
                            "quality": "{}",
                            "taker_gets_funded": {{
                                "currency": "USD",
                                "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value": "0"
                            }},
                            "taker_pays_funded": "0"
                        }}
                    ]
                }})JSON",
                kLedgerHash,
                kAccount2,
                kPayS20XrpGetS10UsdBookDir,
                0,
                2
            )
        }
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCBookOffersHandler,
    RPCBookOffersNormalPathTest,
    testing::ValuesIn(generateNormalPathBookOffersTestBundles()),
    tests::util::kNameGenerator
);

// ledger not exist
TEST_F(RPCBookOffersHandlerTest, LedgerNonExistViaIntSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(30, _))
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "ledger_index": 30,
                "taker_gets": {{
                    "currency": "XRP"
                }},
                "taker_pays": {{
                    "currency": "USD",
                    "issuer": "{}"
                }}
            }})JSON",
            kAccount
        )
    );
    auto const handler = AnyHandler{BookOffersHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(kInput, Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCBookOffersHandlerTest, LedgerNonExistViaSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(30, _))
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "ledger_index": "30",
                "taker_gets": {{
                    "currency": "XRP"
                }},
                "taker_pays": {{
                    "currency": "USD",
                    "issuer": "{}"
                }}
            }})JSON",
            kAccount
        )
    );
    auto const handler = AnyHandler{BookOffersHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(kInput, Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCBookOffersHandlerTest, LedgerNonExistViaHash)
{
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kLedgerHash}, _))
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "ledger_hash": "{}",
                "taker_gets": {{
                    "currency": "XRP"
                }},
                "taker_pays": {{
                    "currency": "USD",
                    "issuer": "{}"
                }}
            }})JSON",
            kLedgerHash,
            kAccount
        )
    );
    auto const handler = AnyHandler{BookOffersHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(kInput, Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCBookOffersHandlerTest, Limit)
{
    auto const seq = 300;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, seq);
    ON_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerHeader));

    auto const issuer = getAccountIdWithString(kAccount);
    // return valid book dir
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(1);

    auto const getsXRPPaysUSDBook = getBookBase(
        rpc::parseBook(
            xrpl::toCurrency("USD"), issuer, xrpl::xrpCurrency(), xrpl::xrpAccount(), std::nullopt
        )
            .value()
    );
    ON_CALL(*backend_, doFetchSuccessorKey(getsXRPPaysUSDBook, seq, _))
        .WillByDefault(Return(xrpl::uint256{kPayS20UsdGetS10XrpBookDir}));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(5);
    auto const indexes = std::vector<xrpl::uint256>(10, xrpl::uint256{kIndex2});

    ON_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kPayS20UsdGetS10XrpBookDir}, seq, _))
        .WillByDefault(
            Return(createOwnerDirLedgerObject(indexes, kIndex1).getSerializer().peekData())
        );
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, seq, _)
    )
        .WillByDefault(Return(
            createAccountRootObject(kAccount2, 0, 2, 200, 2, kIndex1, 2).getSerializer().peekData()
        ));

    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::feeSettings().key, seq, _))
        .WillByDefault(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::account(issuer).key, seq, _))
        .WillByDefault(
            Return(createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2, kTransferRateX2)
                       .getSerializer()
                       .peekData())
        );

    auto const gets10XRPPays20USDOffer = createOfferLedgerObject(
        kAccount2,
        10,
        20,
        xrpl::to_string(xrpl::xrpCurrency()),
        xrpl::to_string(xrpl::toCurrency("USD")),
        toBase58(xrpl::xrpAccount()),
        kAccount,
        kPayS20UsdGetS10XrpBookDir
    );

    std::vector<Blob> const bbs(10, gets10XRPPays20USDOffer.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "taker_gets": {{
                    "currency": "XRP"
                }},
                "taker_pays": {{
                    "currency": "USD",
                    "issuer": "{}"
                }},
                "limit": 5
            }})JSON",
            kAccount
        )
    );
    auto const handler = AnyHandler{BookOffersHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(kInput, Context{.yield = yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value().as_object().at("offers").as_array().size(), 5);
    });
}

TEST_F(RPCBookOffersHandlerTest, LimitMoreThanMax)
{
    auto const seq = 300;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, seq);
    ON_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerHeader));

    auto const issuer = getAccountIdWithString(kAccount);
    // return valid book dir
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(1);

    auto const getsXRPPaysUSDBook = getBookBase(
        rpc::parseBook(
            xrpl::toCurrency("USD"), issuer, xrpl::xrpCurrency(), xrpl::xrpAccount(), std::nullopt
        )
            .value()
    );
    ON_CALL(*backend_, doFetchSuccessorKey(getsXRPPaysUSDBook, seq, _))
        .WillByDefault(Return(xrpl::uint256{kPayS20UsdGetS10XrpBookDir}));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(5);
    auto const indexes =
        std::vector<xrpl::uint256>(BookOffersHandler::kLimitMax + 1, xrpl::uint256{kIndex2});

    ON_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kPayS20UsdGetS10XrpBookDir}, seq, _))
        .WillByDefault(
            Return(createOwnerDirLedgerObject(indexes, kIndex1).getSerializer().peekData())
        );
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, seq, _)
    )
        .WillByDefault(Return(
            createAccountRootObject(kAccount2, 0, 2, 200, 2, kIndex1, 2).getSerializer().peekData()
        ));

    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::feeSettings().key, seq, _))
        .WillByDefault(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::account(issuer).key, seq, _))
        .WillByDefault(
            Return(createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2, kTransferRateX2)
                       .getSerializer()
                       .peekData())
        );

    auto const gets10XRPPays20USDOffer = createOfferLedgerObject(
        kAccount2,
        10,
        20,
        xrpl::to_string(xrpl::xrpCurrency()),
        xrpl::to_string(xrpl::toCurrency("USD")),
        toBase58(xrpl::xrpAccount()),
        kAccount,
        kPayS20UsdGetS10XrpBookDir
    );

    std::vector<Blob> const bbs(
        BookOffersHandler::kLimitMax + 1, gets10XRPPays20USDOffer.getSerializer().peekData()
    );
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    static auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "taker_gets": {{
                    "currency": "XRP"
                }},
                "taker_pays": {{
                    "currency": "USD",
                    "issuer": "{}"
                }},
                "limit": {}
            }})JSON",
            kAccount,
            BookOffersHandler::kLimitMax + 1
        )
    );
    auto const handler = AnyHandler{BookOffersHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(kInput, Context{.yield = yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            output.result.value().as_object().at("offers").as_array().size(),
            BookOffersHandler::kLimitMax
        );
    });
}

// Standalone MPT test: verifies book_offers handler works with MPT taker_gets and empty book
TEST_F(RPCBookOffersHandlerTest, MPTGetsEmptyBook)
{
    constexpr auto kMptIssuanceId = "000004C463C52827307480341125DA0577DEFC38405DBADD";
    auto const seq = 300;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, seq);
    ON_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerHeader));

    // Compute MPT book base
    xrpl::MPTID mptid;
    [[maybe_unused]] auto const parsed = mptid.parseHex(kMptIssuanceId);
    xrpl::MPTIssue const mptIssue{mptid};
    xrpl::Asset const mptAsset{mptIssue};
    xrpl::Asset const xrpAsset{xrpl::xrpIssue()};
    auto const mptBook = rpc::parseBook(xrpAsset, mptAsset, std::nullopt).value();
    auto const mptBookBase = getBookBase(mptBook);

    // Return nullopt from successor → empty book
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(1);
    ON_CALL(*backend_, doFetchSuccessorKey(mptBookBase, seq, _))
        .WillByDefault(Return(std::optional<xrpl::uint256>{}));

    // Global-freeze and transfer-rate both look up the issuer's account (book.out.getIssuer()),
    // matching rippled's getBookPage; the two lookups hit the same issuer-account key. The XRP
    // side (taker_pays) returns immediately without a backend call.
    auto const mptIssuerAccountKey = xrpl::keylet::account(mptIssue.getIssuer()).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);
    ON_CALL(*backend_, doFetchLedgerObject(mptIssuerAccountKey, seq, _))
        .WillByDefault(Return(std::optional<xrpl::Blob>{}));

    // No offers in the book → doFetchLedgerObjects is NOT called
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(0);

    auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "taker_gets": {{
                    "mpt_issuance_id": "{}"
                }},
                "taker_pays": {{
                    "currency": "XRP"
                }}
            }})JSON",
            kMptIssuanceId
        )
    );

    auto const handler = AnyHandler{BookOffersHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(kInput, Context{.yield = yield});
        ASSERT_TRUE(output);
        auto const& result = output.result.value().as_object();
        EXPECT_EQ(result.at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_EQ(result.at("ledger_index").to_number<uint32_t>(), static_cast<uint32_t>(seq));
        EXPECT_TRUE(result.at("offers").as_array().empty());
    });
}

// Standalone MPT test: an offer selling MPT (taker_gets) for XRP (taker_pays) where the
// offer owner holds an MPToken balance. Exercises the MPT funding path in postProcessOrderBook:
// accountHoldsMPT reads the owner's sfMPTAmount and the multiply uses saTakerPays.asset().
// Global-freeze and transfer-rate follow rippled and look up the issuer's account (absent here,
// so no freeze and a parity rate).
TEST_F(RPCBookOffersHandlerTest, MPTGetsFundedOffer)
{
    constexpr auto kMptIssuanceId = "000004C463C52827307480341125DA0577DEFC38405DBADD";
    constexpr auto kMptBookDir = "0000000000000000000000000000000000000000000000005C09B7E04C9A0000";
    auto const seq = 300;
    auto const owner = getAccountIdWithString(kAccount2);

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, seq);
    ON_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerHeader));

    xrpl::MPTID mptid;
    [[maybe_unused]] auto const parsed = mptid.parseHex(kMptIssuanceId);
    xrpl::MPTIssue const mptIssue{mptid};
    xrpl::Asset const mptAsset{mptIssue};
    xrpl::Asset const xrpAsset{xrpl::xrpIssue()};
    auto const mptBook = rpc::parseBook(xrpAsset, mptAsset, std::nullopt).value();
    auto const mptBookBase = getBookBase(mptBook);

    // Build an offer: TakerGets = 10 MPT, TakerPays = 20 XRP, owned by kAccount2.
    xrpl::STObject offer(xrpl::sfLedgerEntry);
    offer.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltOFFER);
    offer.setAccountID(xrpl::sfAccount, owner);
    offer.setFieldU32(xrpl::sfSequence, 0);
    offer.setFieldU32(xrpl::sfFlags, 0);
    offer.setFieldAmount(xrpl::sfTakerGets, xrpl::STAmount(mptIssue, 10));
    offer.setFieldAmount(xrpl::sfTakerPays, xrpl::STAmount(20));
    offer.setFieldH256(xrpl::sfBookDirectory, xrpl::uint256{kMptBookDir});
    offer.setFieldU64(xrpl::sfBookNode, 0);
    offer.setFieldU64(xrpl::sfOwnerNode, 0);
    offer.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    offer.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);

    // Successor walk yields the book dir, then ends.
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(2);
    ON_CALL(*backend_, doFetchSuccessorKey(mptBookBase, seq, _))
        .WillByDefault(Return(xrpl::uint256{kMptBookDir}));
    ON_CALL(*backend_, doFetchSuccessorKey(xrpl::uint256{kMptBookDir}, seq, _))
        .WillByDefault(Return(std::optional<xrpl::uint256>{}));

    auto const mptIssuanceKey = xrpl::keylet::mptokenIssuance(mptid).key;
    auto const mptokenKey = xrpl::keylet::mptoken(mptid, owner).key;

    // MPTIssuance object (no transfer fee, not locked) - used for global freeze + transfer rate.
    auto const mptIssuanceObject = createMptIssuanceObject(kAccount, 2).getSerializer().peekData();
    // Owner holds 7 MPT (less than the 10 the offer sells -> partially funded).
    auto const mptokenObject = createMpTokenObject(kAccount2, mptid, 7).getSerializer().peekData();

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(testing::AtLeast(1));
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kMptBookDir}, seq, _))
        .WillByDefault(Return(
            createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1).getSerializer().peekData()
        ));
    ON_CALL(*backend_, doFetchLedgerObject(mptIssuanceKey, seq, _))
        .WillByDefault(Return(mptIssuanceObject));
    ON_CALL(*backend_, doFetchLedgerObject(mptokenKey, seq, _))
        .WillByDefault(Return(mptokenObject));

    std::vector<Blob> const bbs{offer.getSerializer().peekData()};
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "taker_gets": {{
                    "mpt_issuance_id": "{}"
                }},
                "taker_pays": {{
                    "currency": "XRP"
                }}
            }})JSON",
            kMptIssuanceId
        )
    );

    auto const handler = AnyHandler{BookOffersHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(kInput, Context{.yield = yield});
        ASSERT_TRUE(output);
        auto const& result = output.result.value().as_object();
        auto const& offers = result.at("offers").as_array();
        ASSERT_EQ(offers.size(), 1u);
        auto const& offerJson = offers.at(0).as_object();
        // Owner holds 7 MPT -> owner_funds reflects the MPToken balance.
        EXPECT_EQ(offerJson.at("owner_funds").as_string(), "7");
        // Offer sells 10 MPT but owner only has 7 -> taker_gets_funded is capped at 7.
        ASSERT_TRUE(offerJson.contains("taker_gets_funded"));
        EXPECT_EQ(offerJson.at("taker_gets_funded").as_object().at("value").as_string(), "7");
    });
}

// Standalone MPT test: an offer selling MPT whose issuance requires authorization, owned by a
// holder whose MPToken is NOT authorized. Mirrors rippled's getBookPage, which calls accountHolds
// with AuthHandling::ZeroIfUnauthorized: the unauthorized holder is treated as unfunded even though
// they carry a positive MPToken balance.
TEST_F(RPCBookOffersHandlerTest, MPTGetsUnauthorizedOfferUnfunded)
{
    constexpr auto kMptIssuanceId = "000004C463C52827307480341125DA0577DEFC38405DBADD";
    constexpr auto kMptBookDir = "0000000000000000000000000000000000000000000000005C09B7E04C9A0000";
    auto const seq = 300;
    auto const owner = getAccountIdWithString(kAccount2);

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, seq);
    ON_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerHeader));

    xrpl::MPTID mptid;
    [[maybe_unused]] auto const parsed = mptid.parseHex(kMptIssuanceId);
    xrpl::MPTIssue const mptIssue{mptid};
    xrpl::Asset const mptAsset{mptIssue};
    xrpl::Asset const xrpAsset{xrpl::xrpIssue()};
    auto const mptBook = rpc::parseBook(xrpAsset, mptAsset, std::nullopt).value();
    auto const mptBookBase = getBookBase(mptBook);

    // Offer: TakerGets = 10 MPT, TakerPays = 20 XRP, owned by kAccount2.
    xrpl::STObject offer(xrpl::sfLedgerEntry);
    offer.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltOFFER);
    offer.setAccountID(xrpl::sfAccount, owner);
    offer.setFieldU32(xrpl::sfSequence, 0);
    offer.setFieldU32(xrpl::sfFlags, 0);
    offer.setFieldAmount(xrpl::sfTakerGets, xrpl::STAmount(mptIssue, 10));
    offer.setFieldAmount(xrpl::sfTakerPays, xrpl::STAmount(20));
    offer.setFieldH256(xrpl::sfBookDirectory, xrpl::uint256{kMptBookDir});
    offer.setFieldU64(xrpl::sfBookNode, 0);
    offer.setFieldU64(xrpl::sfOwnerNode, 0);
    offer.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{});
    offer.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 0);

    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(2);
    ON_CALL(*backend_, doFetchSuccessorKey(mptBookBase, seq, _))
        .WillByDefault(Return(xrpl::uint256{kMptBookDir}));
    ON_CALL(*backend_, doFetchSuccessorKey(xrpl::uint256{kMptBookDir}, seq, _))
        .WillByDefault(Return(std::optional<xrpl::uint256>{}));

    auto const mptIssuanceKey = xrpl::keylet::mptokenIssuance(mptid).key;
    auto const mptokenKey = xrpl::keylet::mptoken(mptid, owner).key;

    // Issuance requires authorization; the owner's token holds 7 MPT but is NOT authorized.
    auto const mptIssuanceObject =
        createMptIssuanceObject(kAccount, 2, std::nullopt, xrpl::lsfMPTRequireAuth)
            .getSerializer()
            .peekData();
    auto const mptokenObject = createMpTokenObject(kAccount2, mptid, 7).getSerializer().peekData();

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(testing::AtLeast(1));
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kMptBookDir}, seq, _))
        .WillByDefault(Return(
            createOwnerDirLedgerObject({xrpl::uint256{kIndex2}}, kIndex1).getSerializer().peekData()
        ));
    ON_CALL(*backend_, doFetchLedgerObject(mptIssuanceKey, seq, _))
        .WillByDefault(Return(mptIssuanceObject));
    ON_CALL(*backend_, doFetchLedgerObject(mptokenKey, seq, _))
        .WillByDefault(Return(mptokenObject));

    std::vector<Blob> const bbs{offer.getSerializer().peekData()};
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const kInput = boost::json::parse(
        fmt::format(
            R"JSON({{
                "taker_gets": {{
                    "mpt_issuance_id": "{}"
                }},
                "taker_pays": {{
                    "currency": "XRP"
                }}
            }})JSON",
            kMptIssuanceId
        )
    );

    auto const handler = AnyHandler{BookOffersHandler{backend_, mockAmendmentCenterPtr_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(kInput, Context{.yield = yield});
        ASSERT_TRUE(output);
        auto const& result = output.result.value().as_object();
        auto const& offers = result.at("offers").as_array();
        ASSERT_EQ(offers.size(), 1u);
        auto const& offerJson = offers.at(0).as_object();
        // Unauthorized holder -> treated as unfunded.
        EXPECT_EQ(offerJson.at("owner_funds").as_string(), "0");
        ASSERT_TRUE(offerJson.contains("taker_gets_funded"));
        EXPECT_EQ(offerJson.at("taker_gets_funded").as_object().at("value").as_string(), "0");
    });
}
