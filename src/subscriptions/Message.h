#pragma once

#include <string>

// This class should only be constructed once, then it can
// be read from in parallel by many websocket senders
class Message
{
    std::string message_;

public:
    Message() = delete;
    Message(std::string&& message) : message_(std::move(message))
    {
    }

    Message(Message const&) = delete;
    Message(Message&&) = delete;
    Message&
    operator=(Message const&) = delete;
    Message&
    operator=(Message&&) = delete;

    ~Message() = default;

    char*
    data()
    {
        return message_.data();
    }

    std::size_t
    size()
    {
        return message_.size();
    }
};
