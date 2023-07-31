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

#include <backend/DBHelpers.h>
#include <etl/ETLService.h>
#include <etl/LoadBalancer.h>
#include <etl/NFTHelpers.h>
#include <etl/ProbingSource.h>
#include <etl/Source.h>
#include <log/Logger.h>
#include <rpc/RPCHelpers.h>
#include <util/Profiler.h>

#include <ripple/beast/net/IPEndpoint.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <boost/asio/strand.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>

#include <thread>

using namespace clio;

static boost::beast::websocket::stream_base::timeout
make_TimeoutOption()
{
    // See #289 for details.
    // TODO: investigate the issue and find if there is a solution other than
    // introducing artificial timeouts.
    if (true)
    {
        // The only difference between this and the suggested client role is
        // that idle_timeout is set to 20 instead of none()
        auto opt = boost::beast::websocket::stream_base::timeout{};
        opt.handshake_timeout = std::chrono::seconds(30);
        opt.idle_timeout = std::chrono::seconds(20);
        opt.keep_alive_pings = false;
        return opt;
    }
    else
    {
        return boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client);
    }
}

template <class Derived>
void
SourceImpl<Derived>::reconnect(boost::beast::error_code ec)
{
    if (paused_)
        return;

    if (connected_)
        hooks_.onDisconnected(ec);

    connected_ = false;
    readBuffer_ = {};

    // These are somewhat normal errors. operation_aborted occurs on shutdown,
    // when the timer is cancelled. connection_refused will occur repeatedly
    std::string err = ec.message();
    // if we cannot connect to the transaction processing process
    if (ec.category() == boost::asio::error::get_ssl_category())
    {
        err = std::string(" (") + boost::lexical_cast<std::string>(ERR_GET_LIB(ec.value())) + "," +
            boost::lexical_cast<std::string>(ERR_GET_REASON(ec.value())) + ") ";

        // ERR_PACK /* crypto/err/err.h */
        char buf[128];
        ::ERR_error_string_n(ec.value(), buf, sizeof(buf));
        err += buf;

        log_.error() << err;
    }

    if (ec != boost::asio::error::operation_aborted && ec != boost::asio::error::connection_refused)
    {
        log_.error() << "error code = " << ec << " - " << toString();
    }
    else
    {
        log_.warn() << "error code = " << ec << " - " << toString();
    }

    // exponentially increasing timeouts, with a max of 30 seconds
    size_t waitTime = std::min(pow(2, numFailures_), 30.0);
    numFailures_++;
    timer_.expires_after(boost::asio::chrono::seconds(waitTime));
    timer_.async_wait([this](auto ec) {
        bool startAgain = (ec != boost::asio::error::operation_aborted);
        derived().close(startAgain);
    });
}

void
PlainSource::close(bool startAgain)
{
    timer_.cancel();
    boost::asio::post(strand_, [this, startAgain]() {
        if (closing_)
            return;

        if (derived().ws().is_open())
        {
            // onStop() also calls close(). If the async_close is called twice,
            // an assertion fails. Using closing_ makes sure async_close is only
            // called once
            closing_ = true;
            derived().ws().async_close(boost::beast::websocket::close_code::normal, [this, startAgain](auto ec) {
                if (ec)
                {
                    log_.error() << " async_close : "
                                 << "error code = " << ec << " - " << toString();
                }
                closing_ = false;
                if (startAgain)
                {
                    ws_ = std::make_unique<StreamType>(strand_);
                    run();
                }
            });
        }
        else if (startAgain)
        {
            ws_ = std::make_unique<StreamType>(strand_);
            run();
        }
    });
}

void
SslSource::close(bool startAgain)
{
    timer_.cancel();
    boost::asio::post(strand_, [this, startAgain]() {
        if (closing_)
            return;

        if (derived().ws().is_open())
        {
            // onStop() also calls close(). If the async_close is called twice, an assertion fails. Using closing_ makes
            // sure async_close is only called once
            closing_ = true;
            derived().ws().async_close(boost::beast::websocket::close_code::normal, [this, startAgain](auto ec) {
                if (ec)
                {
                    log_.error() << " async_close : "
                                 << "error code = " << ec << " - " << toString();
                }
                closing_ = false;
                if (startAgain)
                {
                    ws_ = std::make_unique<StreamType>(strand_, *sslCtx_);
                    run();
                }
            });
        }
        else if (startAgain)
        {
            ws_ = std::make_unique<StreamType>(strand_, *sslCtx_);
            run();
        }
    });
}

