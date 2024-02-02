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

#pragma once

#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/json.hpp>
#include <boost/json/object.hpp>

#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace etl {
class Source;
}  // namespace etl

namespace etl::detail {

/**
 * @brief Cache for rippled responses
 */
class ForwardCache {
    using ResponseType = std::optional<boost::json::object>;
    static constexpr std::uint32_t DEFAULT_DURATION = 10;

    util::Logger log_{"ETL"};

    mutable std::shared_mutex mtx_;
    std::unordered_map<std::string, ResponseType> latestForwarded_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    etl::Source const& source_;
    std::uint32_t duration_ = DEFAULT_DURATION;

    void
    clear();

public:
    ForwardCache(util::Config const& config, boost::asio::io_context& ioc, Source const& source)
        : strand_(boost::asio::make_strand(ioc)), source_(source)
    {
        if (config.contains("cache")) {
            auto commands = config.arrayOrThrow("cache", "Source cache must be array");

            if (config.contains("cache_duration"))
                duration_ = config.valueOrThrow<uint32_t>("cache_duration", "Source cache_duration must be a number");

            for (auto const& command : commands) {
                auto key = command.valueOrThrow<std::string>("Source forward command must be array of strings");
                latestForwarded_[key] = {};
            }
        }
    }

    void
    freshen();

    std::optional<boost::json::object>
    get(boost::json::object const& request) const;
};

}  // namespace etl::detail
