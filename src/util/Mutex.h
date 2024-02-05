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

#include <mutex>
#include <type_traits>

namespace util {

template <typename ProtectedDataType>
class Mutex;

/**
 * @brief A lock on a mutex that provides access to the protected data.
 *
 * @tparam ProtectedDataType data type to hold
 */
template <typename ProtectedDataType>
class Lock {
    std::scoped_lock<std::mutex> lock_;
    ProtectedDataType& data_;

public:
    ProtectedDataType const&
    operator*() const
    {
        return data_;
    }

    ProtectedDataType&
    operator*()
    {
        return data_;
    }

    ProtectedDataType const&
    get() const
    {
        return data_;
    }

    ProtectedDataType&
    get()
    {
        return data_;
    }

    ProtectedDataType const*
    operator->() const
    {
        return &data_;
    }

    ProtectedDataType*
    operator->()
    {
        return &data_;
    }

private:
    friend class Mutex<std::remove_const_t<ProtectedDataType>>;

    explicit Lock(std::mutex& mutex, ProtectedDataType& data) : lock_(mutex), data_(data)
    {
    }
};

/**
 * @brief A container for data that is protected by a mutex. Inspired by Mutex in Rust.
 *
 * @tparam ProtectedDataType data type to hold
 */
template <typename ProtectedDataType>
class Mutex {
    mutable std::mutex mutex_;
    ProtectedDataType data_;

public:
    Mutex() = default;

    explicit Mutex(ProtectedDataType data) : data_(std::move(data))
    {
    }

    template <typename... Args>
    static Mutex
    make(Args&&... args)
    {
        return Mutex{ProtectedDataType{std::forward<Args>(args)...}};
    }

    Lock<ProtectedDataType const>
    lock() const
    {
        return Lock<ProtectedDataType const>{mutex_, data_};
    }

    Lock<ProtectedDataType>
    lock()
    {
        return Lock<ProtectedDataType>{mutex_, data_};
    }
};

}  // namespace util