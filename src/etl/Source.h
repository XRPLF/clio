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

#include <data/BackendInterface.h>
#include <etl/ETLHelpers.h>
#include <etl/LoadBalancer.h>
#include <etl/impl/AsyncData.h>
#include <etl/impl/ForwardCache.h>
#include <feed/SubscriptionManager.h>
#include <util/config/Config.h>
#include <util/log/Logger.h>

#include <ripple/proto/org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <grpcpp/grpcpp.h>
#include <utility>

namespace feed {
class SubscriptionManager;
}  // namespace feed

// TODO: we use Source so that we can store a vector of Sources
// but we also use CRTP for implementation of the common logic - this is a bit strange because CRTP as used here is
// supposed to be used instead of an abstract base.
// Maybe we should rework this a bit. At this point there is not too much use in the CRTP implementation - we can move
// things into the base class instead.

namespace etl {

class ProbingSource;

/**
 * @brief Base class for all ETL sources.
 *
 * Note: Since sources below are implemented via CRTP, it sort of makes no sense to have a virtual base class.
 * We should consider using a vector of ProbingSources instead of vector of unique ptrs to this virtual base.
 */
class Source {
public:
    /** @return true if source is connected; false otherwise */
    virtual bool
    isConnected() const = 0;

    /** @return JSON representation of the source */
    virtual boost::json::object
    toJson() const = 0;

    /** @brief Runs the source */
    virtual void
    run() = 0;

    /** @brief Request to pause the source (i.e. disconnect and do nothing) */
    virtual void
    pause() = 0;

    /** @brief Reconnect and resume this source */
    virtual void
    resume() = 0;

    /** @return String representation of the source (for debug) */
    virtual std::string
    toString() const = 0;

    /**
     * @brief Check if ledger is known by this source.
     *
     * @param sequence The ledger sequence to check
     * @return true if ledger is in the range of this source; false otherwise
     */
    virtual bool
    hasLedger(uint32_t sequence) const = 0;

    /**
     * @brief Fetch data for a specific ledger.
     *
     * This function will continuously try to fetch data for the specified ledger until the fetch succeeds, the ledger
     * is found in the database, or the server is shutting down.
     *
     * @param sequence Sequence of the ledger to fetch
     * @param getObjects Whether to get the account state diff between this ledger and the prior one; defaults to true
     * @param getObjectNeighbors Whether to request object neighbors; defaults to false
     * @return A std::pair of the response status and the response itself
     */
    virtual std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedger(uint32_t sequence, bool getObjects = true, bool getObjectNeighbors = false) = 0;

    /**
     * @brief Download a ledger in full.
     *
     * @param sequence Sequence of the ledger to download
     * @param numMarkers Number of markers to generate for async calls
     * @param cacheOnly Only insert into cache, not the DB; defaults to false
     * @return A std::pair of the data and a bool indicating whether the download was successfull
     */
    virtual std::pair<std::vector<std::string>, bool>
    loadInitialLedger(uint32_t sequence, std::uint32_t numMarkers, bool cacheOnly = false) = 0;

    /**
     * @brief Forward a request to rippled.
     *
     * @param request The request to forward
     * @param clientIp IP of the client forwarding this request if known
     * @param yield The coroutine context
     * @return Response wrapped in an optional on success; nullopt otherwise
     */
    virtual std::optional<boost::json::object>
    forwardToRippled(
        boost::json::object const& request,
        std::optional<std::string> const& forwardToRippledclientIp,
        boost::asio::yield_context yield
    ) const = 0;

    /**
     * @return A token that uniquely identifies this source instance.
     */
    virtual boost::uuids::uuid
    token() const = 0;

    virtual ~Source() = default;

    /**
     * @brief Comparison is done via comparing tokens provided by the token() function.
     *
     * @param other The other source to compare to
     * @return true if sources are equal; false otherwise
     */
    bool
    operator==(Source const& other) const
    {
        return token() == other.token();
    }

protected:
    util::Logger log_{"ETL"};

private:
    friend etl::detail::ForwardCache;
    friend ProbingSource;

