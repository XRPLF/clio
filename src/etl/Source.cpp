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

#include <data/DBHelpers.h>
#include <etl/ETLService.h>
#include <etl/LoadBalancer.h>
#include <etl/ProbingSource.h>
#include <etl/Source.h>
#include <rpc/RPCHelpers.h>
#include <util/Profiler.h>

#include <ripple/beast/net/IPEndpoint.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <boost/asio/strand.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>

#include <thread>

namespace etl {

static boost::beast::websocket::stream_base::timeout
make_TimeoutOption()
{
    return boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client);
}

void
PlainSource::close(bool startAgain)
{
    timer_.cancel();
    boost::asio::post(strand_, [this, startAgain]() {
        if (closing_)
            return;

        if (derived().ws().is_open())
        {
            // onStop() also calls close(). If the async_close is called twice,
            // an assertion fails. Using closing_ makes sure async_close is only
            // called once
            closing_ = true;
            derived().ws().async_close(boost::beast::websocket::close_code::normal, [this, startAgain](auto ec) {
                if (ec)
                {
                    LOG(log_.error()) << " async_close : "
                                      << "error code = " << ec << " - " << toString();
                }
                closing_ = false;
                if (startAgain)
                {
                    ws_ = std::make_unique<StreamType>(strand_);
                    run();
                }
            });
        }
        else if (startAgain)
        {
            ws_ = std::make_unique<StreamType>(strand_);
            run();
        }
    });
}

void
SslSource::close(bool startAgain)
{
    timer_.cancel();
    boost::asio::post(strand_, [this, startAgain]() {
        if (closing_)
            return;

        if (derived().ws().is_open())
        {
            // onStop() also calls close(). If the async_close is called twice, an assertion fails. Using closing_ makes
            // sure async_close is only called once
            closing_ = true;
            derived().ws().async_close(boost::beast::websocket::close_code::normal, [this, startAgain](auto ec) {
                if (ec)
                {
                    LOG(log_.error()) << " async_close : "
                                      << "error code = " << ec << " - " << toString();
                }
                closing_ = false;
                if (startAgain)
                {
                    ws_ = std::make_unique<StreamType>(strand_, *sslCtx_);
                    run();
                }
            });
        }
        else if (startAgain)
        {
            ws_ = std::make_unique<StreamType>(strand_, *sslCtx_);
            run();
        }
    });
}

void
PlainSource::onConnect(
    boost::beast::error_code ec,
    boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
{
    if (ec)
    {
        // start over
        reconnect(ec);
    }
    else
    {
        connected_ = true;
        numFailures_ = 0;

        // Websocket stream has it's own timeout system
        boost::beast::get_lowest_layer(derived().ws()).expires_never();

        derived().ws().set_option(make_TimeoutOption());
        derived().ws().set_option(
            boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type& req) {
                req.set(boost::beast::http::field::user_agent, "clio-client");
                req.set("X-User", "clio-client");
            }));

        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        auto host = ip_ + ':' + std::to_string(endpoint.port());
        derived().ws().async_handshake(host, "/", [this](auto ec) { onHandshake(ec); });
    }
}

void
SslSource::onConnect(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
{
    if (ec)
    {
        // start over
        reconnect(ec);
    }
    else
    {
        connected_ = true;
        numFailures_ = 0;

        // Websocket stream has it's own timeout system
        boost::beast::get_lowest_layer(derived().ws()).expires_never();

        derived().ws().set_option(make_TimeoutOption());
        derived().ws().set_option(
            boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type& req) {
                req.set(boost::beast::http::field::user_agent, "clio-client");
                req.set("X-User", "clio-client");
            }));

        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        auto host = ip_ + ':' + std::to_string(endpoint.port());
        ws().next_layer().async_handshake(
            boost::asio::ssl::stream_base::client, [this, endpoint](auto ec) { onSslHandshake(ec, endpoint); });
    }
}

void
SslSource::onSslHandshake(
    boost::beast::error_code ec,
    boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
{
    if (ec)
    {
        reconnect(ec);
    }
    else
    {
        auto host = ip_ + ':' + std::to_string(endpoint.port());
        ws().async_handshake(host, "/", [this](auto ec) { onHandshake(ec); });
    }
}
}  // namespace etl