template <class Derived>
void
SourceImpl<Derived>::onResolve(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results)
{
    if (ec)
    {
        // try again
        reconnect(ec);
    }
    else
    {
        boost::beast::get_lowest_layer(derived().ws()).expires_after(std::chrono::seconds(30));
        boost::beast::get_lowest_layer(derived().ws()).async_connect(results, [this](auto ec, auto ep) {
            derived().onConnect(ec, ep);
        });
    }
}

void
PlainSource::onConnect(
    boost::beast::error_code ec,
    boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
{
    if (ec)
    {
        // start over
        reconnect(ec);
    }
    else
    {
        numFailures_ = 0;

        // Websocket stream has it's own timeout system
        boost::beast::get_lowest_layer(derived().ws()).expires_never();

        derived().ws().set_option(make_TimeoutOption());
        derived().ws().set_option(
            boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type& req) {
                req.set(boost::beast::http::field::user_agent, "clio-client");
                req.set("X-User", "clio-client");
            }));

        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        auto host = ip_ + ':' + std::to_string(endpoint.port());
        derived().ws().async_handshake(host, "/", [this](auto ec) { onHandshake(ec); });
    }
}

void
SslSource::onConnect(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
{
    if (ec)
    {
        // start over
        reconnect(ec);
    }
    else
    {
        numFailures_ = 0;

        // Websocket stream has it's own timeout system
        boost::beast::get_lowest_layer(derived().ws()).expires_never();

        derived().ws().set_option(make_TimeoutOption());
        derived().ws().set_option(
            boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type& req) {
                req.set(boost::beast::http::field::user_agent, "clio-client");
                req.set("X-User", "clio-client");
            }));

        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        auto host = ip_ + ':' + std::to_string(endpoint.port());
        ws().next_layer().async_handshake(
            boost::asio::ssl::stream_base::client, [this, endpoint](auto ec) { onSslHandshake(ec, endpoint); });
    }
}

void
SslSource::onSslHandshake(
    boost::beast::error_code ec,
    boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
{
    if (ec)
    {
        reconnect(ec);
    }
    else
    {
        auto host = ip_ + ':' + std::to_string(endpoint.port());
        ws().async_handshake(host, "/", [this](auto ec) { onHandshake(ec); });
    }
}

template <class Derived>
void
SourceImpl<Derived>::onHandshake(boost::beast::error_code ec)
{
    if (auto action = hooks_.onConnected(ec); action == SourceHooks::Action::STOP)
        return;

    if (ec)
    {
        // start over
        reconnect(ec);
    }
    else
    {
        boost::json::object jv{
            {"command", "subscribe"},
            {"streams", {"ledger", "manifests", "validations", "transactions_proposed"}},
        };
        std::string s = boost::json::serialize(jv);
        log_.trace() << "Sending subscribe stream message";

        derived().ws().set_option(
            boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type& req) {
                req.set(
                    boost::beast::http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " clio-client");
                req.set("X-User", "coro-client");
            }));

        // Send subscription message
        derived().ws().async_write(boost::asio::buffer(s), [this](auto ec, size_t size) { onWrite(ec, size); });
    }
}

template <class Derived>
void
SourceImpl<Derived>::onWrite(boost::beast::error_code ec, size_t bytesWritten)
{
    if (ec)
        reconnect(ec);
    else
        derived().ws().async_read(readBuffer_, [this](auto ec, size_t size) { onRead(ec, size); });
}

template <class Derived>
void
SourceImpl<Derived>::onRead(boost::beast::error_code ec, size_t size)
{
    if (ec)
    {
        reconnect(ec);
    }
    else
    {
        handleMessage(size);
        derived().ws().async_read(readBuffer_, [this](auto ec, size_t size) { onRead(ec, size); });
    }
}