    virtual std::optional<boost::json::object>
    requestFromRippled(
        boost::json::object const& request,
        std::optional<std::string> const& clientIp,
        boost::asio::yield_context yield
    ) const = 0;
};

/**
 * @brief Hooks for source events such as connects and disconnects.
 */
struct SourceHooks {
    enum class Action { STOP, PROCEED };

    std::function<Action(boost::beast::error_code)> onConnected;
    std::function<Action(boost::beast::error_code)> onDisconnected;
};

/**
 * @brief Base implementation of shared source logic.
 *
 * @tparam Derived The derived class for CRTP
 */
template <class Derived>
class SourceImpl : public Source {
    std::string wsPort_;
    std::string grpcPort_;

    std::vector<std::pair<uint32_t, uint32_t>> validatedLedgers_;
    std::string validatedLedgersRaw_{"N/A"};
    std::shared_ptr<NetworkValidatedLedgers> networkValidatedLedgers_;

    mutable std::mutex mtx_;

    // true if this ETL source is forwarding transactions received on the transactions_proposed stream. There are
    // usually multiple ETL sources, so to avoid forwarding the same transaction multiple times, we only forward from
    // one particular ETL source at a time.
    std::atomic_bool forwardingStream_{false};

    std::chrono::system_clock::time_point lastMsgTime_;
    mutable std::mutex lastMsgTimeMtx_;

    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<feed::SubscriptionManager> subscriptions_;
    LoadBalancer& balancer_;

    etl::detail::ForwardCache forwardCache_;
    boost::uuids::uuid uuid_{};

protected:
    std::string ip_;
    size_t numFailures_ = 0;

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    boost::asio::steady_timer timer_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::beast::flat_buffer readBuffer_;

    std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub> stub_;

    std::atomic_bool closing_{false};
    std::atomic_bool paused_{false};
    std::atomic_bool connected_{false};

    SourceHooks hooks_;

public:
    /**
     * @brief Create the base portion of ETL source.
     *
     * @param config The configuration to use
     * @param ioc The io_context to run on
     * @param backend BackendInterface implementation
     * @param subscriptions Subscription manager
     * @param validatedLedgers The network validated ledgers datastructure
     * @param balancer Load balancer to use
     * @param hooks Hooks to use for connect/disconnect events
     */
    SourceImpl(
        util::Config const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<feed::SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> validatedLedgers,
        LoadBalancer& balancer,
        SourceHooks hooks
    )
        : networkValidatedLedgers_(std::move(validatedLedgers))
        , backend_(std::move(backend))
        , subscriptions_(std::move(subscriptions))
        , balancer_(balancer)
        , forwardCache_(config, ioc, *this)
        , strand_(boost::asio::make_strand(ioc))
        , timer_(strand_)
        , resolver_(strand_)
        , hooks_(std::move(hooks))
    {
        static boost::uuids::random_generator uuidGenerator;
        uuid_ = uuidGenerator();

        ip_ = config.valueOr<std::string>("ip", {});
        wsPort_ = config.valueOr<std::string>("ws_port", {});

        if (auto value = config.maybeValue<std::string>("grpc_port"); value) {
            grpcPort_ = *value;
            try {
                boost::asio::ip::tcp::endpoint const endpoint{boost::asio::ip::make_address(ip_), std::stoi(grpcPort_)};
                std::stringstream ss;
                ss << endpoint;
                grpc::ChannelArguments chArgs;
                chArgs.SetMaxReceiveMessageSize(-1);
                stub_ = org::xrpl::rpc::v1::XRPLedgerAPIService::NewStub(
                    grpc::CreateCustomChannel(ss.str(), grpc::InsecureChannelCredentials(), chArgs)
                );
                LOG(log_.debug()) << "Made stub for remote = " << toString();
            } catch (std::exception const& e) {
                LOG(log_.debug()) << "Exception while creating stub = " << e.what() << " . Remote = " << toString();
            }
        }
    }

