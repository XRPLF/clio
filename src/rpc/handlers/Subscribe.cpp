#include <boost/json.hpp>
#include <webserver/SubscriptionManager.h>
#include <webserver/WsBase.h>

#include <rpc/RPCHelpers.h>

namespace RPC {

// these are the streams that take no arguments
static std::unordered_set<std::string> validCommonStreams{
    "ledger",
    "transactions",
    "transactions_proposed"};

Status
validateStreams(boost::json::object const& request)
{
    boost::json::array const& streams = request.at("streams").as_array();

    for (auto const& stream : streams)
    {
        if (!stream.is_string())
            return Status{Error::rpcINVALID_PARAMS, "streamNotString"};

        std::string s = stream.as_string().c_str();

        if (validCommonStreams.find(s) == validCommonStreams.end())
            return Status{Error::rpcINVALID_PARAMS, "invalidStream" + s};
    }

    return OK;
}

void
subscribeToStreams(
    boost::json::object const& request,
    std::shared_ptr<WsBase> session,
    SubscriptionManager& manager)
{
    boost::json::array const& streams = request.at("streams").as_array();

    for (auto const& stream : streams)
    {
        std::string s = stream.as_string().c_str();

        if (s == "ledger")
            manager.subLedger(session);
        else if (s == "transactions")
            manager.subTransactions(session);
        else if (s == "transactions_proposed")
            manager.subProposedTransactions(session);
        else
            assert(false);
    }
}

void
unsubscribeToStreams(
    boost::json::object const& request,
    std::shared_ptr<WsBase> session,
    SubscriptionManager& manager)
{
    boost::json::array const& streams = request.at("streams").as_array();

    for (auto const& stream : streams)
    {
        std::string s = stream.as_string().c_str();

        if (s == "ledger")
            manager.unsubLedger(session);
        else if (s == "transactions")
            manager.unsubTransactions(session);
        else if (s == "transactions_proposed")
            manager.unsubProposedTransactions(session);
        else
            assert(false);
    }
}

Status
validateAccounts(boost::json::array const& accounts)
{
    for (auto const& account : accounts)
    {
        if (!account.is_string())
            return Status{Error::rpcINVALID_PARAMS, "accountNotString"};

        std::string s = account.as_string().c_str();
        auto id = accountFromStringStrict(s);

        if (!id)
            return Status{Error::rpcINVALID_PARAMS, "invalidAccount" + s};
    }

    return OK;
}

void
subscribeToAccounts(
    boost::json::object const& request,
    std::shared_ptr<WsBase> session,
    SubscriptionManager& manager)
{
    boost::json::array const& accounts = request.at("accounts").as_array();

    for (auto const& account : accounts)
    {
        std::string s = account.as_string().c_str();

        auto accountID = accountFromStringStrict(s);

        if (!accountID)
        {
            assert(false);
            continue;
        }

        manager.subAccount(*accountID, session);
    }
}

void
unsubscribeToAccounts(
    boost::json::object const& request,
    std::shared_ptr<WsBase> session,
    SubscriptionManager& manager)
{
    boost::json::array const& accounts = request.at("accounts").as_array();

    for (auto const& account : accounts)
    {
        std::string s = account.as_string().c_str();

        auto accountID = accountFromStringStrict(s);

        if (!accountID)
        {
            assert(false);
            continue;
        }

        manager.unsubAccount(*accountID, session);
    }
}

void
subscribeToAccountsProposed(
    boost::json::object const& request,
    std::shared_ptr<WsBase> session,
    SubscriptionManager& manager)
{
    boost::json::array const& accounts =
        request.at("accounts_proposed").as_array();

    for (auto const& account : accounts)
    {
        std::string s = account.as_string().c_str();

        auto accountID = ripple::parseBase58<ripple::AccountID>(s);

        if (!accountID)
        {
            assert(false);
            continue;
        }

        manager.subProposedAccount(*accountID, session);
    }
}

void
unsubscribeToAccountsProposed(
    boost::json::object const& request,
    std::shared_ptr<WsBase> session,
    SubscriptionManager& manager)
{
    boost::json::array const& accounts =
        request.at("accounts_proposed").as_array();

    for (auto const& account : accounts)
    {
        std::string s = account.as_string().c_str();

        auto accountID = ripple::parseBase58<ripple::AccountID>(s);

        if (!accountID)
        {
            assert(false);
            continue;
        }

        manager.unsubProposedAccount(*accountID, session);
    }
}

std::variant<Status, std::vector<ripple::Book>>
validateAndGetBooks(boost::json::object const& request)
{
    if (!request.at("books").is_array())
        return Status{Error::rpcINVALID_PARAMS, "booksNotArray"};
    boost::json::array const& books = request.at("books").as_array();

    std::vector<ripple::Book> booksToSub;
    for (auto const& book : books)
    {
        auto parsed = parseBook(book.as_object());
        if (auto status = std::get_if<Status>(&parsed))
            return *status;
        else
        {
            auto b = std::get<ripple::Book>(parsed);
            booksToSub.push_back(b);
            if (book.as_object().contains("both"))
                booksToSub.push_back(ripple::reversed(b));
        }
    }
    return booksToSub;
}
void
subscribeToBooks(
    std::vector<ripple::Book> const& books,
    std::shared_ptr<WsBase> session,
    SubscriptionManager& manager)
{
    for (auto const book : books)
    {
        manager.subBook(book, session);
    }
}
Result
doSubscribe(Context const& context)
{
    auto request = context.params;

    if (request.contains("streams"))
    {
        if (!request.at("streams").is_array())
            return Status{Error::rpcINVALID_PARAMS, "streamsNotArray"};

        auto status = validateStreams(request);

        if (status)
            return status;
    }

    if (request.contains("accounts"))
    {
        if (!request.at("accounts").is_array())
            return Status{Error::rpcINVALID_PARAMS, "accountsNotArray"};

        boost::json::array accounts = request.at("accounts").as_array();
        auto status = validateAccounts(accounts);

        if (status)
            return status;
    }

    if (request.contains("accounts_proposed"))
    {
        if (!request.at("accounts_proposed").is_array())
            return Status{Error::rpcINVALID_PARAMS, "accountsProposedNotArray"};

        boost::json::array accounts =
            request.at("accounts_proposed").as_array();
        auto status = validateAccounts(accounts);

        if (status)
            return status;
    }
    std::vector<ripple::Book> books;
    if (request.contains("books"))
    {
        auto parsed = validateAndGetBooks(request);
        if (auto status = std::get_if<Status>(&parsed))
            return *status;
        books = std::get<std::vector<ripple::Book>>(parsed);
    }

    if (request.contains("streams"))
        subscribeToStreams(request, context.session, *context.subscriptions);

    if (request.contains("accounts"))
        subscribeToAccounts(request, context.session, *context.subscriptions);

    if (request.contains("accounts_proposed"))
        subscribeToAccountsProposed(
            request, context.session, *context.subscriptions);

    if (request.contains("books"))
        subscribeToBooks(books, context.session, *context.subscriptions);

    boost::json::object response = {{"status", "success"}};
    return response;
}

Result
doUnsubscribe(Context const& context)
{
    auto request = context.params;

    if (request.contains("streams"))
    {
        if (!request.at("streams").is_array())
            return Status{Error::rpcINVALID_PARAMS, "streamsNotArray"};

        auto status = validateStreams(request);

        if (status)
            return status;
    }

    if (request.contains("accounts"))
    {
        if (!request.at("accounts").is_array())
            return Status{Error::rpcINVALID_PARAMS, "accountsNotArray"};

        boost::json::array accounts = request.at("accounts").as_array();
        auto status = validateAccounts(accounts);

        if (status)
            return status;
    }

    if (request.contains("accounts_proposed"))
    {
        if (!request.at("accounts_proposed").is_array())
            return Status{Error::rpcINVALID_PARAMS, "accountsProposedNotArray"};

        boost::json::array accounts =
            request.at("accounts_proposed").as_array();
        auto status = validateAccounts(accounts);

        if (status)
            return status;
    }

    if (request.contains("streams"))
        unsubscribeToStreams(request, context.session, *context.subscriptions);

    if (request.contains("accounts"))
        unsubscribeToAccounts(request, context.session, *context.subscriptions);

    if (request.contains("accounts_proposed"))
        unsubscribeToAccountsProposed(
            request, context.session, *context.subscriptions);

    boost::json::object response = {{"status", "success"}};
    return response;
}

}  // namespace RPC
