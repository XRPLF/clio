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

#include "etl/impl/ForwardCache.h"
#include "etl/Source.h"
#include "rpc/RPCHelpers.h"
#include "util/log/Logger.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>

namespace etl::detail {

void
ForwardCache::freshen()
{
    LOG(log_.trace()) << "Freshening ForwardCache";

    auto numOutstanding = std::make_shared<std::atomic_uint>(latestForwarded_.size());

    for (auto const& cacheEntry : latestForwarded_) {
        boost::asio::spawn(
            strand_,
            [this, numOutstanding, command = cacheEntry.first](boost::asio::yield_context yield) {
                boost::json::object const request = {{"command", command}};
                auto resp = source_.requestFromRippled(request, std::nullopt, yield);

                if (!resp || resp->contains("error"))
                    resp = {};

                {
                    std::scoped_lock const lk(mtx_);
                    latestForwarded_[command] = resp;
                }
            }
        );
    }
}

void
ForwardCache::clear()
{
    std::scoped_lock const lk(mtx_);
    for (auto& cacheEntry : latestForwarded_)
        latestForwarded_[cacheEntry.first] = {};
}

std::optional<boost::json::object>
ForwardCache::get(boost::json::object const& request) const
{
    std::optional<std::string> command = {};
    if (request.contains("command") && !request.contains("method") && request.at("command").is_string()) {
        command = request.at("command").as_string().c_str();
    } else if (request.contains("method") && !request.contains("command") && request.at("method").is_string()) {
        command = request.at("method").as_string().c_str();
    }

    if (!command)
        return {};
    if (rpc::specifiesCurrentOrClosedLedger(request))
        return {};

    std::shared_lock const lk(mtx_);
    if (!latestForwarded_.contains(*command))
        return {};

    return {latestForwarded_.at(*command)};
}

}  // namespace etl::detail