    ~SourceImpl() override
    {
        derived().close(false);
    }

    bool
    isConnected() const override
    {
        return connected_;
    }

    boost::uuids::uuid
    token() const override
    {
        return uuid_;
    }

    std::optional<boost::json::object>
    requestFromRippled(
        boost::json::object const& request,
        std::optional<std::string> const& clientIp,
        boost::asio::yield_context yield
    ) const override
    {
        LOG(log_.trace()) << "Attempting to forward request to tx. Request = " << boost::json::serialize(request);

        boost::json::object response;

        namespace beast = boost::beast;
        namespace http = boost::beast::http;
        namespace websocket = beast::websocket;
        namespace net = boost::asio;
        using tcp = boost::asio::ip::tcp;

        try {
            auto executor = boost::asio::get_associated_executor(yield);
            beast::error_code ec;
            tcp::resolver resolver{executor};

            auto ws = std::make_unique<websocket::stream<beast::tcp_stream>>(executor);

            auto const results = resolver.async_resolve(ip_, wsPort_, yield[ec]);
            if (ec)
                return {};

            ws->next_layer().expires_after(std::chrono::seconds(3));
            ws->next_layer().async_connect(results, yield[ec]);
            if (ec)
                return {};

            // if client ip is know, change the User-Agent of the handshake and to tell rippled to charge the client
            // IP for RPC resources. See "secure_gateway" in
            // https://github.com/ripple/rippled/blob/develop/cfg/rippled-example.cfg

            // TODO: user-agent can be clio-[version]
            ws->set_option(websocket::stream_base::decorator([&clientIp](websocket::request_type& req) {
                req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
                if (clientIp)
                    req.set(http::field::forwarded, "for=" + *clientIp);
            }));

            ws->async_handshake(ip_, "/", yield[ec]);
            if (ec)
                return {};

            ws->async_write(net::buffer(boost::json::serialize(request)), yield[ec]);
            if (ec)
                return {};

            beast::flat_buffer buffer;
            ws->async_read(buffer, yield[ec]);
            if (ec)
                return {};

            auto begin = static_cast<char const*>(buffer.data().data());
            auto end = begin + buffer.data().size();
            auto parsed = boost::json::parse(std::string(begin, end));

            if (!parsed.is_object()) {
                LOG(log_.error()) << "Error parsing response: " << std::string{begin, end};
                return {};
            }

            response = parsed.as_object();
            response["forwarded"] = true;

            return response;
        } catch (std::exception const& e) {
            LOG(log_.error()) << "Encountered exception : " << e.what();
            return {};
        }
    }

    bool
    hasLedger(uint32_t sequence) const override
    {
        std::lock_guard const lck(mtx_);
        for (auto& pair : validatedLedgers_) {
            if (sequence >= pair.first && sequence <= pair.second) {
                return true;
            }
            if (sequence < pair.first) {
                // validatedLedgers_ is a sorted list of disjoint ranges
                // if the sequence comes before this range, the sequence will
                // come before all subsequent ranges
                return false;
            }
        }
        return false;
    }

    std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedger(uint32_t sequence, bool getObjects = true, bool getObjectNeighbors = false) override
    {
        org::xrpl::rpc::v1::GetLedgerResponse response;
        if (!stub_)
            return {{grpc::StatusCode::INTERNAL, "No Stub"}, response};

        // Ledger header with txns and metadata
        org::xrpl::rpc::v1::GetLedgerRequest request;
        grpc::ClientContext context;

        request.mutable_ledger()->set_sequence(sequence);
        request.set_transactions(true);
        request.set_expand(true);
        request.set_get_objects(getObjects);
        request.set_get_object_neighbors(getObjectNeighbors);
        request.set_user("ETL");

        grpc::Status const status = stub_->GetLedger(&context, request, &response);

        if (status.ok() && !response.is_unlimited()) {
            log_.warn(
            ) << "is_unlimited is false. Make sure secure_gateway is set correctly on the ETL source. source = "
              << toString() << "; status = " << status.error_message();
        }

        return {status, std::move(response)};
    }

