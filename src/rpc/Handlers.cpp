#include <rpc/Handlers.h>
#include <rpc/handlers/Account.h>
#include <rpc/handlers/Channel.h>
#include <rpc/handlers/Exchange.h>
#include <rpc/handlers/Ledger.h>
#include <rpc/handlers/Subscribe.h>
#include <rpc/handlers/Transaction.h>
#include <server/WsSession.h>
#include <server/SslWsSession.h>
#include <reporting/P2pProxy.h>

void
WsSession::do_close()
{
    // perform a close operation
    std::shared_ptr<SubscriptionManager> mgr = subscriptions_.lock();
    mgr->clearSession(this);
}

void
SslWsSession::do_close()
{
    // perform a close operation
    std::shared_ptr<SubscriptionManager> mgr = subscriptions_.lock();
    mgr->clearSession(this);
}

namespace RPC
{

class HandlerTable
{
private:
    HandlerTable()
    {
        for (auto v = RPC::ApiMinimumSupportedVersion;
             v <= RPC::ApiMaximumSupportedVersion;
             ++v)
        {
            addHandler<AccountChannels>(v);
            addHandler<AccountCurrencies>(v);
            addHandler<AccountInfo>(v);
            addHandler<AccountObjects>(v);
            addHandler<AccountOffers>(v);
            addHandler<AccountTx>(v);
            addHandler<BookOffers>(v);
            addHandler<ChannelAuthorize>(v);
            addHandler<ChannelVerify>(v);
            addHandler<Ledger>(v);
            addHandler<LedgerData>(v);
            addHandler<LedgerEntry>(v);
            addHandler<LedgerRange>(v);
            addHandler<Subscribe>(v);
            addHandler<Unsubscribe>(v);
            addHandler<Tx>(v);
        }
    }

public:
    static HandlerTable const&
    instance()
    {
        static HandlerTable const handlerTable{};
        return handlerTable;
    }

    Handler const*
    getHandler(unsigned version, std::string name) const
    {
        if (version > RPC::ApiMaximumSupportedVersion ||
            version < RPC::ApiMinimumSupportedVersion)
            return nullptr;
        auto& innerTable = table_[versionToIndex(version)];
        auto i = innerTable.find(name);
        return i == innerTable.end() ? nullptr : &i->second;
    }

private:
    std::array<std::map<std::string, Handler>, APINumberVersionSupported>
        table_;

    template <class HandlerImpl>
    void
    addHandler(unsigned version)
    {
        assert(
            version >= RPC::ApiMinimumSupportedVersion &&
            version <= RPC::ApiMaximumSupportedVersion);
        auto& innerTable = table_[versionToIndex(version)];
        assert(innerTable.find(HandlerImpl::name()) == innerTable.end());

        Handler h;
        h.name = HandlerImpl::name();
        h.role = HandlerImpl::role();

        h.method = [](Context& ctx, boost::json::object& result) -> Status
            {
                HandlerImpl handler{ctx, result};

                auto status = handler.check();
                if (status)
                    result = make_error(status.error);

                return status;
            };

        innerTable[HandlerImpl::name()] = h;
    }

    inline unsigned
    versionToIndex(unsigned version) const
    {
        return version - RPC::ApiMinimumSupportedVersion;
    }
};

Status
buildResponse(Context& ctx, boost::json::object& result)
{
    std::cout << "METHOD: " << ctx.method << std::endl;
    if (shouldForwardToP2p(ctx))
        result = ctx.balancer->forwardToP2p(ctx.params);
    std::cout << "Finding Handler" << std::endl;
    std::cout << "VERSION: " << ctx.version << std::endl;
    std::cout << "Method: " << ctx.method << std::endl;
    Handler const* handler =
        HandlerTable::instance().getHandler(ctx.version, ctx.method);

    std::cout << "got handler" << std::endl;

    if(!handler)
        std::cout << "no handler" << std::endl;

    if (!handler || !handler->method)
        return {Error::rpcUNKNOWN_COMMAND};

    std::cout << "did not return" << std::endl;
    
    auto method = handler->method;

    return method(ctx, result);
}

}