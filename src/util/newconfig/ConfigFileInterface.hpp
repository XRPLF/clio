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
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Object.hpp"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>
=======
#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigValue.hpp"

#include <optional>
#include <string_view>
>>>>>>> e62e648 (first draft of config)

namespace util::config {

/** @brief The interface for config Json */
class ConfigFileInterface {
public:
    virtual ~ConfigFileInterface() = default;

    virtual void parse(std::string_view) = 0;
<<<<<<< HEAD
    virtual std::optional<ConfigValue> getValue(std::string_view) const = 0;
    virtual std::optional<std::vector<std::pair<std::string, ConfigValue>>> getArray(std::string_view) const = 0;
=======
    virtual std::optional<ValueData<ConfigType>> getValue(std::string_view) const = 0;
    virtual std::optional<Array<ConfigType>> getArray(std::string_view) const = 0;
>>>>>>> e62e648 (first draft of config)
};

}  // namespace util::config
