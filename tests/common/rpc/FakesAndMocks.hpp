#pragma once

#include "rpc/Errors.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <gmock/gmock.h>

#include <cstdint>
#include <optional>
#include <string>

namespace tests::common {

// input data for the test handlers below
struct TestInput {
    std::string hello;
    std::optional<uint32_t> limit;
};

// output data produced by the test handlers below
struct TestOutput {
    std::string computed;
};

// must be implemented as per rpc/common/Concepts.h
inline TestInput
tag_invoke(boost::json::value_to_tag<TestInput>, boost::json::value const& jv)
{
    std::optional<uint32_t> optLimit;
    if (jv.as_object().contains("limit"))
        optLimit = jv.at("limit").as_int64();

    return {
        .hello = boost::json::value_to<std::string>(jv.as_object().at("hello")), .limit = optLimit
    };
}

// must be implemented as per rpc/common/Concepts.h
inline void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, TestOutput const& output)
{
    jv = {{"computed", output.computed}};
}

// example handler
class HandlerFake {
public:
    using Input = TestInput;
    using Output = TestOutput;
    using Result = rpc::HandlerReturnType<Output>;

    static rpc::RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion)
    {
        using namespace rpc::validation;

        static auto const kRpcSpec = rpc::RpcSpec{
            {"hello", Required{}, Type<std::string>{}, EqualTo{"world"}},
            {"limit", Type<uint32_t>{}, Between<uint32_t>{0, 100}},  // optional field
        };

        return kRpcSpec;
    }

    static Result
    process(Input input, [[maybe_unused]] rpc::Context const& ctx)
    {
        return Output{input.hello + '_' + std::to_string(input.limit.value_or(0))};
    }
};

class NoInputHandlerFake {
public:
    using Output = TestOutput;
    using Result = rpc::HandlerReturnType<Output>;

    static Result
    process([[maybe_unused]] rpc::Context const& ctx)
    {
        return Output{"test"};
    }
};

// example handler that returns custom error
class FailingHandlerFake {
public:
    using Input = TestInput;
    using Output = TestOutput;
    using Result = rpc::HandlerReturnType<Output>;

    static rpc::RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion)
    {
        using namespace rpc::validation;

        static auto const kRpcSpec = rpc::RpcSpec{
            {"hello", Required{}, Type<std::string>{}, EqualTo{"world"}},
            {"limit", Type<uint32_t>{}, Between<uint32_t>{0u, 100u}},  // optional field
        };

        return kRpcSpec;
    }

    static Result
    process([[maybe_unused]] Input input, [[maybe_unused]] rpc::Context const& ctx)
    {
        // always fail
        return rpc::Error{rpc::Status{"Very custom error"}};
    }
};

struct InOutFake {
    std::string something;

    // Note: no spaceship comparison possible for std::string
    friend bool
    operator==(InOutFake const& lhs, InOutFake const& rhs) = default;
};

// must be implemented as per rpc/common/Concepts.h
inline InOutFake
tag_invoke(boost::json::value_to_tag<InOutFake>, boost::json::value const& jv)
{
    return {boost::json::value_to<std::string>(jv.as_object().at("something"))};
}

// must be implemented as per rpc/common/Concepts.h
inline void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, InOutFake const& output)
{
    jv = {{"something", output.something}};
}

struct HandlerMock {
    using Input = InOutFake;
    using Output = InOutFake;
    using Result = rpc::HandlerReturnType<Output>;

    MOCK_METHOD(rpc::RpcSpecConstRef, spec, (uint32_t), (const));
    MOCK_METHOD(Result, process, (Input, rpc::Context const&), (const));
};

struct HandlerWithoutInputMock {
    using Output = InOutFake;
    using Result = rpc::HandlerReturnType<Output>;

    MOCK_METHOD(Result, process, (rpc::Context const&), (const));
};

}  // namespace tests::common