    std::string
    toString() const override
    {
        return "{validated_ledger: " + getValidatedRange() + ", ip: " + ip_ + ", web socket port: " + wsPort_ +
            ", grpc port: " + grpcPort_ + "}";
    }

    boost::json::object
    toJson() const override
    {
        boost::json::object res;

        res["validated_range"] = getValidatedRange();
        res["is_connected"] = std::to_string(isConnected());
        res["ip"] = ip_;
        res["ws_port"] = wsPort_;
        res["grpc_port"] = grpcPort_;

        auto last = getLastMsgTime();
        if (last.time_since_epoch().count() != 0) {
            res["last_msg_age_seconds"] = std::to_string(
                std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - getLastMsgTime())
                    .count()
            );
        }

        return res;
    }

    std::pair<std::vector<std::string>, bool>
    loadInitialLedger(std::uint32_t sequence, std::uint32_t numMarkers, bool cacheOnly = false) override
    {
        if (!stub_)
            return {{}, false};

        grpc::CompletionQueue cq;
        void* tag = nullptr;
        bool ok = false;
        std::vector<etl::detail::AsyncCallData> calls;
        auto markers = getMarkers(numMarkers);

        for (size_t i = 0; i < markers.size(); ++i) {
            std::optional<ripple::uint256> nextMarker;

            if (i + 1 < markers.size())
                nextMarker = markers[i + 1];

            calls.emplace_back(sequence, markers[i], nextMarker);
        }

        LOG(log_.debug()) << "Starting data download for ledger " << sequence << ". Using source = " << toString();

        for (auto& c : calls)
            c.call(stub_, cq);

        size_t numFinished = 0;
        bool abort = false;
        size_t const incr = 500000;
        size_t progress = incr;
        std::vector<std::string> edgeKeys;

        while (numFinished < calls.size() && cq.Next(&tag, &ok)) {
            assert(tag);
            auto ptr = static_cast<etl::detail::AsyncCallData*>(tag);

            if (!ok) {
                LOG(log_.error()) << "loadInitialLedger - ok is false";
                return {{}, false};  // handle cancelled
            }

            LOG(log_.trace()) << "Marker prefix = " << ptr->getMarkerPrefix();

            auto result = ptr->process(stub_, cq, *backend_, abort, cacheOnly);
            if (result != etl::detail::AsyncCallData::CallStatus::MORE) {
                ++numFinished;
                LOG(log_.debug()) << "Finished a marker. "
                                  << "Current number of finished = " << numFinished;

                std::string const lastKey = ptr->getLastKey();

                if (!lastKey.empty())
                    edgeKeys.push_back(ptr->getLastKey());
            }

            if (result == etl::detail::AsyncCallData::CallStatus::ERRORED)
                abort = true;

            if (backend_->cache().size() > progress) {
                LOG(log_.info()) << "Downloaded " << backend_->cache().size() << " records from rippled";
                progress += incr;
            }
        }

        LOG(log_.info()) << "Finished loadInitialLedger. cache size = " << backend_->cache().size();
        return {std::move(edgeKeys), !abort};
    }

    std::optional<boost::json::object>
    forwardToRippled(
        boost::json::object const& request,
        std::optional<std::string> const& clientIp,
        boost::asio::yield_context yield
    ) const override
    {
        if (auto resp = forwardCache_.get(request); resp) {
            LOG(log_.debug()) << "request hit forwardCache";
            return resp;
        }

        return requestFromRippled(request, clientIp, yield);
    }

    void
    pause() override
    {
        paused_ = true;
        derived().close(false);
    }

    void
    resume() override
    {
        paused_ = false;
        derived().close(true);
    }

