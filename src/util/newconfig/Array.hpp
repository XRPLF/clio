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

<<<<<<< HEAD
#include <tuple>
=======
#include "util/Assert.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/ValueView.hpp"

#include <cstddef>
#include <iterator>
#include <vector>
>>>>>>> d2f765f (Commit work so far)

namespace util::config {

/**
 * @brief Array definition for Json/Yaml config
 *
 * Used in ClioConfigDefinition to represent multiple potential values (like whitelist)
 */
<<<<<<< HEAD
template <typename... Args>
class Array {
public:
    constexpr Array(Args... args) : elements_{args...}
    {
    }
    constexpr auto
    begin()
    {
        return elements_.begin();
    }
    constexpr auto
    end()
    {
        return elements_.end();
    }

private:
    std::tuple<Args...> elements_{};
=======
class Array {
public:
    /** @brief Custom iterator class which returns ValueView of what's underneath the Array
     */
    struct ArrayIterator {
        using iterator_category = std::forward_iterator_tag;
        using pointer = ConfigValue const*;
        using reference = ConfigValue const&;
        using valueType = ConfigValue;

        /**
         * @brief Constructs an ArrayIterator with a pointer to the ConfigValue
         *
         * @param ptr Pointer to the ConfigValue
         */
        ArrayIterator(pointer ptr) : m_ptr(ptr)
        {
        }

        /**
         * @brief Prefix increment operator
         *
         * @return Reference to the incremented ArrayIterator
         */
        ArrayIterator&
        operator++()
        {
            m_ptr++;
            return *this;
        }

        /**
         * @brief Postfix increment operator
         *
         * @return Copy of the ArrayIterator before increment
         */
        ArrayIterator
        operator++(int)
        {
            ArrayIterator temp = *this;
            m_ptr++;
            return temp;
        }

        /**
         * @brief Dereference operator to get a ValueView of the ConfigValue
         *
         * @return ValueView of the ConfigValue
         */
        ValueView
        operator*()
        {
            return ValueView(*m_ptr);
        }

        /**
         * @brief Equality operator
         *
         * @param other Another ArrayIterator to compare
         * @return true if iterators are equal, otherwise false
         */
        bool
        operator==(ArrayIterator const& other) const
        {
            return m_ptr == other.m_ptr;
        }

        /**
         * @brief Inequality operator
         *
         * @param other Another ArrayIterator to compare
         * @return true if iterators are not equal, otherwise false
         */
        bool
        operator!=(ArrayIterator const& other) const
        {
            return m_ptr != other.m_ptr;
        }

    private:
        pointer m_ptr;
    };

    /**
     * @brief Constructs an Array with the provided arguments
     *
     * @tparam Args Types of the arguments
     * @param args Arguments to initialize the elements of the Array
     */
    template <typename... Args>
    constexpr Array(Args... args) : elements_{args...}
    {
    }

    /**
     * @brief Returns an iterator to the beginning of the Array
     *
     * @return Iterator to the beginning of the Array
     */
    auto
    begin() const
    {
        return ArrayIterator{elements_.data()};
    }

    /**
     * @brief Returns an iterator to the end of the Array
     *
     * @return Iterator to the end of the Array
     */
    auto
    end() const
    {
        return ArrayIterator{elements_.data() + elements_.size()};
    }

    /** @brief Add ConfigValues to Array class
     *
     * @param value The ConfigValue to add
     */
    void
    emplace_back(ConfigValue value)
    {
        elements_.emplace_back(value);
    }

    /**
     * @brief Returns the number of values stored in the Array
     *
     * @return Number of values stored in the Array
     */
    size_t
    size() const
    {
        return elements_.size();
    }

    /**
     * @brief Returns the ConfigValue at the specified index
     *
     * @param idx Index of the ConfigValue to retrieve
     * @return ConfigValue at the specified index
     */
    ConfigValue const&
    at(std::size_t idx) const
    {
        ASSERT(idx >= 0 && idx < elements_.size(), "index is out of scope");
        return elements_[idx];
    }

private:
    std::vector<ConfigValue> elements_;
>>>>>>> d2f765f (Commit work so far)
};

}  // namespace util::config