template <class Derived>
bool
SourceImpl<Derived>::handleMessage(size_t size)
{
    setLastMsgTime();
    connected_ = true;

    try
    {
        auto const msg = boost::beast::buffers_to_string(readBuffer_.data());
        readBuffer_.consume(size);

        auto const raw = boost::json::parse(msg);
        auto const response = raw.as_object();
        uint32_t ledgerIndex = 0;

        if (response.contains("result"))
        {
            auto const& result = response.at("result").as_object();
            if (result.contains("ledger_index"))
                ledgerIndex = result.at("ledger_index").as_int64();

            if (result.contains("validated_ledgers"))
            {
                auto const& validatedLedgers = result.at("validated_ledgers").as_string();
                setValidatedRange({validatedLedgers.data(), validatedLedgers.size()});
            }

            log_.info() << "Received a message on ledger "
                        << " subscription stream. Message : " << response << " - " << toString();
        }
        else if (response.contains("type") && response.at("type") == "ledgerClosed")
        {
            log_.info() << "Received a message on ledger "
                        << " subscription stream. Message : " << response << " - " << toString();
            if (response.contains("ledger_index"))
            {
                ledgerIndex = response.at("ledger_index").as_int64();
            }
            if (response.contains("validated_ledgers"))
            {
                auto const& validatedLedgers = response.at("validated_ledgers").as_string();
                setValidatedRange({validatedLedgers.data(), validatedLedgers.size()});
            }
        }
        else
        {
            if (balancer_.shouldPropagateTxnStream(this))
            {
                if (response.contains("transaction"))
                {
                    forwardCache_.freshen();
                    subscriptions_->forwardProposedTransaction(response);
                }
                else if (response.contains("type") && response.at("type") == "validationReceived")
                {
                    subscriptions_->forwardValidation(response);
                }
                else if (response.contains("type") && response.at("type") == "manifestReceived")
                {
                    subscriptions_->forwardManifest(response);
                }
            }
        }

        if (ledgerIndex != 0)
        {
            log_.trace() << "Pushing ledger sequence = " << ledgerIndex << " - " << toString();
            networkValidatedLedgers_->push(ledgerIndex);
        }

        return true;
    }
    catch (std::exception const& e)
    {
        log_.error() << "Exception in handleMessage : " << e.what();
        return false;
    }
}

// TODO: move to detail
class AsyncCallData
{
    clio::Logger log_{"ETL"};

    std::unique_ptr<org::xrpl::rpc::v1::GetLedgerDataResponse> cur_;
    std::unique_ptr<org::xrpl::rpc::v1::GetLedgerDataResponse> next_;

    org::xrpl::rpc::v1::GetLedgerDataRequest request_;
    std::unique_ptr<grpc::ClientContext> context_;

    grpc::Status status_;
    unsigned char nextPrefix_;

    std::string lastKey_;

public:
    AsyncCallData(uint32_t seq, ripple::uint256 const& marker, std::optional<ripple::uint256> const& nextMarker)
    {
        request_.mutable_ledger()->set_sequence(seq);
        if (marker.isNonZero())
        {
            request_.set_marker(marker.data(), marker.size());
        }
        request_.set_user("ETL");
        nextPrefix_ = 0x00;
        if (nextMarker)
            nextPrefix_ = nextMarker->data()[0];

        unsigned char prefix = marker.data()[0];

        log_.debug() << "Setting up AsyncCallData. marker = " << ripple::strHex(marker)
                     << " . prefix = " << ripple::strHex(std::string(1, prefix))
                     << " . nextPrefix_ = " << ripple::strHex(std::string(1, nextPrefix_));

        assert(nextPrefix_ > prefix || nextPrefix_ == 0x00);

        cur_ = std::make_unique<org::xrpl::rpc::v1::GetLedgerDataResponse>();
        next_ = std::make_unique<org::xrpl::rpc::v1::GetLedgerDataResponse>();
        context_ = std::make_unique<grpc::ClientContext>();
    }

    enum class CallStatus { MORE, DONE, ERRORED };

