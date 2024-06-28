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
#include "util/LoggerFixtures.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>
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

class RPCBaseTest : public NoLoggerFixture {};

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

    auto passingInput = json::parse(R"({ 
        "uint": 123,
        "int": 321,
        "str": "a string",
        "double": 1.0,
        "bool": true,
        "arr": []
    })");
    ASSERT_TRUE(spec.process(passingInput));

    {
        auto failingInput = json::parse(R"({ "uint": "a string" })");
        ASSERT_FALSE(spec.process(failingInput));
    }
    {
        auto failingInput = json::parse(R"({ "int": "a string" })");
        ASSERT_FALSE(spec.process(failingInput));
    }
    {
        auto failingInput = json::parse(R"({ "str": 1234 })");
        ASSERT_FALSE(spec.process(failingInput));
    }
    {
        auto failingInput = json::parse(R"({ "double": "a string" })");
        ASSERT_FALSE(spec.process(failingInput));
    }
    {
        auto failingInput = json::parse(R"({ "bool": "a string" })");
        ASSERT_FALSE(spec.process(failingInput));
    }
    {
        auto failingInput = json::parse(R"({ "arr": "a string" })");
        ASSERT_FALSE(spec.process(failingInput));
    }
}

TEST_F(RPCBaseTest, TypeValidatorMultipleTypes)
{
    auto spec = RpcSpec{
        // either int or string
        {"test", Type<uint32_t, string>{}},
    };

    auto passingInput = json::parse(R"({ "test": "1234" })");
    ASSERT_TRUE(spec.process(passingInput));

    auto passingInput2 = json::parse(R"({ "test": 1234 })");
    ASSERT_TRUE(spec.process(passingInput2));

    auto failingInput = json::parse(R"({ "test": true })");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, RequiredValidator)
{
    auto spec = RpcSpec{
        {"required", Required{}},
    };

    auto passingInput = json::parse(R"({ "required": "present" })");
    ASSERT_TRUE(spec.process(passingInput));

    auto passingInput2 = json::parse(R"({ "required": true })");
    ASSERT_TRUE(spec.process(passingInput2));

    auto failingInput = json::parse(R"({})");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, BetweenValidator)
{
    auto spec = RpcSpec{
        {"amount", Between<uint32_t>{10u, 20u}},
    };

    auto passingInput = json::parse(R"({ "amount": 15 })");
    ASSERT_TRUE(spec.process(passingInput));

    auto passingInput2 = json::parse(R"({ "amount": 10 })");
    ASSERT_TRUE(spec.process(passingInput2));

    auto passingInput3 = json::parse(R"({ "amount": 20 })");
    ASSERT_TRUE(spec.process(passingInput3));

    auto failingInput = json::parse(R"({ "amount": 9 })");
    ASSERT_FALSE(spec.process(failingInput));

    auto failingInput2 = json::parse(R"({ "amount": 21 })");
    ASSERT_FALSE(spec.process(failingInput2));
}

TEST_F(RPCBaseTest, MinValidator)
{
    auto spec = RpcSpec{
        {"amount", Min{6}},
    };

    auto passingInput = json::parse(R"({ "amount": 7 })");
    ASSERT_TRUE(spec.process(passingInput));

    auto passingInput2 = json::parse(R"({ "amount": 6 })");
    ASSERT_TRUE(spec.process(passingInput2));

    auto failingInput = json::parse(R"({ "amount": 5 })");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, MaxValidator)
{
    auto spec = RpcSpec{
        {"amount", Max{6}},
    };

    auto passingInput = json::parse(R"({ "amount": 5 })");
    ASSERT_TRUE(spec.process(passingInput));

    auto passingInput2 = json::parse(R"({ "amount": 6 })");
    ASSERT_TRUE(spec.process(passingInput2));

    auto failingInput = json::parse(R"({ "amount": 7 })");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, OneOfValidator)
{
    auto spec = RpcSpec{
        {"currency", OneOf{"XRP", "USD"}},
    };

    auto passingInput = json::parse(R"({ "currency": "XRP" })");
    ASSERT_TRUE(spec.process(passingInput));

    auto passingInput2 = json::parse(R"({ "currency": "USD" })");
    ASSERT_TRUE(spec.process(passingInput2));

    auto failingInput = json::parse(R"({ "currency": "PRX" })");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, EqualToValidator)
{
    auto spec = RpcSpec{
        {"exact", EqualTo{"CaseSensitive"}},
    };

    auto passingInput = json::parse(R"({ "exact": "CaseSensitive" })");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"({ "exact": "Different" })");
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

    auto passingInput = json::parse(R"({ "arr": [{"limit": 42}] })");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"({ "arr": [{"limit": "not int"}] })");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"({ "arr": [{"limit": 42}] ,"arr2": "not array type" })");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"({ "arr": [] })");
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
         IfType<std::string>{CustomValidators::Uint256HexStringValidator}},
        {"mix2",
         Section{{"limit", Required{}, Type<uint32_t>{}, Between<uint32_t>{0, 100}}},
         Type<std::string, json::object>{}},
    };

    // if json object pass
    auto passingInput = json::parse(R"({ "mix": {"limit": 42, "limit2": 22} })");
    ASSERT_TRUE(spec.process(passingInput));
    // if string pass
    passingInput = json::parse(R"({ "mix": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC" })");
    ASSERT_TRUE(spec.process(passingInput));

    // if json object fail at first requirement
    auto failingInput = json::parse(R"({ "mix": {"limit": "not int"} })");
    ASSERT_FALSE(spec.process(failingInput));
    // if json object fail at second requirement
    failingInput = json::parse(R"({ "mix": {"limit": 22, "limit2": "y"} })");
    ASSERT_FALSE(spec.process(failingInput));

    // if string fail
    failingInput = json::parse(R"({ "mix": "not hash" })");
    ASSERT_FALSE(spec.process(failingInput));

    // type check fail
    failingInput = json::parse(R"({ "mix": 1213 })");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"({ "mix": {"limit": 42, "limit2": 22} , "mix2": 1213 })");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, WithCustomError)
{
    auto const spec = RpcSpec{
        {"transaction",
         WithCustomError{
             CustomValidators::Uint256HexStringValidator, rpc::Status{ripple::rpcBAD_FEATURE, "MyCustomError"}
         }},
        {"other", WithCustomError{Type<std::string>{}, rpc::Status{ripple::rpcALREADY_MULTISIG, "MyCustomError2"}}}
    };

    auto passingInput = json::parse(
        R"({ "transaction": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC", "other": "1"})"
    );
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput =
        json::parse(R"({ "transaction": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515B"})");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "MyCustomError");
    ASSERT_EQ(err.error(), ripple::rpcBAD_FEATURE);

    failingInput = json::parse(R"({ "other": 1})");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "MyCustomError2");
    ASSERT_EQ(err.error(), ripple::rpcALREADY_MULTISIG);
}

