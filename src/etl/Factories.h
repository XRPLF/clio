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

#pragma once

#include <backend/BackendInterface.h>
#include <etl/ETLHelpers.h>
#include <etl/ETLSource.h>
#include <etl/ReportingETL.h>
#include <subscriptions/SubscriptionManager.h>
#include <util/config/Config.h>

#include <memory>

namespace clio::etl {
using namespace data;
using namespace subscription;
using namespace util;

static std::shared_ptr<ETLLoadBalancer>
make_ETLLoadBalancer(
    Config const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<NetworkValidatedLedgers> validatedLedgers)
{
    return std::make_shared<ETLLoadBalancer>(
        config, ioc, backend, subscriptions, validatedLedgers);
}

static std::shared_ptr<NetworkValidatedLedgers>
make_ValidatedLedgers()
{
    return std::make_shared<NetworkValidatedLedgers>();
}

static std::shared_ptr<ReportingETL>
make_ReportingETL(
    Config const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<ETLLoadBalancer> balancer,
    std::shared_ptr<NetworkValidatedLedgers> ledgers)
{
    return ReportingETL::make_ReportingETL(
        config, ioc, backend, subscriptions, balancer, ledgers);
}

}  // namespace clio::etl
