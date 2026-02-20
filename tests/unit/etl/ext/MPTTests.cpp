//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "etl/Models.hpp"
#include "etl/impl/ext/MPT.hpp"
#include "rpc/RPCHelpers.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/TxFormats.h>

#include <algorithm>
#include <utility>
#include <vector>

using namespace etl;
using namespace etl::impl;
using namespace data;
using namespace testing;

namespace {

constinit auto const kSEQ = 123u;
constinit auto const kLEDGER_HASH =
    "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constinit auto const kHOLDER_ACCOUNT = "rK1EX542EgA9m948JrJRaEzwLVEhqWvnr9";

constinit auto const kTXN_HEX =
    "120039220000000024002DBD1A201B002DBDA36840000000000000017321EDECF25C029811CAD07AFD616EB75E3803"
    "E44D0D59A6826AC25FE3"
    "4A43626D2D157440244262E760314164843026CE2F100D0BFEB0DD6F75026FEB3F75FCAA943F5C874FF0411BC82A85"
    "DE504B434B5EC3C6A692"
    "3CC37A1C2ABD3E98EFFC8240B9D0018114CEF330DB51154D8DEE249CC3D6DFD04B91F648EE0115002DBD1817E0AF9F"
    "DE4F9978B8FCD8A50636"
    "30B5737DA605";

constinit auto const kTXN_META =
    "201C00000002F8E311007F562668E165750018E0AE5808C131BAF4C26441D2BCF76F8628774DFDF098B7250BE88114"
    "CEF330DB51154D8DEE24"
    "9CC3D6DFD04B91F648EE0115002DBD1817E0AF9FDE4F9978B8FCD8A5063630B5737DA605E1E1E511006425002DBD2F"
    "55E85C182A243C7CBF0E"
    "F7B8B3E0C8AE68E3DE6616DE1EFE168CD913CA6520444D568F18252475DFAC9D5DE5423DFA08842F398F346DEB2BD5"
    "46C526D26BF81E345CE7"
    "2200000000588F18252475DFAC9D5DE5423DFA08842F398F346DEB2BD546C526D26BF81E345C8214CEF330DB51154D"
    "8DEE249CC3D6DFD04B91"
    "F648EEE1E1E511006125002DBD2F55E85C182A243C7CBF0EF7B8B3E0C8AE68E3DE6616DE1EFE168CD913CA6520444D"
    "56F7D3073515F1C71F2A"
    "D00941BA714A3FBE3D91AEAFCD6345B5389004AD707E95E624002DBD1A2D00000001624000000005F5E0FFE1E72200"
    "00000024002DBD1B2D00"
    "000002624000000005F5E0FE8114CEF330DB51154D8DEE249CC3D6DFD04B91F648EEE1E1F1031000";

constinit auto const kHASH = "6005B465CBBF7FA8E41AC0C0CD38491026D9411FCB7BA46E2AEBB3AF7654261B";
constinit auto const kHASH2 = "6005B465CBBF7FA8E41AC0C0CD38491026D9411FCB7BA46E2AEBB3AF7654261C";
constinit auto const kHASH3 = "6005B465CBBF7FA8E41AC0C0CD38491026D9411FCB7BA46E2AEBB3AF7654261D";

auto
createTestData()
{
    auto transactions = std::vector{
        util::createTransaction(
            ripple::TxType::ttMPTOKEN_ISSUANCE_CREATE
        ),  // not AUTHORIZE so will not be written
        util::createTransaction(ripple::TxType::ttMPTOKEN_AUTHORIZE, kHASH, kTXN_META, kTXN_HEX),
        util::createTransaction(ripple::TxType::ttAMM_CREATE),  // not MPT - will be filtered
        util::createTransaction(
            ripple::TxType::ttMPTOKEN_ISSUANCE_CREATE
        ),  // not unique - will be filtered
    };

    auto const header = createLedgerHeader(kLEDGER_HASH, kSEQ);
    return etl::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = kSEQ
    };
}

auto
createMultipleHoldersTestData()
{
    auto transactions = std::vector{
        util::createTransaction(ripple::TxType::ttMPTOKEN_AUTHORIZE, kHASH, kTXN_META, kTXN_HEX),
        util::createTransaction(ripple::TxType::ttMPTOKEN_AUTHORIZE, kHASH2, kTXN_META, kTXN_HEX),
        util::createTransaction(ripple::TxType::ttMPTOKEN_AUTHORIZE, kHASH3, kTXN_META, kTXN_HEX)
    };

    auto const header = createLedgerHeader(kLEDGER_HASH, kSEQ);
    return etl::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = kSEQ
    };
}

}  // namespace

struct MPTExtTests : util::prometheus::WithPrometheus, MockBackendTest {
protected:
    MPTExt ext_{backend_};
};

TEST_F(MPTExtTests, OnLedgerDataFiltersAndWritesMPTs)
{
    auto const data = createTestData();

    EXPECT_CALL(*backend_, writeMPTHolders).WillOnce([](auto const& holders) {
        EXPECT_EQ(holders.size(), 1);  // Only AUTHORIZE is written in the end
    });

    ext_.onLedgerData(data);
}

TEST_F(MPTExtTests, OnInitialDataFiltersAndWritesMPTs)
{
    auto const data = createTestData();

    EXPECT_CALL(*backend_, writeMPTHolders).WillOnce([](auto const& holders) {
        EXPECT_EQ(holders.size(), 1);  // Only AUTHORIZE is written in the end
    });

    ext_.onInitialData(data);
}

TEST_F(MPTExtTests, OnInitialObjectWritesMPT)
{
    auto const data = util::createObjectWithMPT();

    EXPECT_CALL(*backend_, writeMPTHolders).WillOnce([](auto const& holders) {
        EXPECT_EQ(holders.size(), 1);
    });

    ext_.onInitialObject(kSEQ, data);
}

TEST_F(MPTExtTests, OnInitialDataWithMultipleHolders)
{
    auto const data = createMultipleHoldersTestData();

    EXPECT_CALL(*backend_, writeMPTHolders).WillOnce([](auto const& holders) {
        EXPECT_EQ(holders.size(), 3);  // Expect all three AUTHORIZE transactions

        auto const expectedAccount =
            rpc::accountFromStringStrict(kHOLDER_ACCOUNT);  // Expect all three to be the same
        EXPECT_TRUE(std::ranges::all_of(holders, [&expectedAccount](auto const& data) {
            return data.holder == expectedAccount;
        }));
    });

    ext_.onInitialData(data);
}
