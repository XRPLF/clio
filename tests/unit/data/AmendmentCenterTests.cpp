#include "data/AmendmentCenter.hpp"
#include "data/Types.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockAssert.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

using namespace data;

constexpr auto kSeq = 30u;

struct AmendmentCenterTest : util::prometheus::WithPrometheus,
                             MockBackendTest,
                             SyncAsioContextTest {
    AmendmentCenter amendmentCenter{backend_};
};

// This is a safety net test that will fail anytime we built Clio against a new libXRPL that added
// some Amendment that we forgot to register in data::Amendments.
TEST_F(AmendmentCenterTest, AllAmendmentsFromLibXRPLAreSupported)
{
    for (auto const& [name, _] : xrpl::allAmendments()) {
        EXPECT_TRUE(amendmentCenter.isSupported(name))
            << "XRPL amendment not supported by Clio: " << name;
    }

    // We support at least all the amendments currently exposed by libXRPL
    ASSERT_GE(amendmentCenter.getSupported().size(), xrpl::allAmendments().size());
    ASSERT_GE(amendmentCenter.getAll().size(), xrpl::allAmendments().size());
}

TEST_F(AmendmentCenterTest, Accessors)
{
    {
        auto const am = amendmentCenter.getAmendment("DisallowIncoming");
        EXPECT_EQ(
            am.feature,
            xrpl::uint256("47C3002ABA31628447E8E9A8B315FAA935CE30183F9A9B86845E469CA2CDC3DF")
        );
    }
    {
        auto const am = amendmentCenter["DisallowIncoming"];
        EXPECT_EQ(
            am.feature,
            xrpl::uint256("47C3002ABA31628447E8E9A8B315FAA935CE30183F9A9B86845E469CA2CDC3DF")
        );
    }

    auto const a = amendmentCenter[Amendments::Flow];
    auto const b = amendmentCenter["Flow"];
    EXPECT_EQ(a, b);
}

TEST_F(AmendmentCenterTest, IsEnabled)
{
    EXPECT_TRUE(amendmentCenter.isSupported("fixUniversalNumber"));
    EXPECT_FALSE(amendmentCenter.isSupported("unknown"));

    auto const amendments = createAmendmentsObject({Amendments::fixUniversalNumber});
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::amendments().key, kSeq, testing::_))
        .WillRepeatedly(testing::Return(amendments.getSerializer().peekData()));

    EXPECT_TRUE(amendmentCenter.isEnabled("fixUniversalNumber", kSeq));
    EXPECT_FALSE(amendmentCenter.isEnabled("unknown", kSeq));
    EXPECT_FALSE(amendmentCenter.isEnabled("ImmediateOfferKilled", kSeq));
}

TEST_F(AmendmentCenterTest, IsMultipleEnabled)
{
    auto const amendments = createAmendmentsObject({Amendments::fixUniversalNumber});
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::amendments().key, kSeq, testing::_))
        .WillOnce(testing::Return(amendments.getSerializer().peekData()));

    runSpawn([this](auto yield) {
        std::vector<data::AmendmentKey> const keys{
            "fixUniversalNumber", "unknown", "ImmediateOfferKilled"
        };
        auto const result = amendmentCenter.isEnabled(yield, keys, kSeq);

        EXPECT_EQ(result.size(), keys.size());
        EXPECT_TRUE(result.at(0));
        EXPECT_FALSE(result.at(1));
        EXPECT_FALSE(result.at(2));
    });
}

TEST_F(AmendmentCenterTest, IsEnabledReturnsFalseWhenAmendmentsLedgerObjectUnavailable)
{
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::amendments().key, kSeq, testing::_))
        .WillOnce(testing::Return(std::nullopt));

    runSpawn([this](auto yield) {
        EXPECT_NO_THROW(EXPECT_FALSE(amendmentCenter.isEnabled(yield, "irrelevant", kSeq)));
    });
}

TEST_F(AmendmentCenterTest, IsEnabledReturnsFalseWhenNoAmendments)
{
    auto const amendments = createBrokenAmendmentsObject();
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::amendments().key, kSeq, testing::_))
        .WillOnce(testing::Return(amendments.getSerializer().peekData()));

    runSpawn([this](auto yield) {
        EXPECT_FALSE(amendmentCenter.isEnabled(yield, "irrelevant", kSeq));
    });
}

