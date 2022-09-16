#ifndef RIPPLE_APP_REPORTING_PROBINGETLSOURCE_H_INCLUDED
#define RIPPLE_APP_REPORTING_PROBINGETLSOURCE_H_INCLUDED

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <etl/ETLSource.h>
#include <mutex>

/// This ETLSource implementation attempts to connect over both secure websocket
/// and plain websocket. First to connect pauses the other and the probing is
/// considered done at this point. If however the connected source loses
/// connection the probing is kickstarted again.
class ProbingETLSource : public ETLSource
{
    std::mutex mtx_;
    boost::asio::ssl::context sslCtx_;
    std::shared_ptr<ETLSource> sslSrc_;
    std::shared_ptr<ETLSource> plainSrc_;
    std::shared_ptr<ETLSource> currentSrc_;

public:
    ProbingETLSource(
        boost::json::object const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> nwvl,
        ETLLoadBalancer& balancer,
        boost::asio::ssl::context sslCtx = boost::asio::ssl::context{
            boost::asio::ssl::context::tlsv12});

    ~ProbingETLSource() = default;

    void
    run() override;

    void
    pause() override;

    void
    resume() override;

    bool
    isConnected() const override;

    bool
    hasLedger(uint32_t sequence) const override;

    boost::json::object
    toJson() const override;

    std::string
    toString() const override;

    bool
    loadInitialLedger(
        std::uint32_t ledgerSequence,
        std::uint32_t numMarkers,
        bool cacheOnly = false) override;

    std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedger(
        uint32_t ledgerSequence,
        bool getObjects = true,
        bool getObjectNeighbors = false) override;

    std::optional<boost::json::object>
    forwardToRippled(
        boost::json::object const& request,
        std::string const& clientIp,
        boost::asio::yield_context& yield) const override;

private:
    std::optional<boost::json::object>
    requestFromRippled(
        boost::json::object const& request,
        std::string const& clientIp,
        boost::asio::yield_context& yield) const override;

    ETLSourceHooks
    make_SSLHooks() noexcept;

    ETLSourceHooks
    make_PlainHooks() noexcept;
};

#endif
