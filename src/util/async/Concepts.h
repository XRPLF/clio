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

#include <boost/asio/spawn.hpp>

#include <chrono>
#include <functional>
#include <type_traits>

namespace util::async {

template <typename T>
concept SomeStoppable = requires(T v) {
    {
        v.requestStop()
    } -> std::same_as<void>;
};

template <typename T>
concept SomeCancellable = requires(T v) {
    {
        v.cancel()
    } -> std::same_as<void>;
};

template <typename T>
concept SomeOperation = requires(T v) {
    {
        v.wait()
    } -> std::same_as<void>;
    {
        v.get()
    };
};

template <typename T>
concept SomeStoppableOperation = SomeOperation<T> and SomeStoppable<T>;

template <typename T>
concept SomeCancellableOperation = SomeOperation<T> and SomeCancellable<T>;

template <typename T>
concept SomeOutcome = requires(T v) {
    {
        v.getOperation()
    } -> SomeOperation;
};

template <typename T>
concept SomeStopToken = requires(T v) {
    {
        v.isStopRequested()
    } -> std::same_as<bool>;
};

template <typename T>
concept SomeYieldStopSource = requires(T v, boost::asio::yield_context yield) {
    {
        v[yield]
    } -> SomeStopToken;
};

template <typename T>
concept SomeSimpleStopSource = requires(T v) {
    {
        v.getToken()
    } -> SomeStopToken;
};

template <typename T>
concept SomeStopSource = (SomeSimpleStopSource<T> or SomeYieldStopSource<T>)and SomeStoppable<T>;

template <typename T>
concept SomeStopSourceProvider = requires(T v) {
    {
        v.getStopSource()
    } -> SomeStopSource;
};

template <typename T>
concept SomeStoppableOutcome = SomeOutcome<T> and SomeStopSourceProvider<T>;

template <typename T>
concept SomeHandlerWithoutStopToken = requires(T fn) {
    {
        std::invoke(fn)
    };
};

template <typename T, typename... Args>
concept SomeHandlerWith = requires(T fn) {
    {
        std::invoke(fn, std::declval<Args>()...)
    };
};

template <typename T>
concept SomeStdDuration = requires {
    // Thank you Ed Catmur for this trick.
    // See https://stackoverflow.com/questions/74383254/concept-that-models-only-the-stdchrono-duration-types
    []<class Rep, class Period>(std::type_identity<std::chrono::duration<Rep, Period>>) {}(std::type_identity<T>());
};

template <typename T>
concept SomeOptStdDuration = requires(T v) { SomeStdDuration<decltype(v.value())>; };

}  // namespace util::async