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

#include <log/Logger.h>

#include <ripple/ledger/ReadView.h>
#include <boost/algorithm/string.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/websocket.hpp>
#include "org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h"
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <mutex>
#include <thread>

namespace clio::detail {

/**
 * @brief Cache loading interface
 */
template <typename CacheType>
class CacheLoader
{
    enum class LoadStyle { ASYNC, SYNC, NOT_AT_ALL };

    clio::Logger log_{"ETL"};

    std::reference_wrapper<boost::asio::io_context> ioContext_;
    std::shared_ptr<BackendInterface> backend_;
    std::reference_wrapper<CacheType> cache_;
    LoadStyle cacheLoadStyle_ = LoadStyle::ASYNC;

    // number of diffs to use to generate cursors to traverse the ledger in parallel during initial cache download
    size_t numCacheDiffs_ = 32;

    // number of markers to use at one time to traverse the ledger in parallel during initial cache download
    size_t numCacheMarkers_ = 48;

    // number of ledger objects to fetch concurrently per marker during cache download
    size_t cachePageFetchSize_ = 512;

    struct ClioPeer
    {
        std::string ip;
        int port;
    };

    std::vector<ClioPeer> clioPeers_;

    std::thread thread_;
    std::atomic_bool stopping_ = false;

public:
    CacheLoader(
        clio::Config const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> const& backend,
        CacheType& ledgerCache)
        : ioContext_{std::ref(ioc)}, backend_{backend}, cache_{ledgerCache}
    {
        if (config.contains("cache"))
        {
            auto const cache = config.section("cache");
            if (auto entry = cache.maybeValue<std::string>("load"); entry)
            {
                if (boost::iequals(*entry, "sync"))
                    cacheLoadStyle_ = LoadStyle::SYNC;
                if (boost::iequals(*entry, "async"))
                    cacheLoadStyle_ = LoadStyle::ASYNC;
                if (boost::iequals(*entry, "none") or boost::iequals(*entry, "no"))
                    cacheLoadStyle_ = LoadStyle::NOT_AT_ALL;
            }

            numCacheDiffs_ = cache.valueOr<size_t>("num_diffs", numCacheDiffs_);
            numCacheMarkers_ = cache.valueOr<size_t>("num_markers", numCacheMarkers_);
            cachePageFetchSize_ = cache.valueOr<size_t>("page_fetch_size", cachePageFetchSize_);

            if (auto peers = cache.maybeArray("peers"); peers)
            {
                for (auto const& peer : *peers)
                {
                    auto ip = peer.value<std::string>("ip");
                    auto port = peer.value<uint32_t>("port");

                    // todo: use emplace_back when clang is ready
                    clioPeers_.push_back({ip, port});
                }

                unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
                std::shuffle(std::begin(clioPeers_), std::end(clioPeers_), std::default_random_engine(seed));
            }
        }
    }

    ~CacheLoader()
    {
        stop();
        if (thread_.joinable())
            thread_.join();
    }

    /**
     * @brief Populates the cache by walking through the given ledger.
     *
     * Should only be called once. The default behavior is to return immediately and populate the cache in the
     * background. This can be overridden via config parameter, to populate synchronously, or not at all.
     */
    void
    load(uint32_t seq)
    {
        if (cacheLoadStyle_ == LoadStyle::NOT_AT_ALL)
        {
            cache_.get().setDisabled();
            log_.warn() << "Cache is disabled. Not loading";
            return;
        }

        if (cache_.get().isFull())
        {
            assert(false);
            return;
        }

        if (clioPeers_.size() > 0)
        {
            boost::asio::spawn(ioContext_.get(), [this, seq](boost::asio::yield_context yield) {
                for (auto const& peer : clioPeers_)
                {
                    // returns true on success
                    if (loadCacheFromClioPeer(seq, peer.ip, std::to_string(peer.port), yield))
                        return;
                }

                // if we couldn't successfully load from any peers, load from db
                loadCacheFromDb(seq);
            });
            return;
        }
        else
        {
            loadCacheFromDb(seq);
        }

        // If loading synchronously, poll cache until full
        while (cacheLoadStyle_ == LoadStyle::SYNC && not cache_.get().isFull())
        {
            log_.debug() << "Cache not full. Cache size = " << cache_.get().size() << ". Sleeping ...";
            std::this_thread::sleep_for(std::chrono::seconds(10));
            if (cache_.get().isFull())
                log_.info() << "Cache is full. Cache size = " << cache_.get().size();
        }
    }

