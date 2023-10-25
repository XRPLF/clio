//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <string>
#include <vector>

namespace util::prometheus {

/**
 * @brief Class representing a Prometheus label
 */
class Label {
public:
    Label(std::string name, std::string value);

    bool
    operator<(Label const& rhs) const;

    bool
    operator==(Label const& rhs) const;

    /**
     * @brief Serialize the label to a string in Prometheus format (e.g. name="value"). The value is escaped
     *
     * @return The serialized label
     */
    std::string
    serialize() const;

private:
    std::string name_;
    std::string value_;
};

/**
 * @brief Class representing a collection of Prometheus labels
 */
class Labels {
public:
    Labels() = default;
    explicit Labels(std::vector<Label> labels);

    /**
     * @brief Serialize the labels to a string in Prometheus format (e.g. {"name1="value1",name2="value2"})
     *
     * @return The serialized labels
     */
    std::string
    serialize() const;

private:
    std::vector<Label> labels_;
};

}  // namespace util::prometheus
