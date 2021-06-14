#ifndef RIPPLE_REPORTING_WS_BASE_SESSION_H
#define RIPPLE_REPORTING_WS_BASE_SESSION_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>

namespace http = boost::beast::http;

inline void
wsFail(boost::beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Echoes back all received WebSocket messages
class WsBase
{

public:
    // Send, that enables SubscriptionManager to publish to clients
    virtual void
    send(std::string&& msg) = 0;

};
#endif // RIPPLE_REPORTING_WS_BASE_SESSION_H