    void
    stop()
    {
        stopping_ = true;
    }

private:
    bool
    loadCacheFromClioPeer(
        uint32_t ledgerIndex,
        std::string const& ip,
        std::string const& port,
        boost::asio::yield_context& yield)
    {
        log_.info() << "Loading cache from peer. ip = " << ip << " . port = " << port;
        namespace beast = boost::beast;          // from <boost/beast.hpp>
        namespace http = beast::http;            // from <boost/beast/http.hpp>
        namespace websocket = beast::websocket;  // from
        namespace net = boost::asio;             // from
        using tcp = boost::asio::ip::tcp;        // from
        try
        {
            boost::beast::error_code ec;
            // These objects perform our I/O
            tcp::resolver resolver{ioContext_.get()};

            log_.trace() << "Creating websocket";
            auto ws = std::make_unique<websocket::stream<beast::tcp_stream>>(ioContext_.get());

            // Look up the domain name
            auto const results = resolver.async_resolve(ip, port, yield[ec]);
            if (ec)
                return {};

            log_.trace() << "Connecting websocket";
            // Make the connection on the IP address we get from a lookup
            ws->next_layer().async_connect(results, yield[ec]);
            if (ec)
                return false;

            log_.trace() << "Performing websocket handshake";
            // Perform the websocket handshake
            ws->async_handshake(ip, "/", yield[ec]);
            if (ec)
                return false;

            std::optional<boost::json::value> marker;

            log_.trace() << "Sending request";
            auto getRequest = [&](auto marker) {
                boost::json::object request = {
                    {"command", "ledger_data"},
                    {"ledger_index", ledgerIndex},
                    {"binary", true},
                    {"out_of_order", true},
                    {"limit", 2048}};

                if (marker)
                    request["marker"] = *marker;
                return request;
            };

            bool started = false;
            size_t numAttempts = 0;
            do
            {
                // Send the message
                ws->async_write(net::buffer(boost::json::serialize(getRequest(marker))), yield[ec]);
                if (ec)
                {
                    log_.error() << "error writing = " << ec.message();
                    return false;
                }

                beast::flat_buffer buffer;
                ws->async_read(buffer, yield[ec]);
                if (ec)
                {
                    log_.error() << "error reading = " << ec.message();
                    return false;
                }

                auto raw = beast::buffers_to_string(buffer.data());
                auto parsed = boost::json::parse(raw);

                if (!parsed.is_object())
                {
                    log_.error() << "Error parsing response: " << raw;
                    return false;
                }
                log_.trace() << "Successfully parsed response " << parsed;

                if (auto const& response = parsed.as_object(); response.contains("error"))
                {
                    log_.error() << "Response contains error: " << response;
                    auto const& err = response.at("error");
                    if (err.is_string() && err.as_string() == "lgrNotFound")
                    {
                        ++numAttempts;
                        if (numAttempts >= 5)
                        {
                            log_.error() << " ledger not found at peer after 5 attempts. "
                                            "peer = "
                                         << ip << " ledger = " << ledgerIndex
                                         << ". Check your config and the health of the peer";
                            return false;
                        }
                        log_.warn() << "Ledger not found. ledger = " << ledgerIndex << ". Sleeping and trying again";
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        continue;
                    }
                    return false;
                }
                started = true;
                auto const& response = parsed.as_object()["result"].as_object();

                if (!response.contains("cache_full") || !response.at("cache_full").as_bool())
                {
                    log_.error() << "cache not full for clio node. ip = " << ip;
                    return false;
                }
                if (response.contains("marker"))
                    marker = response.at("marker");
                else
                    marker = {};

                auto const& state = response.at("state").as_array();

                std::vector<Backend::LedgerObject> objects;
                objects.reserve(state.size());
                for (auto const& ledgerObject : state)
                {
                    auto const& obj = ledgerObject.as_object();

                    Backend::LedgerObject stateObject = {};

                    if (!stateObject.key.parseHex(obj.at("index").as_string().c_str()))
                    {
                        log_.error() << "failed to parse object id";
                        return false;
                    }
                    boost::algorithm::unhex(obj.at("data").as_string().c_str(), std::back_inserter(stateObject.blob));
                    objects.push_back(std::move(stateObject));
                }
                cache_.get().update(objects, ledgerIndex, true);

                if (marker)
                    log_.debug() << "At marker " << *marker;
            } while (marker || !started);

            log_.info() << "Finished downloading ledger from clio node. ip = " << ip;

            cache_.get().setFull();
            return true;
        }
        catch (std::exception const& e)
        {
            log_.error() << "Encountered exception : " << e.what() << " - ip = " << ip;
            return false;
        }
    }

