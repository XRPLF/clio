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

#include "util/newconfig/ConfigFileJson.hpp"

#include "util/log/Logger.hpp"
#include "util/newconfig/ConfigValue.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/parse_options.hpp>
#include <ripple/protocol/jss.h>

#include <exception>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <string_view>

namespace util::config {

void
ConfigFileJson::parse(std::string_view configFilePath)
{
    try {
        std::ifstream const in(configFilePath, std::ios::in | std::ios::binary);
        if (in) {
            std::stringstream contents;
            contents << in.rdbuf();
            auto opts = boost::json::parse_options{};
            opts.allow_comments = true;
            object_ = boost::json::parse(contents.str(), {}, opts).as_object();
        }
    } catch (std::exception const& e) {
        LOG(util::LogService::error()) << "Could not read configuration file from '" << configFilePath
                                       << "': " << e.what();
    }
}

std::optional<ValueData<ConfigType>>
ConfigFileJson::getValue(std::string_view key) const
{
    if (!object_.contains(key))
        return std::nullopt;

    /*
    auto jsonValue = object_.at(key);
        if (jsonValue.is_int64()) {
            return ValueData<getType<int>()>{jsonValue.as_int64()};
        } else if (jsonValue.is_string()) {
            return ValueData<ConfigType::String>{jsonValue.as_string()};
        } else if (jsonValue.is_double()) {
            return ValueData<ConfigType::Float>{jsonValue.as_double()};
        } else if (jsonValue.is_bool()) {
            return ValueData<ConfigType::Boolean>{jsonValue.as_bool()};
        }

     */
    return std::nullopt;
}

}  // namespace util::config
