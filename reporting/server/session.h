//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2020 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef RIPPLE_REPORTING_SESSION_H
#define RIPPLE_REPORTING_SESSION_H

#include <boost/asio/dispatch.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <reporting/ReportingETL.h>

#include <iostream>

class session;
class SubscriptionManager;

//------------------------------------------------------------------------------
enum RPCCommand {
    tx,
    account_tx,
    ledger,
    account_info,
    ledger_data,
    book_offers,
    ledger_range,
    ledger_entry,
    account_channels,
    account_lines,
    account_currencies,
    account_offers,
    account_objects,
    channel_authorize,
    channel_verify,
    subscribe,
    unsubscribe
};

static std::unordered_map<std::string, RPCCommand> commandMap{
    {"tx", tx},
    {"account_tx", account_tx},
    {"ledger", ledger},
    {"ledger_range", ledger_range},
    {"ledger_entry", ledger_entry},
    {"account_info", account_info},
    {"ledger_data", ledger_data},
    {"book_offers", book_offers},
    {"account_channels", account_channels},
    {"account_lines", account_lines},
    {"account_currencies", account_currencies},
    {"account_offers", account_offers},
    {"account_objects", account_objects},
    {"channel_authorize", channel_authorize},
    {"channel_verify", channel_verify},
    {"subscribe", subscribe},
    {"unsubscribe", unsubscribe}};

boost::json::object
doAccountInfo(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doTx(boost::json::object const& request, BackendInterface const& backend);
boost::json::object
doAccountTx(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doLedgerData(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doLedgerEntry(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doBookOffers(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doLedger(boost::json::object const& request, BackendInterface const& backend);
boost::json::object
doLedgerRange(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountChannels(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
<<<<<<< HEAD
doAccountLines(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountCurrencies(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountOffers(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountObjects(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doChannelAuthorize(boost::json::object const& request);
boost::json::object
doChannelVerify(boost::json::object const& request);
boost::json::object
=======
doLedger(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
>>>>>>> 8e6524b (add doLedger)
doSubscribe(
    boost::json::object const& request,
    std::shared_ptr<session>& session,
    SubscriptionManager& manager);
boost::json::object
doUnsubscribe(
    boost::json::object const& request,
    std::shared_ptr<session>& session,
    SubscriptionManager& manager);

boost::json::object
buildResponse(
    boost::json::object const& request, 
    BackendInterface const& backend,
    SubscriptionManager& manager,
    std::shared_ptr<session> session);

void
fail(boost::beast::error_code ec, char const* what);

// Echoes back all received WebSocket messages
class session : public std::enable_shared_from_this<session>
{
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
    boost::beast::flat_buffer buffer_;
    std::string response_;
    BackendInterface const& backend_;
    SubscriptionManager& subscriptions_;

public:
    // Take ownership of the socket
    explicit session(
        boost::asio::ip::tcp::socket&& socket,
        SubscriptionManager& subs,
        BackendInterface const& backend)
        : ws_(std::move(socket))
        , subscriptions_(subs)
        , backend_(backend)
    {
    }

    // Get on the correct executor
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        boost::asio::dispatch(
            ws_.get_executor(),
            boost::beast::bind_front_handler(
                &session::on_run, shared_from_this()));
    }

    // Start the asynchronous operation
    void
    on_run()
    {
        // Set suggested timeout settings for the websocket
        ws_.set_option(boost::beast::websocket::stream_base::timeout::suggested(
            boost::beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        ws_.set_option(boost::beast::websocket::stream_base::decorator(
            [](boost::beast::websocket::response_type& res) {
                res.set(
                    boost::beast::http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-server-async");
            }));
        // Accept the websocket handshake
        ws_.async_accept(boost::beast::bind_front_handler(
            &session::on_accept, shared_from_this()));
    }

    void
    on_accept(boost::beast::error_code ec)
    {
        if (ec)
            return fail(ec, "accept");

        // Read a message
        do_read();
    }

    void
    do_read()
    {
        // Read a message into our buffer
        ws_.async_read(
            buffer_,
            boost::beast::bind_front_handler(
                &session::on_read, shared_from_this()));
    }

    void
    on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This indicates that the session was closed
        if (ec == boost::beast::websocket::error::closed)
            return;

        if (ec)
            fail(ec, "read");
        std::string msg{
            static_cast<char const*>(buffer_.data().data()), buffer_.size()};
        // BOOST_LOG_TRIVIAL(debug) << __func__ << msg;
        boost::json::object response;
        try
        {
            boost::json::value raw = boost::json::parse(msg);
            boost::json::object request = raw.as_object();
            BOOST_LOG_TRIVIAL(debug) << " received request : " << request;
            try
            {
                response = buildResponse(
                    request, 
                    backend_,
                    subscriptions_,
                    shared_from_this());
            }
            catch (Backend::DatabaseTimeout const& t)
            {
                BOOST_LOG_TRIVIAL(error) << __func__ << " Database timeout";
                response["error"] =
                    "Database read timeout. Please retry the request";
            }
        }
        catch (std::exception const& e)
        {
            BOOST_LOG_TRIVIAL(error)
                << __func__ << "caught exception : " << e.what();
        }
        BOOST_LOG_TRIVIAL(trace) << __func__ << response;
        response_ = boost::json::serialize(response);

        // Echo the message
        ws_.text(ws_.got_text());
        ws_.async_write(
            boost::asio::buffer(response_),
            boost::beast::bind_front_handler(
                &session::on_write, shared_from_this()));
    }

    void
    send(std::string&& msg)
    {
        ws_.text(ws_.got_text());
        ws_.async_write(
            boost::asio::buffer(msg),
            boost::beast::bind_front_handler(
                &session::on_write, shared_from_this()));
    }

    void
    on_write(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        // Clear the buffer
        buffer_.consume(buffer_.size());

        // Do another read
        do_read();
    }
};

#endif // RIPPLE_REPORTING_SESSION_H