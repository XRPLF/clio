#pragma once

#include "util/MockOperation.hpp"
#include "util/async/AnyStopToken.hpp"
#include "util/async/Error.hpp"

#include <gmock/gmock.h>

#include <any>
#include <chrono>
#include <expected>
#include <functional>
#include <optional>

struct MockStrand {
    template <typename T>
    using ValueType = std::expected<T, util::async::ExecutionError>;

    template <typename T>
    using Operation = MockOperation<T>;

    template <typename T>
    using StoppableOperation = MockStoppableOperation<T>;

    template <typename T>
    using RepeatingOperation = MockRepeatingOperation<T>;

    MOCK_METHOD(Operation<std::any> const&, execute, (std::function<std::any()>), (const));
    MOCK_METHOD(
        Operation<std::any> const&,
        execute,
        (std::function<std::any()>, std::optional<std::chrono::milliseconds>),
        (const)
    );
    MOCK_METHOD(
        StoppableOperation<std::any> const&,
        execute,
        (std::function<std::any(util::async::AnyStopToken)>),
        (const)
    );
    MOCK_METHOD(
        StoppableOperation<std::any> const&,
        execute,
        (std::function<std::any(util::async::AnyStopToken)>,
         std::optional<std::chrono::milliseconds>),
        (const)
    );
    MOCK_METHOD(
        RepeatingOperation<std::any> const&,
        executeRepeatedly,
        (std::chrono::milliseconds, std::function<std::any()>),
        (const)
    );
    MOCK_METHOD(void, submit, (std::function<void()>), (const));
};
