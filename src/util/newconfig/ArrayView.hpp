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

#include "util/Assert.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ObjectView.hpp"
#include "util/newconfig/ValueView.hpp"

#include <cstddef>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>

namespace util::config {

/**
 * @brief View for array structure for config.
 *
 * This class provides a view into an array structure within ClioConfigDefinition.
 * It allows accessing individual elements of the array as either values or objects, and
 * is used within the ClioConfigDefinition to represent multiple potential values.
 */
class ArrayView {
public:
    /**
     * @brief Custom iterator class which contains config object or value underneath ArrayView
     */
    template <typename T>
    struct ArrayIterator {
        using iterator_category = std::forward_iterator_tag;
        using pointer = T const*;
        using reference = T const&;
        using value_type = T;

        /**
         * @brief Constructs an ArrayIterator with underlying ArrayView and index value
         *
         * @param arr ArrayView to iterate
         * @param index Current index of the ArrayView
         */
        ArrayIterator(ArrayView const& arr, std::size_t index) : arr_{arr}, index_{index}
        {
            if (arr_.clioConfig_.get().contains(arr_.prefix_)) {
                ASSERT((std::is_same_v<T, ValueView>), "Array iterator must be ValueView");
            } else {
                ASSERT((std::is_same_v<T, ObjectView>), "Array iterator must be ObjectView");
            }
        }

        /**
         * @brief Prefix increment operator
         *
         * @return Reference to the incremented ArrayIterator
         */
        ArrayIterator&
        operator++()
        {
            if (index_ < arr_.size())
                index_++;
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
            if (index_ < arr_.size())
                index_++;
            return temp;
        }

        /**
         * @brief Dereference operator to get a ValueView or ObjectView
         *
         * @return ValueView of the ConfigValue
         */
        T
        operator*()
        {
            if constexpr (std::is_same_v<T, ObjectView>) {
                return ObjectView{arr_.prefix_, index_, arr_.clioConfig_.get()};
            } else {
                return arr_.clioConfig_.get().getValueInArray(arr_.prefix_, index_);
            }
        }

        /**
         * @brief Equality operator
         *
         * @param other Another ArrayIterator to compare
         * @return true if iterators are pointing to the same ArrayView underneath, otherwise false
         */
        bool
        operator==(ArrayIterator const& other) const
        {
            return (&(this->arr_) == &(other.arr_)) ? index_ == other.index_ : false;
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
            return index_ != other.index_;
        }

    private:
        ArrayView const& arr_;
        std::size_t index_ = 0;
    };

    /**
     * @brief Returns an iterator to the beginning of the Array
     *
     * @return Iterator to the beginning of the Array
     */
    template <typename T>
    auto
    begin() const
    {
        return ArrayIterator<T>(*this, 0);
    }

    /**
     * @brief Returns an iterator to the end of the Array
     *
     * @return Iterator to the end of the Array
     */
    template <typename T>
    auto
    end() const
    {
        return ArrayIterator<T>(*this, this->size());
    }

    /**
     * @brief Constructs an ArrayView with the given prefix and config definition.
     *
     * @param prefix The prefix for the array view.
     * @param configDef The ClioConfigDefinition instance.
     */
    ArrayView(std::string_view prefix, ClioConfigDefinition const& configDef);

    /**
     * @brief Returns an ObjectView at the specified index.
     *
     * @param idx Index of the object to retrieve.
     * @return ObjectView at the specified index.
     */
    [[nodiscard]] ObjectView
    objectAt(std::size_t idx) const;

    /**
     * @brief Returns a ValueView at the specified index.
     *
     * @param idx Index of the value to retrieve.
     * @return ValueView at the specified index.
     */
    [[nodiscard]] ValueView
    valueAt(std::size_t idx) const;

    /**
     * @brief Returns the number of elements in the array.
     *
     * @return Number of elements in the array.
     */
    [[nodiscard]] size_t
    size() const;

private:
    std::string const prefix_;
    std::reference_wrapper<ClioConfigDefinition const> clioConfig_;
};

}  // namespace util::config
