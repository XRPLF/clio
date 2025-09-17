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

#include "rpc/Errors.hpp"
#include "rpc/common/MetaProcessors.hpp"
#include "rpc/common/Modifiers.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/ValidationHelpers.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/ErrorCodes.h>

#include <cstdint>
#include <string>
#include <string_view>

using namespace std;

using namespace rpc;
using namespace rpc::validation;
using namespace rpc::meta;
using namespace rpc::modifiers;

namespace json = boost::json;

class RPCBaseTest : public virtual ::testing::Test {};

TEST_F(RPCBaseTest, CheckType)
{
    auto const jstr = json::value("a string");
    ASSERT_TRUE(checkType<string>(jstr));
    ASSERT_FALSE(checkType<int>(jstr));

    auto const juint = json::value(123u);
    ASSERT_TRUE(checkType<uint32_t>(juint));
    ASSERT_TRUE(checkType<int32_t>(juint));
    ASSERT_FALSE(checkType<bool>(juint));

    auto jint = json::value(123);
    ASSERT_TRUE(checkType<int32_t>(jint));
    ASSERT_TRUE(checkType<uint32_t>(jint));
    ASSERT_FALSE(checkType<bool>(jint));

    jint = json::value(-123);
    ASSERT_TRUE(checkType<int32_t>(jint));
    ASSERT_FALSE(checkType<uint32_t>(jint));
    ASSERT_FALSE(checkType<bool>(jint));

    auto const jbool = json::value(true);
    ASSERT_TRUE(checkType<bool>(jbool));
    ASSERT_FALSE(checkType<int>(jbool));

    auto const jdouble = json::value(0.123);
    ASSERT_TRUE(checkType<double>(jdouble));
    ASSERT_TRUE(checkType<float>(jdouble));
    ASSERT_FALSE(checkType<bool>(jdouble));

    auto const jarr = json::value({1, 2, 3});
    ASSERT_TRUE(checkType<json::array>(jarr));
    ASSERT_FALSE(checkType<int>(jarr));
}

TEST_F(RPCBaseTest, TypeValidator)
{
    auto spec = RpcSpec{
        {"uint", Type<uint32_t>{}},
        {"int", Type<int32_t>{}},
        {"str", Type<string>{}},
        {"double", Type<double>{}},
        {"bool", Type<bool>{}},
        {"arr", Type<json::array>{}},
    };

    auto passingInput = json::parse(R"JSON({
        "uint": 123,
        "int": 321,
        "str": "a string",
        "double": 1.0,
        "bool": true,
        "arr": []
    })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    {
        auto failingInput = json::parse(R"JSON({ "uint": "a string" })JSON");
        ASSERT_FALSE(spec.process(failingInput));
    }
    {
        auto failingInput = json::parse(R"JSON({ "int": "a string" })JSON");
        ASSERT_FALSE(spec.process(failingInput));
    }
    {
        auto failingInput = json::parse(R"JSON({ "str": 1234 })JSON");
        ASSERT_FALSE(spec.process(failingInput));
    }
    {
        auto failingInput = json::parse(R"JSON({ "double": "a string" })JSON");
        ASSERT_FALSE(spec.process(failingInput));
    }
    {
        auto failingInput = json::parse(R"JSON({ "bool": "a string" })JSON");
        ASSERT_FALSE(spec.process(failingInput));
    }
    {
        auto failingInput = json::parse(R"JSON({ "arr": "a string" })JSON");
        ASSERT_FALSE(spec.process(failingInput));
    }
}

