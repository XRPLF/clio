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

#include "util/Expected.hpp"
#include "util/MockOperation.hpp"
#include "util/async/AnyStopToken.hpp"
#include "util/async/Error.hpp"
#include "util/async/impl/Any.hpp"

#include <gmock/gmock.h>

#include <chrono>
#include <functional>
#include <optional>

struct MockStrand {
    template <typename T>
    using ValueType = util::Expected<T, util::async::ExecutionError>;

    template <typename T>
    using Operation = MockOperation<T>;

    template <typename T>
    using StoppableOperation = MockStoppableOperation<T>;

    MOCK_METHOD(Operation<util::async::impl::Any> const&, execute, (std::function<util::async::impl::Any()>), (const));
    MOCK_METHOD(
        Operation<util::async::impl::Any> const&,
        execute,
        (std::function<util::async::impl::Any()>, std::optional<std::chrono::milliseconds>),
        (const)
    );
    MOCK_METHOD(
        StoppableOperation<util::async::impl::Any> const&,
        execute,
        (std::function<util::async::impl::Any(util::async::AnyStopToken)>),
        (const)
    );
    MOCK_METHOD(
        StoppableOperation<util::async::impl::Any> const&,
        execute,
        (std::function<util::async::impl::Any(util::async::AnyStopToken)>, std::optional<std::chrono::milliseconds>),
        (const)
    );
};
