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

#include <tuple>

namespace util::config {

/**
 * @brief Object definition for Json/Yaml config
 *
 * Used in ClioConfigDefinition to represent key-value(s) pair.
 */
template <typename Key, typename... Args>
class Object {
public:
    constexpr Object(Key key, Args... args) : key_{key}, fields_{args...}
    {
    }
    constexpr Key&
    key() const
    {
        return key_;
    }
    constexpr std::tuple<Args...>&
    Val() const
    {
        return fields_;
    }

private:
    Key key_;
    std::tuple<Args...> fields_{};
};

}  // namespace util::config