TEST_F(RPCBaseTest, TypeValidatorMultipleTypes)
{
    auto spec = RpcSpec{
        // either int or string
        {"test", Type<uint32_t, string>{}},
    };

    auto passingInput = json::parse(R"JSON({ "test": "1234" })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto passingInput2 = json::parse(R"JSON({ "test": 1234 })JSON");
    ASSERT_TRUE(spec.process(passingInput2));

    auto failingInput = json::parse(R"JSON({ "test": true })JSON");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, RequiredValidator)
{
    auto spec = RpcSpec{
        {"required", Required{}},
    };

    auto passingInput = json::parse(R"JSON({ "required": "present" })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto passingInput2 = json::parse(R"JSON({ "required": true })JSON");
    ASSERT_TRUE(spec.process(passingInput2));

    auto failingInput = json::parse(R"JSON({})JSON");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, BetweenValidator)
{
    auto spec = RpcSpec{
        {"amount", Between<uint32_t>{10u, 20u}},
    };

    auto passingInput = json::parse(R"JSON({ "amount": 15 })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto passingInput2 = json::parse(R"JSON({ "amount": 10 })JSON");
    ASSERT_TRUE(spec.process(passingInput2));

    auto passingInput3 = json::parse(R"JSON({ "amount": 20 })JSON");
    ASSERT_TRUE(spec.process(passingInput3));

    auto failingInput = json::parse(R"JSON({ "amount": 9 })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    auto failingInput2 = json::parse(R"JSON({ "amount": 21 })JSON");
    ASSERT_FALSE(spec.process(failingInput2));
}

TEST_F(RPCBaseTest, MinValidator)
{
    auto spec = RpcSpec{
        {"amount", Min{6}},
    };

    auto passingInput = json::parse(R"JSON({ "amount": 7 })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto passingInput2 = json::parse(R"JSON({ "amount": 6 })JSON");
    ASSERT_TRUE(spec.process(passingInput2));

    auto failingInput = json::parse(R"JSON({ "amount": 5 })JSON");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, MaxValidator)
{
    auto spec = RpcSpec{
        {"amount", Max{6}},
    };

    auto passingInput = json::parse(R"JSON({ "amount": 5 })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto passingInput2 = json::parse(R"JSON({ "amount": 6 })JSON");
    ASSERT_TRUE(spec.process(passingInput2));

    auto failingInput = json::parse(R"JSON({ "amount": 7 })JSON");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, OneOfValidator)
{
    auto spec = RpcSpec{
        {"currency", OneOf{"XRP", "USD"}},
    };

    auto passingInput = json::parse(R"JSON({ "currency": "XRP" })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto passingInput2 = json::parse(R"JSON({ "currency": "USD" })JSON");
    ASSERT_TRUE(spec.process(passingInput2));

    auto failingInput = json::parse(R"JSON({ "currency": "PRX" })JSON");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, EqualToValidator)
{
    auto spec = RpcSpec{
        {"exact", EqualTo{"CaseSensitive"}},
    };

    auto passingInput = json::parse(R"JSON({ "exact": "CaseSensitive" })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"JSON({ "exact": "Different" })JSON");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, ArrayAtValidator)
{
    auto spec = RpcSpec{
        {"arr",
         Required{},
         Type<json::array>{},
         ValidateArrayAt{
             0,
             {
                 {"limit", Required{}, Type<uint32_t>{}, Between<uint32_t>{0, 100}},
             }
         }},
        {"arr2",
         ValidateArrayAt{
             0,
             {
                 {"limit", Required{}, Type<uint32_t>{}, Between<uint32_t>{0, 100}},
             }
         }},
    };
    // clang-format on

    auto passingInput = json::parse(R"JSON({ "arr": [{"limit": 42}] })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"JSON({ "arr": [{"limit": "not int"}] })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"JSON({ "arr": [{"limit": 42}], "arr2": "not array type" })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"JSON({ "arr": [] })JSON");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, IfTypeValidator)
{
    auto spec = RpcSpec{
        {"mix",
         Required{},
         Type<std::string, json::object>{},
         IfType<json::object>{
             Section{{"limit", Required{}, Type<uint32_t>{}, Between<uint32_t>{0, 100}}},
             Section{{"limit2", Required{}, Type<uint32_t>{}, Between<uint32_t>{0, 100}}}
         },
         IfType<std::string>{CustomValidators::uint256HexStringValidator}},
        {"mix2",
         Section{{"limit", Required{}, Type<uint32_t>{}, Between<uint32_t>{0, 100}}},
         Type<std::string, json::object>{}},
    };

    // if json object pass
    auto passingInput = json::parse(R"JSON({ "mix": {"limit": 42, "limit2": 22} })JSON");
    ASSERT_TRUE(spec.process(passingInput));
    // if string pass
    passingInput =
        json::parse(R"JSON({ "mix": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC" })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    // if json object fail at first requirement
    auto failingInput = json::parse(R"JSON({ "mix": {"limit": "not int"} })JSON");
    ASSERT_FALSE(spec.process(failingInput));
    // if json object fail at second requirement
    failingInput = json::parse(R"JSON({ "mix": {"limit": 22, "limit2": "y"} })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    // if string fail
    failingInput = json::parse(R"JSON({ "mix": "not hash" })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    // type check fail
    failingInput = json::parse(R"JSON({ "mix": 1213 })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"JSON({ "mix": {"limit": 42, "limit2": 22}, "mix2": 1213 })JSON");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, WithCustomError)
{
    auto const spec = RpcSpec{
        {"transaction",
         WithCustomError{
             CustomValidators::uint256HexStringValidator, rpc::Status{ripple::rpcBAD_FEATURE, "MyCustomError"}
         }},
        {"other", WithCustomError{Type<std::string>{}, rpc::Status{ripple::rpcALREADY_MULTISIG, "MyCustomError2"}}}
    };

    auto passingInput = json::parse(
        R"JSON({ "transaction": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC", "other": "1"})JSON"
    );
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput =
        json::parse(R"JSON({ "transaction": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515B"})JSON");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "MyCustomError");
    ASSERT_EQ(err.error(), ripple::rpcBAD_FEATURE);

    failingInput = json::parse(R"JSON({ "other": 1})JSON");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "MyCustomError2");
    ASSERT_EQ(err.error(), ripple::rpcALREADY_MULTISIG);
}

TEST_F(RPCBaseTest, TimeFormatValidator)
{
    auto const spec = RpcSpec{
        {"date", TimeFormatValidator{"%Y-%m-%dT%H:%M:%SZ"}},
    };

    auto passingInput = json::parse(R"JSON({ "date": "2023-01-01T00:00:00Z" })JSON");
    EXPECT_TRUE(spec.process(passingInput));

    passingInput = json::parse("123");
    EXPECT_TRUE(spec.process(passingInput));

    // key not exists
    passingInput = json::parse(R"JSON({ "date1": "2023-01-01T00:00:00Z" })JSON");
    EXPECT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"JSON({ "date": "2023-01-01-00:00:00" })JSON");
    auto err = spec.process(failingInput);
    EXPECT_FALSE(err);
    EXPECT_EQ(err.error(), ripple::rpcINVALID_PARAMS);

    failingInput = json::parse(R"JSON({ "date": "01-01-2024T00:00:00" })JSON");
    EXPECT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"JSON({ "date": "2024-01-01T29:00:00" })JSON");
    EXPECT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"JSON({ "date": "" })JSON");
    EXPECT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"JSON({ "date": 1 })JSON");
    err = spec.process(failingInput);
    EXPECT_FALSE(err);
    EXPECT_EQ(err.error(), ripple::rpcINVALID_PARAMS);
}

TEST_F(RPCBaseTest, CustomValidator)
{
    auto customFormatCheck = CustomValidator{[](json::value const& value, std::string_view /* key */) -> MaybeError {
        return value.as_string().size() == 34 ? MaybeError{} : Error{rpc::Status{"Uh oh"}};
    }};

    auto spec = RpcSpec{
        {"taker", customFormatCheck},
    };

    auto passingInput = json::parse(R"JSON({ "taker": "r9cZA1mLK5R5Am25ArfXFmqgNwjZgnfk59" })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"JSON({ "taker": "wrongformat" })JSON");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, NotSupported)
{
    auto spec = RpcSpec{
        {"taker", Type<uint32_t>{}, NotSupported{123}},
        {"getter", NotSupported{}},
    };

    auto passingInput = json::parse(R"JSON({ "taker": 2 })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"JSON({ "taker": 123 })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"JSON({ "taker": 2, "getter": 2 })JSON");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, LedgerIndexValidator)
{
    auto spec = RpcSpec{
        {"ledgerIndex", CustomValidators::ledgerIndexValidator},
    };
    auto passingInput = json::parse(R"JSON({ "ledgerIndex": "validated" })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"JSON({ "ledgerIndex": "256" })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"JSON({ "ledgerIndex": 256 })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"JSON({ "ledgerIndex": "wrongformat" })JSON");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "ledgerIndexMalformed");

    failingInput = json::parse(R"JSON({ "ledgerIndex": true })JSON");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "ledgerIndexMalformed");
}

TEST_F(RPCBaseTest, AccountValidator)
{
    auto spec = RpcSpec{
        {"account", CustomValidators::accountValidator},
    };
    auto failingInput = json::parse(R"JSON({ "account": 256 })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp" })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput =
        json::parse(R"JSON({ "account": "02000000000000000000000000000000000000000000000000000000000000000" })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp?" })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    auto passingInput = json::parse(R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn" })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput =
        json::parse(R"JSON({ "account": "020000000000000000000000000000000000000000000000000000000000000000" })JSON");
    ASSERT_TRUE(spec.process(passingInput));
}

TEST_F(RPCBaseTest, AccountBase58Validator)
{
    auto spec = RpcSpec{
        {"account", CustomValidators::accountBase58Validator},
    };
    auto failingInput = json::parse(R"JSON({ "account": 256 })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp" })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput =
        json::parse(R"JSON({ "account": "020000000000000000000000000000000000000000000000000000000000000000" })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp?" })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    auto passingInput = json::parse(R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn" })JSON");
    ASSERT_TRUE(spec.process(passingInput));
}

TEST_F(RPCBaseTest, AccountMarkerValidator)
{
    auto spec = RpcSpec{
        {"marker", CustomValidators::accountMarkerValidator},
    };
    auto failingInput = json::parse(R"JSON({ "marker": 256 })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"JSON({ "marker": "testtest" })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"JSON({ "marker": "ABAB1234:1H" })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    auto passingInput = json::parse(R"JSON({ "account": "ABAB1234:123" })JSON");
    ASSERT_TRUE(spec.process(passingInput));
}

TEST_F(RPCBaseTest, Uint160HexStringValidator)
{
    auto const spec = RpcSpec{{"marker", CustomValidators::uint160HexStringValidator}};
    auto passingInput = json::parse(R"JSON({ "marker": "F609A18102218C75767209946A77523CBD97E225"})JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"JSON({ "marker": 160})JSON");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "markerNotString");

    failingInput = json::parse(R"JSON({ "marker": "F609A18102218C75767209946A77523CBD97E2253515BC"})JSON");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "markerMalformed");
}

TEST_F(RPCBaseTest, Uint192HexStringValidator)
{
    auto const spec = RpcSpec{{"mpt_issuance_id", CustomValidators::uint192HexStringValidator}};
    auto passingInput =
        json::parse(R"JSON({ "mpt_issuance_id": "0000012F27A9DE73EAA1E8831FA253E19030A17E2D038198"})JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"JSON({ "mpt_issuance_id": 192})JSON");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "mpt_issuance_idNotString");

    failingInput =
        json::parse(R"JSON({ "mpt_issuance_id": "0000012F27A9DE73EAA1E8831FA253E19030A17E2D038198983515BC"})JSON");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "mpt_issuance_idMalformed");
}

TEST_F(RPCBaseTest, Uint256HexStringValidator)
{
    auto const spec = RpcSpec{{"transaction", CustomValidators::uint256HexStringValidator}};
    auto passingInput =
        json::parse(R"JSON({ "transaction": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC"})JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"JSON({ "transaction": 256})JSON");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "transactionNotString");

    failingInput =
        json::parse(R"JSON({ "transaction": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC"})JSON");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "transactionMalformed");
}

TEST_F(RPCBaseTest, CurrencyValidator)
{
    auto const spec = RpcSpec{{"currency", CustomValidators::currencyValidator}};
    auto passingInput = json::parse(R"JSON({ "currency": "GBP"})JSON");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"JSON({ "currency": "0158415500000000C1F76FF6ECB0BAC600000000"})JSON");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"JSON({ "currency": "0158415500000000c1f76ff6ecb0bac600000000"})JSON");
    ASSERT_TRUE(spec.process(passingInput));

    for (auto const& currency : {"[]<", ">()", "{}|", "?!@", "#$%", "^&*"}) {
        passingInput = json::parse(fmt::format(R"JSON({{ "currency": "{}" }})JSON", currency));
        ASSERT_TRUE(spec.process(passingInput));
    }

    auto failingInput = json::parse(R"JSON({ "currency": 256})JSON");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "currencyNotString");

    failingInput = json::parse(R"JSON({ "currency": "12314"})JSON");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "malformedCurrency");
}

