#pragma once

#include "rpc/common/Concepts.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/value.hpp>
#include <rpcspec/backends/BoostJson.hpp>

#include <utility>

namespace rpc::impl {

template <SomeHandler HandlerType>
struct DefaultProcessor final {
    [[nodiscard]] ReturnType
    operator()(
        HandlerType const& handler,
        boost::json::value const& value,
        Context const& ctx
    ) const
    {
        using boost::json::value_from;

        static_assert(
            SomeHandlerWithTypedInput<HandlerType> or SomeHandlerWithoutInput<HandlerType>,
            "handler matches none of the branches below"
        );

        if constexpr (SomeHandlerWithTypedInput<HandlerType>) {
            auto input = HandlerType::parseInput(value, ctx.apiVersion);
            auto warnings = rpc::spec::toJsonArray(HandlerType::spec(ctx.apiVersion).check(value));

            if (not input.has_value())
                return ReturnType{Error{std::move(input).error()}, std::move(warnings)};

            auto ret = handler.process(*input, ctx);

            if (not ret.has_value())
                return ReturnType{Error{std::move(ret).error()}, std::move(warnings)};

            return ReturnType{value_from(std::move(ret).value()), std::move(warnings)};
        }

        if constexpr (SomeHandlerWithoutInput<HandlerType>) {
            // no input to pass, ignore the value
            auto const ret = handler.process(ctx);
            if (not ret.has_value())
                return ReturnType{Error{ret.error()}};  // forward Status

            return ReturnType{value_from(ret.value())};
        }
    }
};

}  // namespace rpc::impl
