//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace app {

/**
 * @brief Verifies user's config values are correct
 *
 * @param configPath The path to config
 * @return true if config values are all correct, false otherwise
 */
inline bool
verifyConfig(std::string_view configPath)
{
    using namespace util::config;

    auto const json = ConfigFileJson::makeConfigFileJson(configPath);
    if (!json.has_value()) {
        std::cerr << json.error().error << std::endl;
        return false;
    }
    auto const errors = gClioConfig.parse(json.value());
    if (errors.has_value()) {
        for (auto const& err : errors.value())
            std::cerr << err.error << std::endl;
        return false;
    }
    return true;
}
}  // namespace app
