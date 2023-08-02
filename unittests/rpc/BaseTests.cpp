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

#include <util/Fixtures.h>

#include <rpc/Factories.h>
#include <rpc/common/AnyHandler.h>
#include <rpc/common/MetaProcessors.h>
#include <rpc/common/Modifiers.h>
#include <rpc/common/Specs.h>
#include <rpc/common/Validators.h>

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gtest/gtest.h>

#include <optional>
#include <string>

using namespace clio;
using namespace std;

using namespace RPC;
using namespace RPC::validation;
using namespace RPC::meta;
using namespace RPC::modifiers;

namespace json = boost::json;

class RPCBaseTest : public NoLoggerFixture
{
};

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
    // clang-format off
    auto spec = RpcSpec{
        {"arr", Required{}, Type<json::array>{}, ValidateArrayAt{0, {
            {"limit", Required{}, Type<uint32_t>{}, Between<uint32_t>{0, 100}},
        }}},
        {"arr2", ValidateArrayAt{0, {
            {"limit", Required{}, Type<uint32_t>{}, Between<uint32_t>{0, 100}},
        }}},
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
    // clang-format off
    auto spec = RpcSpec{
        {"mix", 
            Required{}, Type<std::string, json::object>{},
            IfType<json::object>{
                Section{{"limit", Required{}, Type<uint32_t>{}, Between<uint32_t>{0, 100}}},
                Section{{"limit2", Required{}, Type<uint32_t>{}, Between<uint32_t>{0, 100}}}
            },
            IfType<std::string>{
                Uint256HexStringValidator
            }
        },
        {"mix2",
            Section{{"limit", Required{}, Type<uint32_t>{}, Between<uint32_t>{0, 100}}}, 
            Type<std::string, json::object>{}
        },
    };
    // clang-format on

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
         WithCustomError{Uint256HexStringValidator, RPC::Status{ripple::rpcBAD_FEATURE, "MyCustomError"}}},
        {"other", WithCustomError{Type<std::string>{}, RPC::Status{ripple::rpcALREADY_MULTISIG, "MyCustomError2"}}}};

    auto passingInput = json::parse(
        R"({ "transaction": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC", "other": "1"})");
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
    // clang-format off
    auto customFormatCheck = CustomValidator{
        [](json::value const& value, std::string_view key) -> MaybeError {
            return value.as_string().size() == 34 ? 
                MaybeError{} : Error{RPC::Status{"Uh oh"}};
        }
    };
    // clang-format on

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
        {"ledgerIndex", LedgerIndexValidator},
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
        {"account", AccountValidator},
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
        {"marker", AccountMarkerValidator},
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
    auto const spec = RpcSpec{{"transaction", Uint256HexStringValidator}};
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
    auto const spec = RpcSpec{{"currency", CurrencyValidator}};
    auto passingInput = json::parse(R"({ "currency": "GBP"})");
    ASSERT_TRUE(spec.process(passingInput));

    passingInput = json::parse(R"({ "currency": "0158415500000000C1F76FF6ECB0BAC600000000"})");
    ASSERT_TRUE(spec.process(passingInput));

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
    auto const spec = RpcSpec{{"issuer", IssuerValidator}};
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
    auto const spec = RpcSpec{{"streams", SubscribeStreamValidator}};
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
        })");
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
    auto const spec = RpcSpec{{"accounts", SubscribeAccountsValidator}};
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
