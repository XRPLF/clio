#pragma once

#include "web/ng/Error.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/system/detail/error_code.hpp>

#include <functional>
#include <optional>
#include <queue>

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

public:
    SendingQueue(Sender sender) : sender_{std::move(sender)}
    {
    }

    std::expected<void, Error>
    send(T message, boost::asio::yield_context yield)
    {
        if (error_)
            return std::unexpected{error_};

        queue_.push(std::move(message));
        if (isSending_)
            return {};

        isSending_ = true;
        while (not queue_.empty() and not error_) {
            auto const responseToSend = std::move(queue_.front());
            queue_.pop();
            sender_(responseToSend, yield[error_]);
        }
        isSending_ = false;
        if (error_)
            return std::unexpected{error_};
        return {};
    }
};

}  // namespace web::ng::impl
