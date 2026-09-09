#pragma once

#include "rpc/common/Concepts.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/value.hpp>
#include <rpcspec/WarningsToJson.hpp>

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
        using boost::json::value_to;

        static_assert(
            kIsSingleInputPath<HandlerType>,
            "handler satisfies both the legacy and the typed input path; dispatch would be "
            "decided by the order of the branches below rather than by the handler"
        );
        static_assert(
            SomeHandlerWithTypedInput<HandlerType> or SomeHandlerWithInput<HandlerType> or
                SomeHandlerWithoutInput<HandlerType>,
            "handler matches none of the branches below"
        );

        // New `rpc-spec`-based handler
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

        if constexpr (SomeHandlerWithInput<HandlerType>) {
            // Old spec-based handler: first we run validation against specified API version
            // TODO: This will be eventually removed once fully migraded to new rpc-spec system.
            auto const spec = handler.spec(ctx.apiVersion);
            auto warnings = spec.check(value);
            auto input = value;  // copy here, spec require mutable data

            if (auto const ret = spec.process(input); not ret.has_value())
                return ReturnType{Error{ret.error()}, std::move(warnings)};  // forward Status

            auto const inData = value_to<typename HandlerType::Input>(input);
            auto ret = handler.process(inData, ctx);

            // real handler is given expected Input, not json
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