TEST_F(RPCBaseTest, CustomValidator)
{
    auto customFormatCheck = CustomValidator{[](json::value const& value, std::string_view /* key */) -> MaybeError {
        return value.as_string().size() == 34 ? MaybeError{} : Error{rpc::Status{"Uh oh"}};
    }};

    auto spec = RpcSpec{
        {"taker", customFormatCheck},
    };

    auto passingInput = json::parse(R"({ "taker": "r9cZA1mLK5R5Am25ArfXFmqgNwjZgnfk59" })");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"({ "taker": "wrongformat" })");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, NotSupported)
{
    auto spec = RpcSpec{
        {"taker", Type<uint32_t>{}, NotSupported{123}},
        {"getter", NotSupported{}},
    };

    auto passingInput = json::parse(R"({ "taker": 2 })");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"({ "taker": 123 })");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"({ "taker": 2, "getter": 2 })");
    ASSERT_FALSE(spec.process(failingInput));
}

TEST_F(RPCBaseTest, LedgerIndexValidator)
{
    auto spec = RpcSpec{
        {"ledgerIndex", CustomValidators::LedgerIndexValidator},
    };
    auto passingInput = json::parse(R"({ "ledgerIndex": "validated" })");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"({ "ledgerIndex": "256" })");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"({ "ledgerIndex": 256 })");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"({ "ledgerIndex": "wrongformat" })");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "ledgerIndexMalformed");

    failingInput = json::parse(R"({ "ledgerIndex": true })");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "ledgerIndexMalformed");
}

TEST_F(RPCBaseTest, AccountValidator)
{
    auto spec = RpcSpec{
        {"account", CustomValidators::AccountValidator},
    };
    auto failingInput = json::parse(R"({ "account": 256 })");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp" })");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"({ "account": "02000000000000000000000000000000000000000000000000000000000000000" })");
    ASSERT_FALSE(spec.process(failingInput));

    auto passingInput = json::parse(R"({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn" })");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput =
        json::parse(R"({ "account": "020000000000000000000000000000000000000000000000000000000000000000" })");
    ASSERT_TRUE(spec.process(passingInput));
}

TEST_F(RPCBaseTest, AccountMarkerValidator)
{
    auto spec = RpcSpec{
        {"marker", CustomValidators::AccountMarkerValidator},
    };
    auto failingInput = json::parse(R"({ "marker": 256 })");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"({ "marker": "testtest" })");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"({ "marker": "ABAB1234:1H" })");
    ASSERT_FALSE(spec.process(failingInput));

    auto passingInput = json::parse(R"({ "account": "ABAB1234:123" })");
    ASSERT_TRUE(spec.process(passingInput));
}

TEST_F(RPCBaseTest, Uint256HexStringValidator)
{
    auto const spec = RpcSpec{{"transaction", CustomValidators::Uint256HexStringValidator}};
    auto passingInput =
        json::parse(R"({ "transaction": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC"})");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"({ "transaction": 256})");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "transactionNotString");

    failingInput = json::parse(R"({ "transaction": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC"})");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "transactionMalformed");
}