    void
    loadCacheFromDb(uint32_t seq)
    {
        std::vector<Backend::LedgerObject> diff;
        std::vector<std::optional<ripple::uint256>> cursors;

        auto append = [](auto&& a, auto&& b) { a.insert(std::end(a), std::begin(b), std::end(b)); };

        for (size_t i = 0; i < numCacheDiffs_; ++i)
        {
            append(diff, Backend::synchronousAndRetryOnTimeout([&](auto yield) {
                       return backend_->fetchLedgerDiff(seq - i, yield);
                   }));
        }

        std::sort(diff.begin(), diff.end(), [](auto a, auto b) {
            return a.key < b.key || (a.key == b.key && a.blob.size() < b.blob.size());
        });

        diff.erase(std::unique(diff.begin(), diff.end(), [](auto a, auto b) { return a.key == b.key; }), diff.end());

        cursors.push_back({});
        for (auto& obj : diff)
            if (obj.blob.size())
                cursors.push_back({obj.key});
        cursors.push_back({});

        std::stringstream cursorStr;
        for (auto& c : cursors)
            if (c)
                cursorStr << ripple::strHex(*c) << ", ";

        log_.info() << "Loading cache. num cursors = " << cursors.size() - 1;
        log_.trace() << "cursors = " << cursorStr.str();

        thread_ = std::thread{[this, seq, cursors]() {
            auto startTime = std::chrono::system_clock::now();
            auto markers = std::make_shared<std::atomic_int>(0);
            auto numRemaining = std::make_shared<std::atomic_int>(cursors.size() - 1);

            for (size_t i = 0; i < cursors.size() - 1; ++i)
            {
                auto const start = cursors[i];
                auto const end = cursors[i + 1];

                markers->wait(numCacheMarkers_);
                ++(*markers);

                boost::asio::spawn(
                    ioContext_.get(),
                    [this, seq, start, end, numRemaining, startTime, markers](boost::asio::yield_context yield) {
                        std::optional<ripple::uint256> cursor = start;
                        std::string cursorStr =
                            cursor.has_value() ? ripple::strHex(cursor.value()) : ripple::strHex(Backend::firstKey);
                        log_.debug() << "Starting a cursor: " << cursorStr << " markers = " << *markers;

                        while (not stopping_)
                        {
                            auto res = Backend::retryOnTimeout([this, seq, &cursor, &yield]() {
                                return backend_->fetchLedgerPage(cursor, seq, cachePageFetchSize_, false, yield);
                            });

                            cache_.get().update(res.objects, seq, true);

                            if (!res.cursor || (end && *(res.cursor) > *end))
                                break;

                            log_.trace() << "Loading cache. cache size = " << cache_.get().size()
                                         << " - cursor = " << ripple::strHex(res.cursor.value())
                                         << " start = " << cursorStr << " markers = " << *markers;

                            cursor = std::move(res.cursor);
                        }

                        --(*markers);
                        markers->notify_one();

                        if (--(*numRemaining) == 0)
                        {
                            auto endTime = std::chrono::system_clock::now();
                            auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);

                            log_.info() << "Finished loading cache. cache size = " << cache_.get().size() << ". Took "
                                        << duration.count() << " seconds";
                            cache_.get().setFull();
                        }
                        else
                        {
                            log_.info() << "Finished a cursor. num remaining = " << *numRemaining
                                        << " start = " << cursorStr << " markers = " << *markers;
                        }
                    });
            }
        }};
    }
};

}  // namespace clio::detail
