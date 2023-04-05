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

#pragma once

#include <rpc/RPC.h>
#include <rpc/common/Specs.h>
#include <rpc/common/Validators.h>

#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <gmock/gmock.h>

#include <optional>
#include <string>

namespace unittests::detail {

// input data for the test handlers below
struct TestInput
{
    std::string hello;
    std::optional<uint32_t> limit;
};

// output data produced by the test handlers below
struct TestOutput
{
    std::string computed;
};

// must be implemented as per rpc/common/Concepts.h
inline TestInput
tag_invoke(boost::json::value_to_tag<TestInput>, boost::json::value const& jv)
{
    std::optional<uint32_t> optLimit;
    if (jv.as_object().contains("limit"))
        optLimit = jv.at("limit").as_int64();

    return {jv.as_object().at("hello").as_string().c_str(), optLimit};
}

// must be implemented as per rpc/common/Concepts.h
inline void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, TestOutput const& output)
{
    jv = {{"computed", output.computed}};
}

// example handler
class HandlerFake
{
public:
    using Input = TestInput;
    using Output = TestOutput;
    using Result = RPCng::HandlerReturnType<Output>;

    RPCng::RpcSpecConstRef
    spec() const
    {
        using namespace RPCng::validation;

        static const auto rpcSpec = RPCng::RpcSpec{
            {"hello", Required{}, Type<std::string>{}, EqualTo{"world"}},
            {"limit", Type<uint32_t>{}, Between<uint32_t>{0, 100}},  // optional field
        };

        return rpcSpec;
    }

    Result
    process(Input input) const
    {
        return Output{input.hello + '_' + std::to_string(input.limit.value_or(0))};
    }
};

// example handler
class CoroutineHandlerFake
{
public:
    using Input = TestInput;
    using Output = TestOutput;
    using Result = RPCng::HandlerReturnType<Output>;

    RPCng::RpcSpecConstRef
    spec() const
    {
        using namespace RPCng::validation;

        static const auto rpcSpec = RPCng::RpcSpec{
            {"hello", Required{}, Type<std::string>{}, EqualTo{"world"}},
            {"limit", Type<uint32_t>{}, Between<uint32_t>{0, 100}},  // optional field
        };

        return rpcSpec;
    }

    Result
    process(Input input, RPCng::Context const& ctx) const
    {
        return Output{input.hello + '_' + std::to_string(input.limit.value_or(0))};
    }
};

class NoInputHandlerFake
{
public:
    using Output = TestOutput;
    using Result = RPCng::HandlerReturnType<Output>;

    Result
    process() const
    {
        return Output{"test"};
    }
};

// example handler that returns custom error
class FailingHandlerFake
{
public:
    using Input = TestInput;
    using Output = TestOutput;
    using Result = RPCng::HandlerReturnType<Output>;

    RPCng::RpcSpecConstRef
    spec() const
    {
        using namespace RPCng::validation;

        static const auto rpcSpec = RPCng::RpcSpec{
            {"hello", Required{}, Type<std::string>{}, EqualTo{"world"}},
            {"limit", Type<uint32_t>{}, Between<uint32_t>{0u, 100u}},  // optional field
        };

        return rpcSpec;
    }

    Result
    process([[maybe_unused]] Input input) const
    {
        // always fail
        return RPCng::Error{RPC::Status{"Very custom error"}};
    }
};

struct InOutFake
{
    std::string something;

    // Note: no spaceship comparison possible for std::string
    friend bool
    operator==(InOutFake const& lhs, InOutFake const& rhs) = default;
};

// must be implemented as per rpc/common/Concepts.h
inline InOutFake
tag_invoke(boost::json::value_to_tag<InOutFake>, boost::json::value const& jv)
{
    return {jv.as_object().at("something").as_string().c_str()};
}

// must be implemented as per rpc/common/Concepts.h
inline void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, InOutFake const& output)
{
    jv = {{"something", output.something}};
}

struct HandlerMock
{
    using Input = InOutFake;
    using Output = InOutFake;
    using Result = RPCng::HandlerReturnType<Output>;

    MOCK_METHOD(RPCng::RpcSpecConstRef, spec, (), (const));
    MOCK_METHOD(Result, process, (Input), (const));
};

struct HandlerWithoutInputMock
{
    using Output = InOutFake;
    using Result = RPCng::HandlerReturnType<Output>;

    MOCK_METHOD(Result, process, (), (const));
};

}  // namespace unittests::detail