    /**
     * @brief Callback for resolving the server host.
     *
     * @param ec The error code
     * @param results Result of the resolve operation
     */
    void
    onResolve(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results)
    {
        if (ec) {
            // try again
            reconnect(ec);
        } else {
            static constexpr std::size_t LOWEST_LAYER_TIMEOUT_SECONDS = 30;
            boost::beast::get_lowest_layer(derived().ws())
                .expires_after(std::chrono::seconds(LOWEST_LAYER_TIMEOUT_SECONDS));
            boost::beast::get_lowest_layer(derived().ws()).async_connect(results, [this](auto ec, auto ep) {
                derived().onConnect(ec, ep);
            });
        }
    }

    /**
     * @brief Callback for handshake with the server.
     *
     * @param ec The error code
     */
    void
    onHandshake(boost::beast::error_code ec)
    {
        if (auto action = hooks_.onConnected(ec); action == SourceHooks::Action::STOP)
            return;

        if (ec) {
            // start over
            reconnect(ec);
        } else {
            boost::json::object const jv{
                {"command", "subscribe"},
                {"streams", {"ledger", "manifests", "validations", "transactions_proposed"}},
            };
            std::string s = boost::json::serialize(jv);
            LOG(log_.trace()) << "Sending subscribe stream message";

            derived().ws().set_option(
                boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type& req) {
                    req.set(
                        boost::beast::http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " clio-client"
                    );
                    req.set("X-User", "coro-client");
                })
            );

            // Send subscription message
            derived().ws().async_write(boost::asio::buffer(s), [this](auto ec, size_t size) { onWrite(ec, size); });
        }
    }

    /**
     * @brief Callback for writing data.
     *
     * @param ec The error code
     * @param size Amount of bytes written
     */
    void
    onWrite(boost::beast::error_code ec, [[maybe_unused]] size_t size)
    {
        if (ec) {
            reconnect(ec);
        } else {
            derived().ws().async_read(readBuffer_, [this](auto ec, size_t size) { onRead(ec, size); });
        }
    }

    /**
     * @brief Callback for data available to read.
     *
     * @param ec The error code
     * @param size Amount of bytes read
     */
    void
    onRead(boost::beast::error_code ec, size_t size)
    {
        if (ec) {
            reconnect(ec);
        } else {
            handleMessage(size);
            derived().ws().async_read(readBuffer_, [this](auto ec, size_t size) { onRead(ec, size); });
        }
    }

    /**
     * @brief Handle the most recently received message.
     *
     * @param size Amount of bytes available in the read buffer
     * @return true if the message was handled successfully; false otherwise
     */
    bool
    handleMessage(size_t size)
    {
        setLastMsgTime();

        try {
            auto const msg = boost::beast::buffers_to_string(readBuffer_.data());
            readBuffer_.consume(size);

            auto const raw = boost::json::parse(msg);
            auto const response = raw.as_object();
            uint32_t ledgerIndex = 0;

            if (response.contains("result")) {
                auto const& result = response.at("result").as_object();
                if (result.contains("ledger_index"))
                    ledgerIndex = result.at("ledger_index").as_int64();

                if (result.contains("validated_ledgers")) {
                    auto const& validatedLedgers = result.at("validated_ledgers").as_string();
                    setValidatedRange({validatedLedgers.data(), validatedLedgers.size()});
                }

                LOG(log_.info()) << "Received a message on ledger "
                                 << " subscription stream. Message : " << response << " - " << toString();
            } else if (response.contains("type") && response.at("type") == "ledgerClosed") {
                LOG(log_.info()) << "Received a message on ledger "
                                 << " subscription stream. Message : " << response << " - " << toString();
                if (response.contains("ledger_index")) {
                    ledgerIndex = response.at("ledger_index").as_int64();
                }
                if (response.contains("validated_ledgers")) {
                    auto const& validatedLedgers = response.at("validated_ledgers").as_string();
                    setValidatedRange({validatedLedgers.data(), validatedLedgers.size()});
                }
            } else {
                if (balancer_.shouldPropagateTxnStream(this)) {
                    if (response.contains("transaction")) {
                        forwardCache_.freshen();
                        subscriptions_->forwardProposedTransaction(response);
                    } else if (response.contains("type") && response.at("type") == "validationReceived") {
                        subscriptions_->forwardValidation(response);
                    } else if (response.contains("type") && response.at("type") == "manifestReceived") {
                        subscriptions_->forwardManifest(response);
                    }
                }
            }

            if (ledgerIndex != 0) {
                LOG(log_.trace()) << "Pushing ledger sequence = " << ledgerIndex << " - " << toString();
                networkValidatedLedgers_->push(ledgerIndex);
            }

            return true;
        } catch (std::exception const& e) {
            LOG(log_.error()) << "Exception in handleMessage : " << e.what();
            return false;
        }
    }

