//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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
