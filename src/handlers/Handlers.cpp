#include <handlers/Handlers.h>
#include <handlers/methods/Account.h>
#include <handlers/methods/Channel.h>
#include <handlers/methods/Exchange.h>
#include <handlers/methods/Ledger.h>
#include <handlers/methods/Subscribe.h>
#include <handlers/methods/Transaction.h>
#include <etl/ETLSource.h>

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

static std::unordered_set<std::string> forwardCommands{
    "submit",
    "submit_multisigned",
    "fee",
    "path_find",
    "ripple_path_find",
    "manifest"};

bool
shouldForwardToRippled(Context const& ctx)
{
    auto request = ctx.params;

    if (request.contains("forward") && request.at("forward").is_bool())
        return request.at("forward").as_bool();

    BOOST_LOG_TRIVIAL(debug) << "checked forward";

    if (forwardCommands.find(ctx.method) != forwardCommands.end())
        return true;

    BOOST_LOG_TRIVIAL(debug) << "checked command";

    if (request.contains("ledger_index"))
    {
        auto indexValue = request.at("ledger_index");
        if (indexValue.is_string())
        {
            BOOST_LOG_TRIVIAL(debug) << "checking ledger as string";
            std::string index = indexValue.as_string().c_str();
            return index == "current" || index == "closed";
        }
    }
    
    BOOST_LOG_TRIVIAL(debug) << "checked ledger";

    return false;
}

std::pair<Status, std::uint32_t>
buildResponse(Context& ctx, boost::json::object& result)
{
    if (shouldForwardToRippled(ctx))
    {
        result = ctx.balancer->forwardToRippled(ctx.params);
        return {OK, 1};
    }

    Handler const* handler =
        HandlerTable::instance().getHandler(ctx.version, ctx.method);

    if (!handler || !handler->method || !handler->cost)
        return {Error::rpcUNKNOWN_COMMAND, 1};
    
    auto method = handler->method;

    return {method(ctx, result), handler->cost};
}

}