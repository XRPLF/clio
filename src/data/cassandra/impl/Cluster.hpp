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

#include "data/cassandra/impl/ManagedObject.h"
#include "util/log/Logger.h"

#include <cassandra.h>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <variant>

namespace data::cassandra::detail {

// TODO: move Settings to public interface, not detail

/**
 * @brief Bundles all cassandra settings in one place.
 */
struct Settings {
    static constexpr std::size_t DEFAULT_CONNECTION_TIMEOUT = 10000;
    static constexpr uint32_t DEFAULT_MAX_WRITE_REQUESTS_OUTSTANDING = 10'000;
    static constexpr uint32_t DEFAULT_MAX_READ_REQUESTS_OUTSTANDING = 100'000;
    static constexpr std::size_t DEFAULT_BATCH_SIZE = 20;

    /**
     * @brief Represents the configuration of contact points for cassandra.
     */
    struct ContactPoints {
        std::string contactPoints = "127.0.0.1";  // defaults to localhost
        std::optional<uint16_t> port = {};
    };

    /**
     * @brief Represents the configuration of a secure connection bundle.
     */
    struct SecureConnectionBundle {
        std::string bundle;  // no meaningful default
    };

    /** @brief Enables or disables cassandra driver logger */
    bool enableLog = false;

    /** @brief Connect timeout specified in milliseconds */
    std::chrono::milliseconds connectionTimeout = std::chrono::milliseconds{DEFAULT_CONNECTION_TIMEOUT};

    /** @brief Request timeout specified in milliseconds */
    std::chrono::milliseconds requestTimeout = std::chrono::milliseconds{0};  // no timeout at all

    /** @brief Connection information; either ContactPoints or SecureConnectionBundle */
    std::variant<ContactPoints, SecureConnectionBundle> connectionInfo = ContactPoints{};

    /** @brief The number of threads for the driver to pool */
    uint32_t threads = std::thread::hardware_concurrency();

    /** @brief The maximum number of outstanding write requests at any given moment */
    uint32_t maxWriteRequestsOutstanding = DEFAULT_MAX_WRITE_REQUESTS_OUTSTANDING;

    /** @brief The maximum number of outstanding read requests at any given moment */
    uint32_t maxReadRequestsOutstanding = DEFAULT_MAX_READ_REQUESTS_OUTSTANDING;

    /** @brief The number of connection per host to always have active */
    uint32_t coreConnectionsPerHost = 1u;

    /** @brief Size of batches when writing */
    std::size_t writeBatchSize = DEFAULT_BATCH_SIZE;

    /** @brief Size of the IO queue */
    std::optional<uint32_t> queueSizeIO{};

    /** @brief SSL certificate */
    std::optional<std::string> certificate{};  // ssl context

    /** @brief Username/login */
    std::optional<std::string> username{};

    /** @brief Password to match the `username` */
    std::optional<std::string> password{};

    /**
     * @brief Creates a new Settings object as a copy of the current one with overridden contact points.
     */
    Settings
    withContactPoints(std::string_view contactPoints)
    {
        auto tmp = *this;
        tmp.connectionInfo = ContactPoints{std::string{contactPoints}};
        return tmp;
    }

    /**
     * @brief Returns the default settings.
     */
    static Settings
    defaultSettings()
    {
        return Settings();
    }
};

class Cluster : public ManagedObject<CassCluster> {
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
