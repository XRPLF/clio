#pragma once

#include "util/MockOperation.hpp"
#include "util/MockStopToken.hpp"
#include "util/MockStrand.hpp"
#include "util/async/AnyStopToken.hpp"
#include "util/async/Error.hpp"

#include <gmock/gmock.h>

#include <any>
#include <chrono>
#include <expected>
#include <functional>
#include <optional>

struct MockExecutionContext {
    template <typename T>
    using ValueType = std::expected<T, util::async::ExecutionError>;

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

    MOCK_METHOD(Operation<std::any> const&, execute, (std::function<std::any()>), ());
    MOCK_METHOD(
        Operation<std::any> const&,
        execute,
        (std::function<std::any()>, std::optional<std::chrono::milliseconds>),
        ()
    );
    MOCK_METHOD(
        StoppableOperation<std::any> const&,
        execute,
        (std::function<std::any(util::async::AnyStopToken)>,
         std::optional<std::chrono::milliseconds>),
        ()
    );
    MOCK_METHOD(
        ScheduledOperation<std::any> const&,
        scheduleAfter,
        (std::chrono::milliseconds, std::function<std::any(util::async::AnyStopToken)>),
        ()
    );
    MOCK_METHOD(
        ScheduledOperation<std::any> const&,
        scheduleAfter,
        (std::chrono::milliseconds, std::function<std::any(util::async::AnyStopToken, bool)>),
        ()
    );
    MOCK_METHOD(
        RepeatingOperation<std::any> const&,
        executeRepeatedly,
        (std::chrono::milliseconds, std::function<std::any()>),
        ()
    );
    MOCK_METHOD(void, submit, (std::function<void()>), ());

    MOCK_METHOD(MockStrand const&, makeStrand, (), ());
    MOCK_METHOD(void, stop, (), (const));
    MOCK_METHOD(void, join, (), ());
};
