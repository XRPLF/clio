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
#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/ObjectView.hpp"
#include "util/newconfig/ValueView.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace util::config {

class ClioConfigDefinition;

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

    /**
     * @brief Returns an iterator to the beginning of the values.
     *
     * @return Iterator to the beginning of the values.
     */
    [[nodiscard]] Array::ArrayIterator
    beginValues() const;

    /**
     * @brief Returns an iterator to the end of the values.
     *
     * @return Iterator to the end of the values.
     */
    [[nodiscard]] Array::Sentinel
    endValues() const;

private:
    std::string const prefix_;
    std::reference_wrapper<ClioConfigDefinition const> clioConfig_;
};

}  // namespace util::config
