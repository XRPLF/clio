#pragma once

#include "rpc/Errors.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <gmock/gmock.h>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/FieldSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace tests::common {

// output data produced by the test handlers below
struct TestOutput {
    std::string computed;
};

// must be implemented as per rpc/common/Concepts.h
inline void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, TestOutput const& output)
{
    jv = {{"computed", output.computed}};
}

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

struct HandlerWithoutInputMock {
    using Output = InOutFake;
    using Result = rpc::HandlerReturnType<Output>;

    MOCK_METHOD(Result, process, (rpc::Context const&), (const));
};

// The shared consteval spec resolves a handler's spec from its Input type via an ADL
// `specFor` hook, so the fake Input below needs its own namespace to host that hook.
namespace typed_fake {

// input data for TypedHandlerFake
struct TypedInput {
    std::string hello;
    std::optional<uint32_t> limit;
};

inline constexpr auto kInputSpec = rpc::spec::spec<TypedInput>(
    rpc::spec::field(
        "hello",
        &TypedInput::hello,
        rpc::spec::required,
        rpc::spec::oneOf("world"),
        rpc::spec::asString
    ),
    rpc::spec::field(
        "limit",
        &TypedInput::limit,
        rpc::spec::between(uint32_t{0}, uint32_t{100}),
        rpc::spec::asUint32
    ),
    rpc::spec::field("old_field", rpc::spec::deprecated)
);

inline constexpr auto kSpec = rpc::spec::versioned<TypedInput>(kInputSpec);

[[nodiscard]] constexpr auto const&
specFor(TypedInput const*) noexcept
{
    return kSpec;
}

}  // namespace typed_fake

class TypedHandlerFake : public rpc::HandlerFor<typed_fake::TypedInput> {
public:
    using Output = TestOutput;
    using Result = rpc::HandlerReturnType<Output>;

    static Result
    process(Input const& input, [[maybe_unused]] rpc::Context const& ctx)
    {
        return Output{input.hello + '_' + std::to_string(input.limit.value_or(0))};
    }
};

class FailingTypedHandlerFake : public rpc::HandlerFor<typed_fake::TypedInput> {
public:
    using Output = TestOutput;
    using Result = rpc::HandlerReturnType<Output>;

    static Result
    process([[maybe_unused]] Input const& input, [[maybe_unused]] rpc::Context const& ctx)
    {
        return rpc::Error{rpc::Status{"Very custom error"}};
    }
};

}  // namespace tests::common