protected:
    Derived&
    derived()
    {
        return static_cast<Derived&>(*this);
    }

    void
    run() override
    {
        resolver_.async_resolve(ip_, wsPort_, [this](auto ec, auto results) { onResolve(ec, results); });
    }

    void
    reconnect(boost::beast::error_code ec)
    {
        static constexpr std::size_t BUFFER_SIZE = 128;
        if (paused_)
            return;

        if (isConnected())
            hooks_.onDisconnected(ec);

        connected_ = false;
        readBuffer_ = {};

        // These are somewhat normal errors. operation_aborted occurs on shutdown,
        // when the timer is cancelled. connection_refused will occur repeatedly
        std::string err = ec.message();
        // if we cannot connect to the transaction processing process
        if (ec.category() == boost::asio::error::get_ssl_category()) {
            err = std::string(" (") + boost::lexical_cast<std::string>(ERR_GET_LIB(ec.value())) + "," +
                boost::lexical_cast<std::string>(ERR_GET_REASON(ec.value())) + ") ";

            // ERR_PACK /* crypto/err/err.h */
            char buf[BUFFER_SIZE];
            ::ERR_error_string_n(ec.value(), buf, sizeof(buf));
            err += buf;

            LOG(log_.error()) << err;
        }

        if (ec != boost::asio::error::operation_aborted && ec != boost::asio::error::connection_refused) {
            LOG(log_.error()) << "error code = " << ec << " - " << toString();
        } else {
            LOG(log_.warn()) << "error code = " << ec << " - " << toString();
        }

        // exponentially increasing timeouts, with a max of 30 seconds
        size_t const waitTime = std::min(pow(2, numFailures_), 30.0);
        numFailures_++;
        timer_.expires_after(boost::asio::chrono::seconds(waitTime));
        timer_.async_wait([this](auto ec) {
            bool const startAgain = (ec != boost::asio::error::operation_aborted);
            derived().close(startAgain);
        });
    }

private:
    void
    setLastMsgTime()
    {
        std::lock_guard const lck(lastMsgTimeMtx_);
        lastMsgTime_ = std::chrono::system_clock::now();
    }

    std::chrono::system_clock::time_point
    getLastMsgTime() const
    {
        std::lock_guard const lck(lastMsgTimeMtx_);
        return lastMsgTime_;
    }

    void
    setValidatedRange(std::string const& range)
    {
        std::vector<std::pair<uint32_t, uint32_t>> pairs;
        std::vector<std::string> ranges;
        boost::split(ranges, range, boost::is_any_of(","));
        for (auto& pair : ranges) {
            std::vector<std::string> minAndMax;

            boost::split(minAndMax, pair, boost::is_any_of("-"));

            if (minAndMax.size() == 1) {
                uint32_t const sequence = std::stoll(minAndMax[0]);
                pairs.emplace_back(sequence, sequence);
            } else {
                ASSERT(minAndMax.size() == 2, "Min and max should be of size 2. Got size ={}", minAndMax.size());
                uint32_t const min = std::stoll(minAndMax[0]);
                uint32_t const max = std::stoll(minAndMax[1]);
                pairs.emplace_back(min, max);
            }
        }
        std::sort(pairs.begin(), pairs.end(), [](auto left, auto right) { return left.first < right.first; });

        // we only hold the lock here, to avoid blocking while string processing
        std::lock_guard const lck(mtx_);
        validatedLedgers_ = std::move(pairs);
        validatedLedgersRaw_ = range;
    }

    std::string
    getValidatedRange() const
    {
        std::lock_guard const lck(mtx_);
        return validatedLedgersRaw_;
    }
};

