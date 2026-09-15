#pragma once

#include "web/ng/Error.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/system/detail/error_code.hpp>

#include <cstddef>
#include <expected>
#include <functional>
#include <queue>
#include <utility>

namespace web::ng::impl {

template <typename T>
class SendingQueue {
public:
    using Sender = std::function<
        void(T const&, boost::asio::basic_yield_context<boost::asio::any_io_executor>)>;

private:
    std::queue<T> queue_;
    Sender sender_;
    Error error_;
    bool isSending_{false};
    size_t maxSize_;

public:
    SendingQueue(Sender sender, size_t maxSize) : sender_{std::move(sender)}, maxSize_{maxSize}
    {
    }

    std::expected<void, Error>
    send(T message, boost::asio::yield_context yield)
    {
        if (error_)
            return std::unexpected{error_};

        if (queue_.size() >= maxSize_) {
            error_ = boost::asio::error::timed_out;
            return std::unexpected{error_};
        }

        queue_.push(std::move(message));
        if (isSending_)
            return {};

        isSending_ = true;
        while (not queue_.empty() and not error_) {
            auto const responseToSend = std::move(queue_.front());
            queue_.pop();

            Error writeError;
            sender_(responseToSend, yield[writeError]);
            if (writeError)
                error_ = writeError;
        }
        isSending_ = false;
        if (error_)
            return std::unexpected{error_};
        return {};
    }
};

}  // namespace web::ng::impl
