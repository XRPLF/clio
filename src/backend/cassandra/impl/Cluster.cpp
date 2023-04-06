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

#include <backend/cassandra/impl/Cluster.h>
#include <backend/cassandra/impl/SslContext.h>
#include <backend/cassandra/impl/Statement.h>
#include <util/Expected.h>

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

namespace Backend::Cassandra::detail {

Cluster::Cluster(Settings const& settings) : ManagedObject{cass_cluster_new(), clusterDeleter}
{
    using std::to_string;

    cass_cluster_set_token_aware_routing(*this, cass_true);
    if (auto const rc = cass_cluster_set_protocol_version(*this, CASS_PROTOCOL_VERSION_V4); rc != CASS_OK)
    {
        throw std::runtime_error(std::string{"Error setting cassandra protocol version to v4: "} + cass_error_desc(rc));
    }

    if (auto const rc = cass_cluster_set_num_threads_io(*this, settings.threads); rc != CASS_OK)
    {
        throw std::runtime_error(
            std::string{"Error setting cassandra io threads to "} + to_string(settings.threads) + ": " +
            cass_error_desc(rc));
    }

    cass_log_set_level(settings.enableLog ? CASS_LOG_TRACE : CASS_LOG_DISABLED);
    cass_cluster_set_connect_timeout(*this, settings.connectionTimeout.count());
    cass_cluster_set_request_timeout(*this, settings.requestTimeout.count());

    // TODO: other options to experiment with and consider later:
    // cass_cluster_set_max_concurrent_requests_threshold(*this, 10000);
    // cass_cluster_set_queue_size_event(*this, 100000);
    // cass_cluster_set_queue_size_io(*this, 100000);
    // cass_cluster_set_write_bytes_high_water_mark(*this, 16 * 1024 * 1024);  // 16mb
    // cass_cluster_set_write_bytes_low_water_mark(*this, 8 * 1024 * 1024);  // half of allowance
    // cass_cluster_set_pending_requests_high_water_mark(*this, 5000);
    // cass_cluster_set_pending_requests_low_water_mark(*this, 2500);  // half
    // cass_cluster_set_max_requests_per_flush(*this, 1000);
    // cass_cluster_set_max_concurrent_creation(*this, 8);
    // cass_cluster_set_max_connections_per_host(*this, 6);
    // cass_cluster_set_core_connections_per_host(*this, 4);
    // cass_cluster_set_constant_speculative_execution_policy(*this, 1000, 1024);

    if (auto const rc = cass_cluster_set_queue_size_io(
            *this, settings.maxWriteRequestsOutstanding + settings.maxReadRequestsOutstanding);
        rc != CASS_OK)
    {
        throw std::runtime_error(std::string{"Could not set queue size for IO per host: "} + cass_error_desc(rc));
    }

    setupConnection(settings);
    setupCertificate(settings);
    setupCredentials(settings);
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
    auto throwErrorIfNeeded = [](CassError rc, std::string const label, std::string const value) {
        if (rc != CASS_OK)
            throw std::runtime_error("Cassandra: Error setting " + label + " [" + value + "]: " + cass_error_desc(rc));
    };

    {
        log_.debug() << "Attempt connection using contact points: " << points.contactPoints;
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
    log_.debug() << "Attempt connection using secure bundle";
    if (auto const rc = cass_cluster_set_cloud_secure_connection_bundle(*this, bundle.bundle.data()); rc != CASS_OK)
    {
        throw std::runtime_error("Failed to connect using secure connection bundle" + bundle.bundle);
    }
}

void
Cluster::setupCertificate(Settings const& settings)
{
    if (not settings.certificate)
        return;

    log_.debug() << "Configure SSL context";
    SslContext context = SslContext(*settings.certificate);
    cass_cluster_set_ssl(*this, context);
}

void
Cluster::setupCredentials(Settings const& settings)
{
    if (not settings.username || not settings.password)
        return;

    log_.debug() << "Set credentials; username: " << settings.username.value();
    cass_cluster_set_credentials(*this, settings.username.value().c_str(), settings.password.value().c_str());
}

}  // namespace Backend::Cassandra::detail
