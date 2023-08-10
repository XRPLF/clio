//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <data/cassandra/impl/ManagedObject.h>
#include <util/log/Logger.h>

#include <cassandra.h>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <variant>

namespace data::cassandra::detail {

struct Settings
{
    struct ContactPoints
    {
        std::string contactPoints = "127.0.0.1";  // defaults to localhost
        std::optional<uint16_t> port;
    };

    struct SecureConnectionBundle
    {
        std::string bundle;  // no meaningful default
    };

    bool enableLog = false;
    std::chrono::milliseconds connectionTimeout = std::chrono::milliseconds{10000};
    std::chrono::milliseconds requestTimeout = std::chrono::milliseconds{0};  // no timeout at all
    std::variant<ContactPoints, SecureConnectionBundle> connectionInfo = ContactPoints{};
    uint32_t threads = std::thread::hardware_concurrency();
    uint32_t maxWriteRequestsOutstanding = 10'000;
    uint32_t maxReadRequestsOutstanding = 100'000;
    uint32_t maxConnectionsPerHost = 2u;
    uint32_t coreConnectionsPerHost = 2u;
    uint32_t maxConcurrentRequestsThreshold =
        (maxWriteRequestsOutstanding + maxReadRequestsOutstanding) / coreConnectionsPerHost;
    std::optional<uint32_t> queueSizeEvent;
    std::optional<uint32_t> queueSizeIO;
    std::optional<uint32_t> writeBytesHighWatermark;
    std::optional<uint32_t> writeBytesLowWatermark;
    std::optional<uint32_t> pendingRequestsHighWatermark;
    std::optional<uint32_t> pendingRequestsLowWatermark;
    std::optional<uint32_t> maxRequestsPerFlush;
    std::optional<uint32_t> maxConcurrentCreation;
    std::optional<std::string> certificate;  // ssl context
    std::optional<std::string> username;
    std::optional<std::string> password;

    Settings
    withContactPoints(std::string_view contactPoints)
    {
        auto tmp = *this;
        tmp.connectionInfo = ContactPoints{std::string{contactPoints}};
        return tmp;
    }

    static Settings
    defaultSettings()
    {
        return Settings();
    }
};

class Cluster : public ManagedObject<CassCluster>
{
    util::Logger log_{"Backend"};

public:
    Cluster(Settings const& settings);

private:
    void
    setupConnection(Settings const& settings);

    void
    setupContactPoints(Settings::ContactPoints const& points);

    void
    setupSecureBundle(Settings::SecureConnectionBundle const& bundle);

    void
    setupCertificate(Settings const& settings);

    void
    setupCredentials(Settings const& settings);
};

}  // namespace data::cassandra::detail
