//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include "util/config/Config.hpp"

#include "util/config/detail/Helpers.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/parse_options.hpp>
#include <boost/json/value.hpp>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace util {

// Note: `store_(store)` MUST use `()` instead of `{}` otherwise gcc
// picks `initializer_list` constructor and anything passed becomes an
// array :-D
Config::Config(boost::json::value store) : store_(std::move(store))
{
}

Config::operator bool() const noexcept
{
    return not store_.is_null();
}

bool
Config::contains(KeyType key) const
{
    return lookup(key).has_value();
}

std::optional<boost::json::value>
Config::lookup(KeyType key) const
{
    if (store_.is_null())
        return std::nullopt;

    std::reference_wrapper<boost::json::value const> cur = std::cref(store_);
    auto hasBrokenPath = false;
    auto tokenized = detail::Tokenizer<KeyType, Separator>{key};
    std::string subkey{};

    auto maybeSection = tokenized.next();
    while (maybeSection.has_value()) {
        auto section = maybeSection.value();
        subkey += section;

        if (not hasBrokenPath) {
            if (not cur.get().is_object())
                throw detail::StoreException("Not an object at '" + subkey + "'");
            if (not cur.get().as_object().contains(section)) {
                hasBrokenPath = true;
            } else {
                cur = std::cref(cur.get().as_object().at(section));
            }
        }

        subkey += Separator;
        maybeSection = tokenized.next();
    }

    if (hasBrokenPath)
        return std::nullopt;
    return std::make_optional(cur);
}

std::optional<Config::ArrayType>
Config::maybeArray(KeyType key) const
{
    try {
        auto maybe_arr = lookup(key);
        if (maybe_arr && maybe_arr->is_array()) {
            auto& arr = maybe_arr->as_array();
            ArrayType out;
            out.reserve(arr.size());

            std::transform(std::begin(arr), std::end(arr), std::back_inserter(out), [](auto&& element) {
                return Config{std::forward<decltype(element)>(element)};
            });
            return std::make_optional<ArrayType>(std::move(out));
        }
    } catch (detail::StoreException const&) {  // NOLINT(bugprone-empty-catch)
        // ignore store error, but rethrow key errors
    }

    return std::nullopt;
}

Config::ArrayType
Config::array(KeyType key) const
{
    if (auto maybe_arr = maybeArray(key); maybe_arr)
        return maybe_arr.value();
    throw std::logic_error("No array found at '" + key + "'");
}

Config::ArrayType
Config::arrayOr(KeyType key, ArrayType fallback) const
{
    if (auto maybe_arr = maybeArray(key); maybe_arr)
        return maybe_arr.value();
    return fallback;
}

Config::ArrayType
Config::arrayOrThrow(KeyType key, std::string_view err) const
{
    try {
        return maybeArray(key).value();
    } catch (std::exception const&) {
        throw std::runtime_error(err.data());
    }
}

Config
Config::section(KeyType key) const
{
    auto maybe_element = lookup(key);
    if (maybe_element && maybe_element->is_object())
        return Config{std::move(*maybe_element)};
    throw std::logic_error("No section found at '" + key + "'");
}

Config
Config::sectionOr(KeyType key, boost::json::object fallback) const
{
    auto maybe_element = lookup(key);
    if (maybe_element && maybe_element->is_object())
        return Config{std::move(*maybe_element)};
    return Config{std::move(fallback)};
}

Config::ArrayType
Config::array() const
{
    if (not store_.is_array())
        throw std::logic_error("_self_ is not an array");

    ArrayType out;
    auto const& arr = store_.as_array();
    out.reserve(arr.size());

    std::transform(std::cbegin(arr), std::cend(arr), std::back_inserter(out), [](auto const& element) {
        return Config{element};
    });
    return out;
}

Config
ConfigReader::open(std::filesystem::path path)
{
    try {
        std::ifstream const in(path, std::ios::in | std::ios::binary);
        if (in) {
            std::stringstream contents;
            contents << in.rdbuf();
            auto opts = boost::json::parse_options{};
            opts.allow_comments = true;
            return Config{boost::json::parse(contents.str(), {}, opts)};
        }
    } catch (std::exception const& e) {
        LOG(util::LogService::error()) << "Could not read configuration file from '" << path.string()
                                       << "': " << e.what();
    }

    return Config{};
}

}  // namespace util
