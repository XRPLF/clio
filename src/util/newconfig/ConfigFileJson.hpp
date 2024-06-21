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

#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigFileInterface.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Errors.hpp"

#include <boost/json/object.hpp>

#include <optional>
#include <string_view>

namespace util::config {

/** @brief Json representation of config */
class ConfigFileJson final : public ConfigFileInterface {
public:
    ConfigFileJson() = default;

    void
    parse(std::string_view configFilePath) override;
    std::optional<ValueData<ConfigType>>
    getValue(std::string_view key) const override;
    std::optional<Array<ConfigType>>
    getArray(std::string_view key) const override;

private:
    // ValueData<ConfigType> getType(const boost::json::value& jsonValue) const;
    boost::json::object object_;
};

}  // namespace util::config
