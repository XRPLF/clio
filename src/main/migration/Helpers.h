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

#include <backend/CassandraBackend.h>
#include <config/Config.h>
#include <log/Logger.h>

#include <ripple/basics/base64.h>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <cassandra.h>

#include <filesystem>
#include <optional>
#include <string>

static std::uint32_t const MAX_RETRIES = 5;
static std::chrono::seconds const WAIT_TIME = std::chrono::seconds(60);
static std::uint32_t const NFT_WRITE_BATCH_SIZE = 10000;

static void
wait(
    boost::asio::steady_timer& timer,
    std::string const& reason,
    std::chrono::seconds timeout = WAIT_TIME)
{
    clio::LogService::info() << reason << ". Waiting then retrying";
    timer.expires_after(timeout);
    timer.wait();
    clio::LogService::info() << "Done waiting";
}

static std::pair<std::string, std::string>
parseHostPort(std::string input)
{
    std::vector<std::string> components;
    boost::split(components, input, boost::is_any_of(":"));

    if (components.size() != 2)
        throw std::logic_error(
            "Host and port must be specified as `host:port` string. Got "
            "instead: `" +
            input + "`");

    return std::make_pair(components.at(0), components.at(1));
}

static std::optional<boost::json::object>
doRequestFromRippled(
    std::string repairAddress,
    boost::json::object const& request)
{
    auto const [ip, wsPort] = parseHostPort(repairAddress);
    clio::LogService::debug()
        << "Attempting to forward request to repair server. "
        << "request = " << boost::json::serialize(request);

    boost::json::object response;

    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace websocket = beast::websocket;
    namespace net = boost::asio;
    using tcp = boost::asio::ip::tcp;

    try
    {
        boost::asio::io_context ioc;
        tcp::resolver resolver{ioc};

        auto ws = std::make_unique<websocket::stream<beast::tcp_stream>>(ioc);
        auto const results = resolver.resolve(ip, wsPort);

        ws->next_layer().expires_after(std::chrono::seconds(15));
        ws->next_layer().connect(results);

        ws->handshake(ip, "/");
        ws->write(net::buffer(boost::json::serialize(request)));

        beast::flat_buffer buffer;
        ws->read(buffer);

        auto begin = static_cast<char const*>(buffer.data().data());
        auto end = begin + buffer.data().size();
        auto parsed = boost::json::parse(std::string(begin, end));

        if (!parsed.is_object())
        {
            clio::LogService::error()
                << "Error parsing response: " << std::string{begin, end};
            return {};
        }

        return parsed.as_object();
    }
    catch (std::exception const& e)
    {
        clio::LogService::fatal() << "Encountered exception : " << e.what();
        return {};
    }
}

static std::optional<boost::json::object>
requestFromRippled(
    boost::asio::steady_timer& timer,
    std::string repairAddress,
    boost::json::object const& request,
    std::uint32_t const attempts = 0)
{
    auto response = doRequestFromRippled(repairAddress, request);
    if (response.has_value())
        return response;

    if (attempts >= MAX_RETRIES)
        return std::nullopt;

    wait(timer, "Failed to request from rippled", std::chrono::seconds{1});
    return requestFromRippled(timer, repairAddress, request, attempts + 1);
}

static std::string
hexStringToBinaryString(std::string hex)
{
    auto blob = ripple::strUnHex(hex);
    std::string strBlob;
    for (auto c : *blob)
        strBlob += c;
    return strBlob;
}

