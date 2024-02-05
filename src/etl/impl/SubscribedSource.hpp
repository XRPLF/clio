//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "etl/ETLHelpers.hpp"
#include "util/Mutex.hpp"
#include "util/requests/WsConnection.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace etl::impl {

class SubscribedSource {
    util::Logger log_;
    util::requests::WsConnectionBuilder wsConnectionBuilder_;
    util::requests::WsConnectionPtr wsConnection_;

    util::Mutex<std::vector<std::pair<uint32_t, uint32_t>>> validatedLedgers_;
    std::string validatedLedgersRaw_{"N/A"};
    std::shared_ptr<NetworkValidatedLedgers> networkValidatedLedgers_;

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

public:
    SubscribedSource(
        boost::asio::io_context& ioContext,
        std::string const& ip,
        std::string const& wsPort,
        std::shared_ptr<NetworkValidatedLedgers> validatedLedgers
    );

    bool
    hasLedger(uint32_t sequence) const;

private:
    void
    subscribe();
};

}  // namespace etl::impl