TEST_F(AmendmentCenterTest, IsEnabledReturnsVectorOfFalseWhenAmendmentsLedgerObjectUnavailable)
{
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::amendments().key, kSeq, testing::_))
        .WillOnce(testing::Return(std::nullopt));

    runSpawn([this](auto yield) {
        std::vector<data::AmendmentKey> const keys{"fixUniversalNumber", "ImmediateOfferKilled"};
        std::vector<bool> vec;
        EXPECT_NO_THROW(vec = amendmentCenter.isEnabled(yield, keys, kSeq));

        EXPECT_EQ(vec.size(), keys.size());
        EXPECT_TRUE(std::ranges::all_of(vec, std::logical_not<>{}));
    });
}

TEST_F(AmendmentCenterTest, IsEnabledReturnsVectorOfFalseWhenNoAmendments)
{
    auto const amendments = createBrokenAmendmentsObject();
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::amendments().key, kSeq, testing::_))
        .WillOnce(testing::Return(amendments.getSerializer().peekData()));

    runSpawn([this](auto yield) {
        std::vector<data::AmendmentKey> const keys{"fixUniversalNumber", "ImmediateOfferKilled"};
        auto const vec = amendmentCenter.isEnabled(yield, keys, kSeq);

        EXPECT_EQ(vec.size(), keys.size());
        EXPECT_TRUE(std::ranges::all_of(vec, [](bool val) { return val == false; }));
    });
}

TEST_F(AmendmentCenterTest, DeletedLibXRPLAmendmentIsNotKnownToLibXRPL)
{
    // OwnerPaysFee was removed from libXRPL in 2.6.0; confirm it's not present upstream
    EXPECT_FALSE(xrpl::allAmendments().contains(std::string{Amendments::OwnerPaysFee}));
}

TEST_F(AmendmentCenterTest, DeletedLibXRPLAmendmentIsPresentInGetAllWithCorrectFlags)
{
    auto const& all = amendmentCenter.getAll();
    auto const it = std::ranges::find(all, std::string{Amendments::OwnerPaysFee}, &Amendment::name);

    ASSERT_NE(
        it, all.end()
    ) << "OwnerPaysFee must be present in getAll() even after libXRPL deleted it";
    EXPECT_FALSE(it->isSupportedByXRPL);
    EXPECT_TRUE(it->isSupportedByClio);
    EXPECT_TRUE(it->isRetired);
}

TEST_F(AmendmentCenterTest, DeletedLibXRPLAmendmentIsSupportedByClio)
{
    // Clio still registers OwnerPaysFee so isSupported() and getSupported() must include it
    EXPECT_TRUE(amendmentCenter.isSupported(Amendments::OwnerPaysFee));
    EXPECT_TRUE(amendmentCenter.getSupported().contains(std::string{Amendments::OwnerPaysFee}));
}

TEST(AmendmentTest, GenerateAmendmentId)
{
    // https://xrpl.org/known-amendments.html#disallowincoming refer to the published id
    EXPECT_EQ(
        xrpl::uint256("47C3002ABA31628447E8E9A8B315FAA935CE30183F9A9B86845E469CA2CDC3DF"),
        Amendment::getAmendmentId("DisallowIncoming")
    );
}

struct AmendmentCenterAssertTest : common::util::WithMockAssert, AmendmentCenterTest {};

TEST_F(AmendmentCenterAssertTest, GetInvalidAmendmentAsserts)
{
    EXPECT_CLIO_ASSERT_FAIL({
        [[maybe_unused]] auto _ = amendmentCenter.getAmendment("invalidAmendmentKey");
    });
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto _ = amendmentCenter["invalidAmendmentKey"]; });
}

struct AmendmentKeyTest : testing::Test {};

TEST_F(AmendmentKeyTest, Convertible)
{
    std::string const key1 = "key1";
    auto key2 = "key2";

    EXPECT_NO_THROW({
        auto const first = AmendmentKey(key1);
        auto const second = AmendmentKey(key2);
        auto const third = AmendmentKey("test");

        std::string const s1 = first;
        EXPECT_EQ(s1, key1);

        xrpl::uint256 const k1 = first;
        xrpl::uint256 const k2 = second;

        EXPECT_EQ(
            k1, xrpl::uint256{"7E365F775657DC0EB960E6295A1F44B3F67479F54D5D12C5D87E6DB234F072E3"}
        );
        EXPECT_EQ(
            k2, xrpl::uint256{"B4F33541E0E2FC2F7AA17D2D2E6A9B424809123485251D3413E91CC462309772"}
        );
    });
}

TEST_F(AmendmentKeyTest, Comparison)
{
    auto const first = AmendmentKey("1");
    auto const second = AmendmentKey("2");
    EXPECT_GT(second, first);
}
