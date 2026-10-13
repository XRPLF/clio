#pragma once

#include "util/Assert.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ObjectView.hpp"
#include "util/config/ValueView.hpp"

#include <cstddef>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

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
        // Required by the std iterator contract.
        // NOLINTBEGIN(readability-identifier-naming)
        using iterator_category = std::forward_iterator_tag;
        using pointer = T const*;
        using reference = T const&;
        using value_type = T;
        // NOLINTEND(readability-identifier-naming)

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
                ++index_;
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
                ++index_;
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
         * @return true if iterators are pointing to the same element in the same ArrayView object,
         * otherwise false
         */
        bool
        operator==(ArrayIterator const& other) const
        {
            return &arr_ == &other.arr_ && index_ == other.index_;
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
            return &arr_ != &other.arr_ || index_ != other.index_;
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
    [[nodiscard]] auto
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
    [[nodiscard]] auto
    end() const
    {
        return ArrayIterator<T>(*this, this->size());
    }

    /**
     * @brief A range over the elements of an array, viewed as T (ValueView or ObjectView)
     */
    template <typename T>
    class Elements;

    /**
     * @brief Returns a range over the elements of an array of values
     *
     * @return Range of ValueView, usable in a range-based for loop
     */
    [[nodiscard]] Elements<ValueView>
    values() const;

    /**
     * @brief Returns a range over the elements of an array of objects
     *
     * @return Range of ObjectView, usable in a range-based for loop
     */
    [[nodiscard]] Elements<ObjectView>
    objects() const;

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
    std::string prefix_;
    std::reference_wrapper<ClioConfigDefinition const> clioConfig_;
};

template <typename T>
class ArrayView::Elements {
public:
    /**
     * @brief Constructs the range over the given array
     *
     * @param array The array to iterate over; stored by value so the range never dangles
     */
    explicit Elements(ArrayView array) : array_{std::move(array)}
    {
    }

    /**
     * @brief Returns an iterator to the first element
     *
     * @return Iterator to the first element
     */
    [[nodiscard]] ArrayIterator<T>
    begin() const
    {
        return array_.begin<T>();
    }

    /**
     * @brief Returns an iterator past the last element
     *
     * @return Iterator past the last element
     */
    [[nodiscard]] ArrayIterator<T>
    end() const
    {
        return array_.end<T>();
    }

private:
    ArrayView array_;
};

[[nodiscard]] inline ArrayView::Elements<ValueView>
ArrayView::values() const
{
    return Elements<ValueView>{*this};
}

[[nodiscard]] inline ArrayView::Elements<ObjectView>
ArrayView::objects() const
{
    return Elements<ObjectView>{*this};
}

}  // namespace util::config