TEST_F(RPCBaseTest, IssuerValidator)
{
    auto const spec = RpcSpec{{"issuer", CustomValidators::issuerValidator}};
    auto passingInput = json::parse(R"JSON({ "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"})JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"JSON({ "issuer": 256})JSON");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "issuerNotString");

    failingInput = json::parse(fmt::format(R"JSON({{ "issuer": "{}"}})JSON", toBase58(ripple::noAccount())));
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
}

TEST_F(RPCBaseTest, SubscribeStreamValidator)
{
    auto const spec = RpcSpec{{"streams", CustomValidators::subscribeStreamValidator}};
    auto passingInput = json::parse(
        R"JSON({
            "streams": [
                "ledger",
                "transactions_proposed",
                "validations",
                "transactions",
                "manifests",
                "transactions",
                "book_changes"
            ]
        })JSON"
    );
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"JSON({ "streams": 256})JSON");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);

    failingInput = json::parse(R"JSON({ "streams": ["test"]})JSON");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);

    failingInput = json::parse(R"JSON({ "streams": [123]})JSON");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
}

TEST_F(RPCBaseTest, SubscribeAccountsValidator)
{
    auto const spec = RpcSpec{{"accounts", CustomValidators::subscribeAccountsValidator}};
    auto passingInput = json::parse(
        R"JSON({ "accounts": ["rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"]})JSON"
    );
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"JSON({ "accounts": 256})JSON");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);

    failingInput = json::parse(R"JSON({ "accounts": ["test"]})JSON");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);

    failingInput = json::parse(R"JSON({ "accounts": [123]})JSON");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
}

