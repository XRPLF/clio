//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include <chrono>
#include <type_traits>
#include <utility>

namespace util {

/**
 * @brief Profiler function to measure the time consuming
 * @param func function object, can be a lamdba or function wrapper
 * @return return a pair if function wrapper has return value: result of
 * function wrapper and the elapsed time(ms) during executing the given
 * function only return the elapsed time if function wrapper does not have
 * return value
 */
template <typename U = std::chrono::milliseconds, typename F>
[[nodiscard]] auto
timed(F&& func)
{
    auto start = std::chrono::system_clock::now();

    if constexpr (std::is_same_v<decltype(func()), void>)
    {
        func();
        return std::chrono::duration_cast<U>(std::chrono::system_clock::now() - start).count();
    }
    else
    {
        auto ret = func();
        auto elapsed = std::chrono::duration_cast<U>(std::chrono::system_clock::now() - start).count();
        return std::make_pair(ret, elapsed);
    }
}

}  // namespace util