    CallStatus
    process(
        std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub>& stub,
        grpc::CompletionQueue& cq,
        BackendInterface& backend,
        bool abort,
        bool cacheOnly = false)
    {
        log_.trace() << "Processing response. "
                     << "Marker prefix = " << getMarkerPrefix();
        if (abort)
        {
            log_.error() << "AsyncCallData aborted";
            return CallStatus::ERRORED;
        }
        if (!status_.ok())
        {
            log_.error() << "AsyncCallData status_ not ok: "
                         << " code = " << status_.error_code() << " message = " << status_.error_message();
            return CallStatus::ERRORED;
        }
        if (!next_->is_unlimited())
        {
            log_.warn() << "AsyncCallData is_unlimited is false. Make sure "
                           "secure_gateway is set correctly at the ETL source";
        }

        std::swap(cur_, next_);

        bool more = true;

        // if no marker returned, we are done
        if (cur_->marker().size() == 0)
            more = false;

        // if returned marker is greater than our end, we are done
        unsigned char prefix = cur_->marker()[0];
        if (nextPrefix_ != 0x00 && prefix >= nextPrefix_)
            more = false;

        // if we are not done, make the next async call
        if (more)
        {
            request_.set_marker(std::move(cur_->marker()));
            call(stub, cq);
        }

        auto const numObjects = cur_->ledger_objects().objects_size();
        log_.debug() << "Writing " << numObjects << " objects";

        std::vector<Backend::LedgerObject> cacheUpdates;
        cacheUpdates.reserve(numObjects);

        for (int i = 0; i < numObjects; ++i)
        {
            auto& obj = *(cur_->mutable_ledger_objects()->mutable_objects(i));
            if (!more && nextPrefix_ != 0x00)
            {
                if (((unsigned char)obj.key()[0]) >= nextPrefix_)
                    continue;
            }
            cacheUpdates.push_back(
                {*ripple::uint256::fromVoidChecked(obj.key()),
                 {obj.mutable_data()->begin(), obj.mutable_data()->end()}});
            if (!cacheOnly)
            {
                if (lastKey_.size())
                    backend.writeSuccessor(std::move(lastKey_), request_.ledger().sequence(), std::string{obj.key()});
                lastKey_ = obj.key();
                backend.writeNFTs(getNFTDataFromObj(request_.ledger().sequence(), obj.key(), obj.data()));
                backend.writeLedgerObject(
                    std::move(*obj.mutable_key()), request_.ledger().sequence(), std::move(*obj.mutable_data()));
            }
        }
        backend.cache().update(cacheUpdates, request_.ledger().sequence(), cacheOnly);
        log_.debug() << "Wrote " << numObjects << " objects. Got more: " << (more ? "YES" : "NO");

        return more ? CallStatus::MORE : CallStatus::DONE;
    }

    void
    call(std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub>& stub, grpc::CompletionQueue& cq)
    {
        context_ = std::make_unique<grpc::ClientContext>();

        std::unique_ptr<grpc::ClientAsyncResponseReader<org::xrpl::rpc::v1::GetLedgerDataResponse>> rpc(
            stub->PrepareAsyncGetLedgerData(context_.get(), request_, &cq));

        rpc->StartCall();

        rpc->Finish(next_.get(), &status_, this);
    }

    std::string
    getMarkerPrefix()
    {
        if (next_->marker().size() == 0)
            return "";
        else
            return ripple::strHex(std::string{next_->marker().data()[0]});
    }

    std::string
    getLastKey()
    {
        return lastKey_;
    }
};

template <class Derived>
std::pair<std::vector<std::string>, bool>
SourceImpl<Derived>::loadInitialLedger(uint32_t sequence, uint32_t numMarkers, bool cacheOnly)
{
    if (!stub_)
        return {{}, false};

    grpc::CompletionQueue cq;
    void* tag;
    bool ok = false;
    std::vector<AsyncCallData> calls;
    auto markers = getMarkers(numMarkers);

    for (size_t i = 0; i < markers.size(); ++i)
    {
        std::optional<ripple::uint256> nextMarker;

        if (i + 1 < markers.size())
            nextMarker = markers[i + 1];

        calls.emplace_back(sequence, markers[i], nextMarker);
    }

    log_.debug() << "Starting data download for ledger " << sequence << ". Using source = " << toString();

    for (auto& c : calls)
        c.call(stub_, cq);

    size_t numFinished = 0;
    bool abort = false;
    size_t incr = 500000;
    size_t progress = incr;
    std::vector<std::string> edgeKeys;

    while (numFinished < calls.size() && cq.Next(&tag, &ok))
    {
        assert(tag);
        auto ptr = static_cast<AsyncCallData*>(tag);

        if (!ok)
        {
            log_.error() << "loadInitialLedger - ok is false";
            return {{}, false};  // handle cancelled
        }
        else
        {
            log_.trace() << "Marker prefix = " << ptr->getMarkerPrefix();

            auto result = ptr->process(stub_, cq, *backend_, abort, cacheOnly);
            if (result != AsyncCallData::CallStatus::MORE)
            {
                ++numFinished;
                log_.debug() << "Finished a marker. "
                             << "Current number of finished = " << numFinished;

                std::string lastKey = ptr->getLastKey();

                if (lastKey.size())
                    edgeKeys.push_back(ptr->getLastKey());
            }

            if (result == AsyncCallData::CallStatus::ERRORED)
                abort = true;

            if (backend_->cache().size() > progress)
            {
                log_.info() << "Downloaded " << backend_->cache().size() << " records from rippled";
                progress += incr;
            }
        }
    }

    log_.info() << "Finished loadInitialLedger. cache size = " << backend_->cache().size();
    return {std::move(edgeKeys), !abort};
}

