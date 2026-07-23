#pragma once

#include "util/MockOperation.hpp"
#include "util/MockStopToken.hpp"
#include "util/MockStrand.hpp"
#include "util/async/AnyStopToken.hpp"
#include "util/async/Error.hpp"
#include "util/async/impl/Any.hpp"

#include <gmock/gmock.h>

#include <chrono>
#include <expected>
#include <functional>
#include <optional>

struct MockExecutionContext {
    template <typename T>
    using ValueType = std::expected<T, util::async::ExecutionError>;

    using Any = util::async::impl::Any;

    using StopSource = MockStopSource;
    using StopToken = MockStopToken;
    using Strand = MockStrand;

    template <typename T>
    using Operation = MockOperation<T>;

    template <typename T>
    using StoppableOperation = MockStoppableOperation<T>;

    template <typename T>
    using ScheduledOperation = MockScheduledOperation<T>;

    template <typename T>
    using RepeatingOperation = MockRepeatingOperation<T>;

    MOCK_METHOD(Operation<Any> const&, execute, (std::function<Any()>), ());
    MOCK_METHOD(
        Operation<Any> const&,
        execute,
        (std::function<Any()>, std::optional<std::chrono::milliseconds>),
        ()
    );
    MOCK_METHOD(
        StoppableOperation<Any> const&,
        execute,
        (std::function<Any(util::async::AnyStopToken)>, std::optional<std::chrono::milliseconds>),
        ()
    );
    MOCK_METHOD(
        ScheduledOperation<Any> const&,
        scheduleAfter,
        (std::chrono::milliseconds, std::function<Any(util::async::AnyStopToken)>),
        ()
    );
    MOCK_METHOD(
        ScheduledOperation<Any> const&,
        scheduleAfter,
        (std::chrono::milliseconds, std::function<Any(util::async::AnyStopToken, bool)>),
        ()
    );
    MOCK_METHOD(
        RepeatingOperation<Any> const&,
        executeRepeatedly,
        (std::chrono::milliseconds, std::function<Any()>),
        ()
    );
    MOCK_METHOD(void, submit, (std::function<void()>), ());

    MOCK_METHOD(MockStrand const&, makeStrand, (), ());
    MOCK_METHOD(void, stop, (), (const));
    MOCK_METHOD(void, join, (), ());
};