static void
maybeWriteTransaction(
    std::shared_ptr<Backend::CassandraBackend> const& backend,
    std::optional<boost::json::object> const& tx)
{
    if (!tx.has_value())
        throw std::runtime_error("Could not repair transaction");

    auto package = tx.value();
    if (!package.contains("result") || !package.at("result").is_object() ||
        package.at("result").as_object().contains("error"))
        throw std::runtime_error("Received non-success response from rippled");

    auto data = package.at("result").as_object();

    auto const date = data.at("date").as_int64();
    auto const ledgerIndex = data.at("ledger_index").as_int64();
    auto hashStr = hexStringToBinaryString(data.at("hash").as_string().c_str());
    auto metaStr = hexStringToBinaryString(data.at("meta").as_string().c_str());
    auto txStr = hexStringToBinaryString(data.at("tx").as_string().c_str());

    backend->writeTransaction(
        std::move(hashStr),
        ledgerIndex,
        date,
        std::move(txStr),
        std::move(metaStr));
    backend->sync();
}

static void
repairCorruptedTx(
    boost::asio::steady_timer& timer,
    std::string repairAddress,
    std::shared_ptr<Backend::CassandraBackend> const& backend,
    ripple::uint256 const& hash)
{
    clio::LogService::info() << " - repairing " << hash;
    auto const data = requestFromRippled(
        timer,
        repairAddress,
        {
            {"method", "tx"},
            {"transaction", to_string(hash)},
            {"binary", true},
        });

    maybeWriteTransaction(backend, data);
}

static std::vector<NFTsData>
doNFTWrite(
    std::vector<NFTsData>& nfts,
    std::shared_ptr<Backend::CassandraBackend> const& backend,
    std::string const& tag)
{
    auto const size = nfts.size();
    if (size == 0)
        return nfts;
    backend->writeNFTs(std::move(nfts));
    backend->sync();
    clio::LogService::info() << tag << ": Wrote " << size << " records";
    return {};
}

static std::vector<NFTsData>
maybeDoNFTWrite(
    std::vector<NFTsData>& nfts,
    std::shared_ptr<Backend::CassandraBackend> const& backend,
    std::string const& tag)
{
    if (nfts.size() < NFT_WRITE_BATCH_SIZE)
        return nfts;
    return doNFTWrite(nfts, backend, tag);
}

static std::vector<Backend::TransactionAndMetadata>
doTryFetchTransactions(
    boost::asio::steady_timer& timer,
    std::shared_ptr<Backend::CassandraBackend> const& backend,
    std::vector<ripple::uint256> const& hashes,
    boost::asio::yield_context& yield,
    std::uint32_t const attempts = 0)
{
    try
    {
        return backend->fetchTransactions(hashes, yield);
    }
    catch (Backend::DatabaseTimeout const& e)
    {
        if (attempts >= MAX_RETRIES)
            throw e;

        wait(timer, "Transactions read error");
        return doTryFetchTransactions(
            timer, backend, hashes, yield, attempts + 1);
    }
}

static Backend::LedgerPage
doTryFetchLedgerPage(
    boost::asio::steady_timer& timer,
    std::shared_ptr<Backend::CassandraBackend> const& backend,
    std::optional<ripple::uint256> const& cursor,
    std::uint32_t const sequence,
    boost::asio::yield_context& yield,
    std::uint32_t const attempts = 0)
{
    try
    {
        return backend->fetchLedgerPage(cursor, sequence, 10000, false, yield);
    }
    catch (Backend::DatabaseTimeout const& e)
    {
        if (attempts >= MAX_RETRIES)
            throw e;

        wait(timer, "Page read error");
        return doTryFetchLedgerPage(
            timer, backend, cursor, sequence, yield, attempts + 1);
    }
}

static const CassResult*
doTryGetTxPageResult(
    CassStatement* const query,
    boost::asio::steady_timer& timer,
    std::shared_ptr<Backend::CassandraBackend> const& backend,
    std::uint32_t const attempts = 0)
{
    CassFuture* fut = cass_session_execute(backend->cautionGetSession(), query);
    CassResult const* result = cass_future_get_result(fut);
    cass_future_free(fut);

    if (result != nullptr)
        return result;

    if (attempts >= MAX_RETRIES)
        throw std::runtime_error("Already retried too many times");

    wait(timer, "Unexpected empty result from tx paging");
    return doTryGetTxPageResult(query, timer, backend, attempts + 1);
}