TEST_F(RPCBaseTest, CurrencyValidator)
{
    auto const spec = RpcSpec{{"currency", CustomValidators::CurrencyValidator}};
    auto passingInput = json::parse(R"({ "currency": "GBP"})");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"({ "currency": "0158415500000000C1F76FF6ECB0BAC600000000"})");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"({ "currency": "0158415500000000c1f76ff6ecb0bac600000000"})");
    ASSERT_TRUE(spec.process(passingInput));

    for (auto const& currency : {"[]<", ">()", "{}|", "?!@", "#$%", "^&*"}) {
        passingInput = json::parse(fmt::format(R"({{ "currency" : "{}" }})", currency));
        ASSERT_TRUE(spec.process(passingInput));
    }

    auto failingInput = json::parse(R"({ "currency": 256})");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "currencyNotString");

    failingInput = json::parse(R"({ "currency": "12314"})");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "malformedCurrency");
}

TEST_F(RPCBaseTest, IssuerValidator)
{
    auto const spec = RpcSpec{{"issuer", CustomValidators::IssuerValidator}};
    auto passingInput = json::parse(R"({ "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"})");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"({ "issuer": 256})");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);
    ASSERT_EQ(err.error().message, "issuerNotString");

    failingInput = json::parse(fmt::format(R"({{ "issuer": "{}"}})", toBase58(ripple::noAccount())));
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
}

TEST_F(RPCBaseTest, SubscribeStreamValidator)
{
    auto const spec = RpcSpec{{"streams", CustomValidators::SubscribeStreamValidator}};
    auto passingInput = json::parse(
        R"({ 
            "streams": 
            [
                "ledger", 
                "transactions_proposed",
                "validations",
                "transactions",
                "manifests",
                "transactions",
                "book_changes"
            ]
        })"
    );
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"({ "streams": 256})");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);

    failingInput = json::parse(R"({ "streams": ["test"]})");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);

    failingInput = json::parse(R"({ "streams": [123]})");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
}

TEST_F(RPCBaseTest, SubscribeAccountsValidator)
{
    auto const spec = RpcSpec{{"accounts", CustomValidators::SubscribeAccountsValidator}};
    auto passingInput =
        json::parse(R"({ "accounts": ["rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn","rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"]})");
    ASSERT_TRUE(spec.process(passingInput));

    auto failingInput = json::parse(R"({ "accounts": 256})");
    auto err = spec.process(failingInput);
    ASSERT_FALSE(err);

    failingInput = json::parse(R"({ "accounts": ["test"]})");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);

    failingInput = json::parse(R"({ "accounts": [123]})");
    err = spec.process(failingInput);
    ASSERT_FALSE(err);
}

TEST_F(RPCBaseTest, ClampingModifier)
{
    auto spec = RpcSpec{
        {"amount", Clamp<uint32_t>{10u, 20u}},
    };

    auto passingInput = json::parse(R"({ "amount": 15 })");
    ASSERT_TRUE(spec.process(passingInput));

    auto passingInput2 = json::parse(R"({ "amount": 5 })");
    ASSERT_TRUE(spec.process(passingInput2));
    ASSERT_EQ(passingInput2.at("amount").as_uint64(), 10u);  // clamped

    auto passingInput3 = json::parse(R"({ "amount": 25 })");
    ASSERT_TRUE(spec.process(passingInput3));
    ASSERT_EQ(passingInput3.at("amount").as_uint64(), 20u);  // clamped
}

TEST_F(RPCBaseTest, ToLowerModifier)
{
    auto spec = RpcSpec{
        {"str", ToLower{}},
    };

    auto passingInput = json::parse(R"({ "str": "TesT" })");
    ASSERT_TRUE(spec.process(passingInput));
    ASSERT_EQ(passingInput.at("str").as_string(), "test");

    auto passingInput2 = json::parse(R"({ "str2": "TesT" })");
    ASSERT_TRUE(spec.process(passingInput2));  // no str no problem

    auto passingInput3 = json::parse(R"({ "str": "already lower case" })");
    ASSERT_TRUE(spec.process(passingInput3));
    ASSERT_EQ(passingInput3.at("str").as_string(), "already lower case");

    auto passingInput4 = json::parse(R"({ "str": "" })");
    ASSERT_TRUE(spec.process(passingInput4));  // empty str no problem
    ASSERT_EQ(passingInput4.at("str").as_string(), "");
}

TEST_F(RPCBaseTest, ToNumberModifier)
{
    auto const spec = RpcSpec{
        {"str", ToNumber{}},
    };

    auto passingInput = json::parse(R"({ "str": [] })");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"({ "str2": "TesT" })");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"([])");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"({ "str": "123" })");
    ASSERT_TRUE(spec.process(passingInput));
    ASSERT_EQ(passingInput.at("str").as_int64(), 123);

    auto failingInput = json::parse(R"({ "str": "ok" })");
    ASSERT_FALSE(spec.process(failingInput));

    failingInput = json::parse(R"({ "str": "123.123" })");
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
    auto passingInput = json::parse(R"({ "str": "sss" })");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"({ "strNotExist": 123 })");
    ASSERT_TRUE(spec.process(passingInput));

    // not a json object
    passingInput = json::parse(R"([])");
    ASSERT_TRUE(spec.process(passingInput));
}