template <class Derived>
std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
SourceImpl<Derived>::fetchLedger(uint32_t ledgerSequence, bool getObjects, bool getObjectNeighbors)
{
    org::xrpl::rpc::v1::GetLedgerResponse response;
    if (!stub_)
        return {{grpc::StatusCode::INTERNAL, "No Stub"}, response};

    // Ledger header with txns and metadata
    org::xrpl::rpc::v1::GetLedgerRequest request;
    grpc::ClientContext context;

    request.mutable_ledger()->set_sequence(ledgerSequence);
    request.set_transactions(true);
    request.set_expand(true);
    request.set_get_objects(getObjects);
    request.set_get_object_neighbors(getObjectNeighbors);
    request.set_user("ETL");

    grpc::Status status = stub_->GetLedger(&context, request, &response);

    if (status.ok() && !response.is_unlimited())
    {
        log_.warn() << "is_unlimited is false. Make sure secure_gateway is set correctly on the ETL source. source = "
                    << toString() << "; status = " << status.error_message();
    }

    return {status, std::move(response)};
}

template <class Derived>
std::optional<boost::json::object>
SourceImpl<Derived>::forwardToRippled(
    boost::json::object const& request,
    std::string const& clientIp,
    boost::asio::yield_context& yield) const
{
    if (auto resp = forwardCache_.get(request); resp)
    {
        log_.debug() << "request hit forwardCache";
        return resp;
    }

    return requestFromRippled(request, clientIp, yield);
}

template <class Derived>
std::optional<boost::json::object>
SourceImpl<Derived>::requestFromRippled(
    boost::json::object const& request,
    std::string const& clientIp,
    boost::asio::yield_context& yield) const
{
    log_.trace() << "Attempting to forward request to tx. "
                 << "request = " << boost::json::serialize(request);

    boost::json::object response;
    if (!connected_)
    {
        log_.error() << "Attempted to proxy but failed to connect to tx";
        return {};
    }

    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace websocket = beast::websocket;
    namespace net = boost::asio;
    using tcp = boost::asio::ip::tcp;

    try
    {
        auto executor = boost::asio::get_associated_executor(yield);
        boost::beast::error_code ec;
        tcp::resolver resolver{executor};

        auto ws = std::make_unique<websocket::stream<beast::tcp_stream>>(executor);

        auto const results = resolver.async_resolve(ip_, wsPort_, yield[ec]);
        if (ec)
            return {};

        ws->next_layer().expires_after(std::chrono::seconds(3));
        ws->next_layer().async_connect(results, yield[ec]);
        if (ec)
            return {};

        // Set a decorator to change the User-Agent of the handshake and to tell rippled to charge the client IP for RPC
        // resources. See "secure_gateway" in https://github.com/ripple/rippled/blob/develop/cfg/rippled-example.cfg
        ws->set_option(websocket::stream_base::decorator([&clientIp](websocket::request_type& req) {
            req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
            req.set(http::field::forwarded, "for=" + clientIp);
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

        if (!parsed.is_object())
        {
            log_.error() << "Error parsing response: " << std::string{begin, end};
            return {};
        }

        response = parsed.as_object();
        response["forwarded"] = true;

        return response;
    }
    catch (std::exception const& e)
    {
        log_.error() << "Encountered exception : " << e.what();
        return {};
    }
}
