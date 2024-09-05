//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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
        (std::function<std::any(util::async::AnyStopToken)>, std::optional<std::chrono::milliseconds>),
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
    MOCK_METHOD(MockStrand const&, makeStrand, (), ());
    MOCK_METHOD(void, stop, (), (const));
    MOCK_METHOD(void, join, (), ());
};
