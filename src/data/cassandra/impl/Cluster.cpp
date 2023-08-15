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

#include <data/cassandra/impl/Cluster.h>
#include <data/cassandra/impl/SslContext.h>
#include <data/cassandra/impl/Statement.h>
#include <util/Expected.h>

#include <fmt/core.h>

#include <exception>
#include <vector>

namespace {
static constexpr auto clusterDeleter = [](CassCluster* ptr) { cass_cluster_free(ptr); };

template <class... Ts>
struct overloadSet : Ts...
{
    using Ts::operator()...;
};

// explicit deduction guide (not needed as of C++20, but clang be clang)
template <class... Ts>
overloadSet(Ts...) -> overloadSet<Ts...>;
};  // namespace

namespace data::cassandra::detail {

Cluster::Cluster(Settings const& settings) : ManagedObject{cass_cluster_new(), clusterDeleter}
{
    using std::to_string;

    cass_cluster_set_token_aware_routing(*this, cass_true);
    if (auto const rc = cass_cluster_set_protocol_version(*this, CASS_PROTOCOL_VERSION_V4); rc != CASS_OK)
    {
        throw std::runtime_error(
            fmt::format("Error setting cassandra protocol version to v4: {}", cass_error_desc(rc)));
    }

    if (auto const rc = cass_cluster_set_num_threads_io(*this, settings.threads); rc != CASS_OK)
    {
        throw std::runtime_error(
            fmt::format("Error setting cassandra io threads to {}: {}", settings.threads, cass_error_desc(rc)));
    }

    cass_log_set_level(settings.enableLog ? CASS_LOG_TRACE : CASS_LOG_DISABLED);
    cass_cluster_set_connect_timeout(*this, settings.connectionTimeout.count());
    cass_cluster_set_request_timeout(*this, settings.requestTimeout.count());

    if (auto const rc =
            cass_cluster_set_max_concurrent_requests_threshold(*this, settings.maxConcurrentRequestsThreshold);
        rc != CASS_OK)
    {
        throw std::runtime_error(
            fmt::format("Could not set max concurrent requests per host threshold: {}", cass_error_desc(rc)));
    }

    if (auto const rc = cass_cluster_set_max_connections_per_host(*this, settings.maxConnectionsPerHost); rc != CASS_OK)
    {
        throw std::runtime_error(fmt::format("Could not set max connections per host: {}", cass_error_desc(rc)));
    }

    if (auto const rc = cass_cluster_set_core_connections_per_host(*this, settings.coreConnectionsPerHost);
        rc != CASS_OK)
    {
        throw std::runtime_error(fmt::format("Could not set core connections per host: {}", cass_error_desc(rc)));
    }

    auto const queueSize =
        settings.queueSizeIO.value_or(settings.maxWriteRequestsOutstanding + settings.maxReadRequestsOutstanding);
    if (auto const rc = cass_cluster_set_queue_size_io(*this, queueSize); rc != CASS_OK)
    {
        throw std::runtime_error(fmt::format("Could not set queue size for IO per host: {}", cass_error_desc(rc)));
    }

    auto apply = []<typename ValueType, typename Fn>(
        std::optional<ValueType> const& maybeValue, Fn&& fn) requires std::is_object_v<Fn>
    {
        if (maybeValue)
            std::invoke(fn, maybeValue.value());
    };

    apply(settings.queueSizeEvent, [this](auto value) {
        if (auto const rc = cass_cluster_set_queue_size_event(*this, value); rc != CASS_OK)
            throw std::runtime_error(
                fmt::format("Could not set queue size for events per host: {}", cass_error_desc(rc)));
    });

    apply(settings.writeBytesHighWatermark, [this](auto value) {
        if (auto const rc = cass_cluster_set_write_bytes_high_water_mark(*this, value); rc != CASS_OK)
            throw std::runtime_error(fmt::format("Could not set write bytes high water_mark: {}", cass_error_desc(rc)));
    });

    apply(settings.writeBytesLowWatermark, [this](auto value) {
        if (auto const rc = cass_cluster_set_write_bytes_low_water_mark(*this, value); rc != CASS_OK)
            throw std::runtime_error(fmt::format("Could not set write bytes low water mark: {}", cass_error_desc(rc)));
    });

    apply(settings.pendingRequestsHighWatermark, [this](auto value) {
        if (auto const rc = cass_cluster_set_pending_requests_high_water_mark(*this, value); rc != CASS_OK)
            throw std::runtime_error(
                fmt::format("Could not set pending requests high water mark: {}", cass_error_desc(rc)));
    });

    apply(settings.pendingRequestsLowWatermark, [this](auto value) {
        if (auto const rc = cass_cluster_set_pending_requests_low_water_mark(*this, value); rc != CASS_OK)
            throw std::runtime_error(
                fmt::format("Could not set pending requests low water mark: {}", cass_error_desc(rc)));
    });

    apply(settings.maxRequestsPerFlush, [this](auto value) {
        if (auto const rc = cass_cluster_set_max_requests_per_flush(*this, value); rc != CASS_OK)
            throw std::runtime_error(fmt::format("Could not set max requests per flush: {}", cass_error_desc(rc)));
    });

    apply(settings.maxConcurrentCreation, [this](auto value) {
        if (auto const rc = cass_cluster_set_max_concurrent_creation(*this, value); rc != CASS_OK)
            throw std::runtime_error(fmt::format("Could not set max concurrent creation: {}", cass_error_desc(rc)));
    });

    setupConnection(settings);
    setupCertificate(settings);
    setupCredentials(settings);

    auto valueOrDefault = []<typename T>(std::optional<T> const& maybeValue) -> std::string {
        return maybeValue ? to_string(*maybeValue) : "default";
    };

    LOG(log_.info()) << "Threads: " << settings.threads;
    LOG(log_.info()) << "Max concurrent requests per host: " << settings.maxConcurrentRequestsThreshold;
    LOG(log_.info()) << "Max connections per host: " << settings.maxConnectionsPerHost;
    LOG(log_.info()) << "Core connections per host: " << settings.coreConnectionsPerHost;
    LOG(log_.info()) << "IO queue size: " << queueSize;
    LOG(log_.info()) << "Event queue size: " << valueOrDefault(settings.queueSizeEvent);
    LOG(log_.info()) << "Write bytes high watermark: " << valueOrDefault(settings.writeBytesHighWatermark);
    LOG(log_.info()) << "Write bytes low watermark: " << valueOrDefault(settings.writeBytesLowWatermark);
    LOG(log_.info()) << "Pending requests high watermark: " << valueOrDefault(settings.pendingRequestsHighWatermark);
    LOG(log_.info()) << "Pending requests low watermark: " << valueOrDefault(settings.pendingRequestsLowWatermark);
    LOG(log_.info()) << "Max requests per flush: " << valueOrDefault(settings.maxRequestsPerFlush);
    LOG(log_.info()) << "Max concurrent creation: " << valueOrDefault(settings.maxConcurrentCreation);
}

void
Cluster::setupConnection(Settings const& settings)
{
    std::visit(
        overloadSet{
            [this](Settings::ContactPoints const& points) { setupContactPoints(points); },
            [this](Settings::SecureConnectionBundle const& bundle) { setupSecureBundle(bundle); }},
        settings.connectionInfo);
}

void
Cluster::setupContactPoints(Settings::ContactPoints const& points)
{
    using std::to_string;
    auto throwErrorIfNeeded = [](CassError rc, std::string const& label, std::string const& value) {
        if (rc != CASS_OK)
            throw std::runtime_error(
                fmt::format("Cassandra: Error setting {} [{}]: {}", label, value, cass_error_desc(rc)));
    };

    {
        LOG(log_.debug()) << "Attempt connection using contact points: " << points.contactPoints;
        auto const rc = cass_cluster_set_contact_points(*this, points.contactPoints.data());
        throwErrorIfNeeded(rc, "contact_points", points.contactPoints);
    }

    if (points.port)
    {
        auto const rc = cass_cluster_set_port(*this, points.port.value());
        throwErrorIfNeeded(rc, "port", to_string(points.port.value()));
    }
}

void
Cluster::setupSecureBundle(Settings::SecureConnectionBundle const& bundle)
{
    LOG(log_.debug()) << "Attempt connection using secure bundle";
    if (auto const rc = cass_cluster_set_cloud_secure_connection_bundle(*this, bundle.bundle.data()); rc != CASS_OK)
    {
        throw std::runtime_error("Failed to connect using secure connection bundle " + bundle.bundle);
    }
}

void
Cluster::setupCertificate(Settings const& settings)
{
    if (not settings.certificate)
        return;

    LOG(log_.debug()) << "Configure SSL context";
    SslContext context = SslContext(*settings.certificate);
    cass_cluster_set_ssl(*this, context);
}

void
Cluster::setupCredentials(Settings const& settings)
{
    if (not settings.username || not settings.password)
        return;

    LOG(log_.debug()) << "Set credentials; username: " << settings.username.value();
    cass_cluster_set_credentials(*this, settings.username.value().c_str(), settings.password.value().c_str());
}

}  // namespace data::cassandra::detail
