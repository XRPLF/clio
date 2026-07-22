#pragma once

#include "util/MockOperation.hpp"
#include "util/async/AnyStopToken.hpp"
#include "util/async/Error.hpp"
#include "util/async/impl/Any.hpp"

#include <gmock/gmock.h>

#include <chrono>
#include <expected>
#include <functional>
#include <optional>

struct MockStrand {
    template <typename T>
    using ValueType = std::expected<T, util::async::ExecutionError>;

    using Any = util::async::impl::Any;

    template <typename T>
    using Operation = MockOperation<T>;

    template <typename T>
    using StoppableOperation = MockStoppableOperation<T>;

    template <typename T>
    using RepeatingOperation = MockRepeatingOperation<T>;

    MOCK_METHOD(Operation<Any> const&, execute, (std::function<Any()>), (const));
    MOCK_METHOD(
        Operation<Any> const&,
        execute,
        (std::function<Any()>, std::optional<std::chrono::milliseconds>),
        (const)
    );
    MOCK_METHOD(
        StoppableOperation<Any> const&,
        execute,
        (std::function<Any(util::async::AnyStopToken)>),
        (const)
    );
    MOCK_METHOD(
        StoppableOperation<Any> const&,
        execute,
        (std::function<Any(util::async::AnyStopToken)>, std::optional<std::chrono::milliseconds>),
        (const)
    );
    MOCK_METHOD(
        RepeatingOperation<Any> const&,
        executeRepeatedly,
        (std::chrono::milliseconds, std::function<Any()>),
        (const)
    );
    MOCK_METHOD(void, submit, (std::function<void()>), (const));
};
