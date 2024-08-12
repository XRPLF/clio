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

#include "util/newconfig/ConfigFileInterface.hpp"

#include <boost/filesystem/path.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// TODO: implement when we support yaml

namespace util::config {

/** @brief Yaml representation of config */
class ConfigFileYaml final : public ConfigFileInterface {
public:
    ConfigFileYaml() = default;

    void
    parse(boost::filesystem::path filePath) override;

    std::variant<int64_t, std::string, bool, double>
    getValue(std::string_view key) const override;

    std::vector<std::variant<int64_t, std::string, bool, double>>
    getArray(std::string_view key) const override;

    bool
    containsKey(std::string_view key) const override;
};

}  // namespace util::config