TEST_F(RPCBaseTest, ClampingModifier)
{
    auto spec = RpcSpec{
        {"amount", Clamp<uint32_t>{10u, 20u}},
    };

    auto passingInput = json::parse(R"JSON({ "amount": 15 })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    auto passingInput2 = json::parse(R"JSON({ "amount": 5 })JSON");
    ASSERT_TRUE(spec.process(passingInput2));
    ASSERT_EQ(passingInput2.at("amount").as_uint64(), 10u);  // clamped

    auto passingInput3 = json::parse(R"JSON({ "amount": 25 })JSON");
    ASSERT_TRUE(spec.process(passingInput3));
    ASSERT_EQ(passingInput3.at("amount").as_uint64(), 20u);  // clamped
}

TEST_F(RPCBaseTest, ToLowerModifier)
{
    auto spec = RpcSpec{
        {"str", ToLower{}},
    };

    auto passingInput = json::parse(R"JSON({ "str": "TesT" })JSON");
    ASSERT_TRUE(spec.process(passingInput));
    ASSERT_EQ(passingInput.at("str").as_string(), "test");

    auto passingInput2 = json::parse(R"JSON({ "str2": "TesT" })JSON");
    ASSERT_TRUE(spec.process(passingInput2));  // no str no problem

    auto passingInput3 = json::parse(R"JSON({ "str": "already lower case" })JSON");
    ASSERT_TRUE(spec.process(passingInput3));
    ASSERT_EQ(passingInput3.at("str").as_string(), "already lower case");

    auto passingInput4 = json::parse(R"JSON({ "str": "" })JSON");
    ASSERT_TRUE(spec.process(passingInput4));  // empty str no problem
    ASSERT_EQ(passingInput4.at("str").as_string(), "");
}

TEST_F(RPCBaseTest, ToNumberModifier)
{
    auto const spec = RpcSpec{
        {"str", ToNumber{}},
    };

    auto passingInput = json::parse(R"JSON({ "str": [] })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"JSON({ "str2": "TesT" })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"JSON([])JSON");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"JSON({ "str": "123" })JSON");
    ASSERT_TRUE(spec.process(passingInput));
    ASSERT_EQ(passingInput.at("str").as_int64(), 123);

    auto failingInput = json::parse(R"JSON({ "str": "ok" })JSON");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"JSON({ "str": "123.123" })JSON");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, CustomModifier)
{
    testing::StrictMock<testing::MockFunction<MaybeError(json::value & value, std::string_view)>> mockModifier;
    auto const customModifier = CustomModifier{mockModifier.AsStdFunction()};
    auto const spec = RpcSpec{
        {"str", customModifier},
    };

    EXPECT_CALL(mockModifier, Call).WillOnce(testing::Return(MaybeError{}));
    auto passingInput = json::parse(R"JSON({ "str": "sss" })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"JSON({ "strNotExist": 123 })JSON");
    ASSERT_TRUE(spec.process(passingInput));

    // not a json object
    passingInput = json::parse(R"JSON([])JSON");
    ASSERT_TRUE(spec.process(passingInput));
}
