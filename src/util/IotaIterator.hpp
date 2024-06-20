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

#include <iterator>

namespace util {

/**
 * @brief An customised iterator that mimic consecutive integers without creating a container. The purpose of this
 * iterator is to bridge the consecutive integers to the standard library algorithms that require iterators. This is a
 * random access iterator, which means it supports X a, X b(a), b=a, a==b, a!=b, *a, ++a, a++, a+=n, a-=n, a+n, a-n,
 * a-b, a<b, a>b, a<=b, a>=b, a[n]
 */
struct IotaIterator {
private:
    int value_;

public:
    using value_type = int;
    using difference_type = std::ptrdiff_t;
    using pointer = int*;
    using reference = int&;
    using iterator_category = std::random_access_iterator_tag;

    /**
     * @brief Construct a new Iota Iterator object
     * @param v The value of the iterator
     */
    IotaIterator(int v) : value_(v)
    {
    }

    IotaIterator(IotaIterator const&) = default;

    IotaIterator&
    operator=(IotaIterator const&) = default;

    /**
     * @brief Dereference operator, return the value_
     * @return int const& The value_'s const reference
     */
    int const&
    operator*() const noexcept
    {
        return value_;
    }

    /**
     * @brief += operator, increment the value_ by offset
     * @param offset The offset to increment the value_
     * @return IotaIterator& The iterator itself
     */
    IotaIterator&
    operator+=(int offset) noexcept
    {
        value_ += offset;
        return *this;
    }

    /**
     * @brief -= operator, decrement the value_ by offset
     * @param offset The offset to decrement the value_
     * @return IotaIterator& The iterator itself
     */
    IotaIterator&
    operator-=(int offset) noexcept
    {
        value_ += (-offset);
        return *this;
    }

    /**
     * @brief ++ operator, increment the value_ by 1 and return the iterator itself
     * @return IotaIterator The iterator itself
     */
    IotaIterator
    operator++() noexcept
    {
        ++value_;
        return *this;
    }

    /**
     * @brief ++(int) operator, return a new iterator of value_ then increment itself value_ by 1
     * @return IotaIterator The new iterator
     */
    IotaIterator
    operator++(int) noexcept
    {
        IotaIterator tmp(*this);
        ++(*this);
        return tmp;
    }

    /**
     * @brief -- operator, decrement the value_ by 1 and return the iterator itself
     * @return IotaIterator The iterator itself
     */
    IotaIterator
    operator--() noexcept
    {
        --value_;
        return *this;
    }

    /**
     * @brief --(int) operator, return a new iterator of value_ then decrement itself value_ by 1
     * @return IotaIterator The new iterator
     */
    IotaIterator
    operator--(int) noexcept
    {
        IotaIterator tmp(*this);
        --(*this);
        return tmp;
    }

    /**
     * @brief offset operator, return the iterator that is offset away from the current iterator
     * @param offset The offset to move the iterator
     * @return IotaIterator The new iterator that is offset away from the current iterator
     */
    IotaIterator
    operator[](int offset) const noexcept
    {
        return IotaIterator(value_ + offset);
    }
};

/**
 * @brief Compare two IotaIterator objects, return true if they contains the same value_
 * @param lhs The left hand side IotaIterator object
 * @param rhs The right hand side IotaIterator object
 * @return true If they contains the same value_
 */
bool
operator==(IotaIterator const& lhs, IotaIterator const& rhs) noexcept;

/**
 * @brief Subtract two IotaIterator objects, return the difference of their value_
 * @param lhs The left hand side IotaIterator object
 * @param rhs The right hand side IotaIterator object
 * @return IotaIterator::difference_type The difference of their value_
 */
IotaIterator::difference_type
operator-(IotaIterator const& lhs, IotaIterator const& rhs) noexcept;

/**
 * @brief Compare two IotaIterator objects, return true if they do not contain the same value_
 * @param lhs The left hand side IotaIterator object
 * @param rhs The right hand side IotaIterator object
 * @return true If they do not contain the same value_
 */
bool
operator!=(IotaIterator const& lhs, IotaIterator const& rhs) noexcept;

/**
 * @brief Compare two IotaIterator objects, return true if the value_ of rhs is less than the value_ of lhs
 * @param lhs The left hand side IotaIterator object
 * @param rhs The right hand side IotaIterator object
 * @return true If the value_ of rhs is less than the value_ of lhs
 */
bool
operator<(IotaIterator const& lhs, IotaIterator const& rhs) noexcept;

/**
 * @brief Compare two IotaIterator objects, return true if the value_ of rhs is less than or equal to the value_ of lhs
 * @param lhs The left hand side IotaIterator object
 * @param rhs The right hand side IotaIterator object
 * @return true If the value_ of rhs is less than or equal to the value_ of lhs
 */
bool
operator<=(IotaIterator const& lhs, IotaIterator const& rhs) noexcept;

/**
 * @brief Compare two IotaIterator objects, return true if the value_ of rhs is greater than the value_ of lhs
 * @param lhs The left hand side IotaIterator object
 * @param rhs The right hand side IotaIterator object
 * @return true If the value_ of rhs is greater than the value_ of lhs
 */
bool
operator>(IotaIterator const& lhs, IotaIterator const& rhs) noexcept;

/**
 * @brief Compare two IotaIterator objects, return true if the value_ of rhs is greater than or equal to the value_ of
 * @param lhs The left hand side IotaIterator object
 * @param rhs The right hand side IotaIterator object
 * @return true If the value_ of rhs is greater than or equal to the value_ of lhs
 */
bool
operator>=(IotaIterator const& lhs, IotaIterator const& rhs) noexcept;

/**
 * @brief Add an offset to the IotaIterator object, return a new IotaIterator object that contains the value_ of the
 * original IotaIterator object plus the offset
 * @param lhs The IotaIterator object
 * @param offset The offset to add to the IotaIterator object
 * @return IotaIterator The new IotaIterator object that contains the value_ of the original IotaIterator object plus
 * the offset
 */
IotaIterator
operator+(IotaIterator const& lhs, int offset) noexcept;

/**
 * @brief Subtract an offset from the IotaIterator object, return a new IotaIterator object that contains the value_ of
 * the original IotaIterator object minus the offset
 * @param lhs The IotaIterator object
 * @param offset The offset to subtract from the IotaIterator object
 * @return IotaIterator The new IotaIterator object that contains the value_ of the original IotaIterator object minus
 * the offset
 */
IotaIterator
operator-(IotaIterator const& lhs, int offset) noexcept;

}  // namespace util