/**
 * @brief Implementation of a source that uses a regular, non-secure websocket connection.
 */
class PlainSource : public SourceImpl<PlainSource> {
    using StreamType = boost::beast::websocket::stream<boost::beast::tcp_stream>;
    std::unique_ptr<StreamType> ws_;

public:
    /**
     * @brief Create a non-secure ETL source.
     *
     * @param config The configuration to use
     * @param ioc The io_context to run on
     * @param backend BackendInterface implementation
     * @param subscriptions Subscription manager
     * @param validatedLedgers The network validated ledgers datastructure
     * @param balancer Load balancer to use
     * @param hooks Hooks to use for connect/disconnect events
     */
    PlainSource(
        util::Config const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<feed::SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> validatedLedgers,
        LoadBalancer& balancer,
        SourceHooks hooks
    )
        : SourceImpl(config, ioc, backend, subscriptions, validatedLedgers, balancer, std::move(hooks))
        , ws_(std::make_unique<StreamType>(strand_))
    {
    }

    /**
     * @brief Callback for connection to the server.
     *
     * @param ec The error code
     * @param endpoint The resolved endpoint
     */
    void
    onConnect(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint);

    /**
     * @brief Close the websocket.
     *
     * @param startAgain Whether to automatically reconnect
     */
    void
    close(bool startAgain);

    /** @return The underlying TCP stream */
    StreamType&
    ws()
    {
        return *ws_;
    }
};

/**
 * @brief Implementation of a source that uses a secure websocket connection.
 */
class SslSource : public SourceImpl<SslSource> {
    using StreamType = boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>;
    std::optional<std::reference_wrapper<boost::asio::ssl::context>> sslCtx_;
    std::unique_ptr<StreamType> ws_;

public:
    /**
     * @brief Create a secure ETL source.
     *
     * @param config The configuration to use
     * @param ioc The io_context to run on
     * @param sslCtx The SSL context if any
     * @param backend BackendInterface implementation
     * @param subscriptions Subscription manager
     * @param validatedLedgers The network validated ledgers datastructure
     * @param balancer Load balancer to use
     * @param hooks Hooks to use for connect/disconnect events
     */
    SslSource(
        util::Config const& config,
        boost::asio::io_context& ioc,
        std::optional<std::reference_wrapper<boost::asio::ssl::context>> sslCtx,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<feed::SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> validatedLedgers,
        LoadBalancer& balancer,
        SourceHooks hooks
    )
        : SourceImpl(config, ioc, backend, subscriptions, validatedLedgers, balancer, std::move(hooks))
        , sslCtx_(sslCtx)
        , ws_(std::make_unique<StreamType>(strand_, *sslCtx_))
    {
    }

    /**
     * @brief Callback for connection to the server.
     *
     * @param ec The error code
     * @param endpoint The resolved endpoint
     */
    void
    onConnect(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint);

    /**
     * @brief Callback for SSL handshake completion.
     *
     * @param ec The error code
     * @param endpoint The resolved endpoint
     */
    void
    onSslHandshake(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint);

    /**
     * @brief Close the websocket.
     *
     * @param startAgain Whether to automatically reconnect
     */
    void
    close(bool startAgain);

    /** @return The underlying SSL stream */
    StreamType&
    ws()
    {
        return *ws_;
    }
};
}  // namespace etl
