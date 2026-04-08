#pragma once

#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <gmock/gmock.h>

#include <chrono>
#include <string>

struct MockCounters {
    MOCK_METHOD(void, rpcFailed, (std::string const&), ());
    MOCK_METHOD(void, rpcErrored, (std::string const&), ());
    MOCK_METHOD(void, rpcComplete, (std::string const&, std::chrono::microseconds const&), ());
    MOCK_METHOD(void, rpcForwarded, (std::string const&), ());
    MOCK_METHOD(void, rpcFailedToForward, (std::string const&), ());
    MOCK_METHOD(void, onTooBusy, (), ());
    MOCK_METHOD(void, onNotReady, (), ());
    MOCK_METHOD(void, onBadSyntax, (), ());
    MOCK_METHOD(void, onUnknownCommand, (), ());
    MOCK_METHOD(void, onInternalError, (), ());
    MOCK_METHOD(boost::json::object, report, (), (const));
    MOCK_METHOD(std::chrono::seconds, uptime, (), (const));
};
