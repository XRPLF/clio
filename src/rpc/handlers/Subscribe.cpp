#include <boost/json.hpp>
#include <subscriptions/SubscriptionManager.h>
#include <webserver/WsBase.h>

#include <rpc/RPCHelpers.h>

namespace RPC {

// these are the streams that take no arguments
static std::unordered_set<std::string> validCommonStreams{
    "ledger",
    "transactions",
    "transactions_proposed",
    "validations",
    "manifests",
    "book_changes"};

Status
validateStreams(boost::json::object const& request)
{
    boost::json::array const& streams = request.at(JS(streams)).as_array();

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

boost::json::object
subscribeToStreams(
    boost::asio::yield_context& yield,
    boost::json::object const& request,
    std::shared_ptr<WsBase> session,
    SubscriptionManager& manager)
{
    boost::json::array const& streams = request.at(JS(streams)).as_array();

    boost::json::object response;
    for (auto const& stream : streams)
    {
        std::string s = stream.as_string().c_str();

        if (s == "ledger")
            response = manager.subLedger(yield, session);
        else if (s == "transactions")
            manager.subTransactions(session);
        else if (s == "transactions_proposed")
            manager.subProposedTransactions(session);
        else if (s == "validations")
            manager.subValidation(session);
        else if (s == "manifests")
            manager.subManifest(session);
        else if (s == "book_changes")
            manager.subBookChanges(session);
        else
            assert(false);
    }
    return response;
}

void
unsubscribeToStreams(
    boost::json::object const& request,
    std::shared_ptr<WsBase> session,
    SubscriptionManager& manager)
{
    boost::json::array const& streams = request.at(JS(streams)).as_array();

    for (auto const& stream : streams)
    {
        std::string s = stream.as_string().c_str();

        if (s == "ledger")
            manager.unsubLedger(session);
        else if (s == "transactions")
            manager.unsubTransactions(session);
        else if (s == "transactions_proposed")
            manager.unsubProposedTransactions(session);
        else if (s == "validations")
            manager.unsubValidation(session);
        else if (s == "manifests")
            manager.unsubManifest(session);
        else if (s == "book_changes")
            manager.unsubBookChanges(session);
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
    boost::json::array const& accounts = request.at(JS(accounts)).as_array();

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
    boost::json::array const& accounts = request.at(JS(accounts)).as_array();

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
        request.at(JS(accounts_proposed)).as_array();

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
        request.at(JS(accounts_proposed)).as_array();

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

std::variant<Status, std::pair<std::vector<ripple::Book>, boost::json::array>>
validateAndGetBooks(
    boost::asio::yield_context& yield,
    boost::json::object const& request,
    std::shared_ptr<Backend::BackendInterface const> const& backend)
{
    if (!request.at(JS(books)).is_array())
        return Status{Error::rpcINVALID_PARAMS, "booksNotArray"};
    boost::json::array const& books = request.at(JS(books)).as_array();

    std::vector<ripple::Book> booksToSub;
    std::optional<Backend::LedgerRange> rng;
    boost::json::array snapshot;
    for (auto const& book : books)
    {
        auto parsedBook = parseBook(book.as_object());
        if (auto status = std::get_if<Status>(&parsedBook))
            return *status;

        auto b = std::get<ripple::Book>(parsedBook);
        booksToSub.push_back(b);
        bool both = book.as_object().contains(JS(both));
        if (both)
            booksToSub.push_back(ripple::reversed(b));

        if (book.as_object().contains(JS(snapshot)))
        {
            if (!rng)
                rng = backend->fetchLedgerRange();
            ripple::AccountID takerID = beast::zero;
            if (book.as_object().contains(JS(taker)))
                if (auto const status = getTaker(book.as_object(), takerID);
                    status)
                    return status;

            auto getOrderBook = [&snapshot, &backend, &rng, &takerID](
                                    auto book,
                                    boost::asio::yield_context& yield) {
                auto bookBase = getBookBase(book);
                auto [offers, retMarker] = backend->fetchBookOffers(
                    bookBase, rng->maxSequence, 200, {}, yield);

                auto orderBook = postProcessOrderBook(
                    offers, book, takerID, *backend, rng->maxSequence, yield);
                std::copy(
                    orderBook.begin(),
                    orderBook.end(),
                    std::back_inserter(snapshot));
            };
            getOrderBook(b, yield);
            if (both)
                getOrderBook(ripple::reversed(b), yield);
        }
    }
    return std::make_pair(booksToSub, snapshot);
}

void
subscribeToBooks(
    std::vector<ripple::Book> const& books,
    std::shared_ptr<WsBase> session,
    SubscriptionManager& manager)
{
    for (auto const& book : books)
    {
        manager.subBook(book, session);
    }
}

void
unsubscribeToBooks(
    std::vector<ripple::Book> const& books,
    std::shared_ptr<WsBase> session,
    SubscriptionManager& manager)
{
    for (auto const& book : books)
    {
        manager.unsubBook(book, session);
    }
}

Result
doSubscribe(Context const& context)
{
    auto request = context.params;

    if (!request.contains(JS(streams)) && !request.contains(JS(accounts)) &&
        !request.contains(JS(accounts_proposed)) &&
        !request.contains(JS(books)))
        return Status{
            Error::rpcINVALID_PARAMS, "does not contain valid subscription"};

    if (request.contains(JS(streams)))
    {
        if (!request.at(JS(streams)).is_array())
            return Status{Error::rpcINVALID_PARAMS, "streamsNotArray"};

        auto status = validateStreams(request);

        if (status)
            return status;
    }

    if (request.contains(JS(accounts)))
    {
        if (!request.at(JS(accounts)).is_array())
            return Status{Error::rpcINVALID_PARAMS, "accountsNotArray"};

        boost::json::array accounts = request.at(JS(accounts)).as_array();
        auto status = validateAccounts(accounts);

        if (status)
            return status;
    }

    if (request.contains(JS(accounts_proposed)))
    {
        if (!request.at(JS(accounts_proposed)).is_array())
            return Status{Error::rpcINVALID_PARAMS, "accountsProposedNotArray"};

        boost::json::array accounts =
            request.at(JS(accounts_proposed)).as_array();
        auto status = validateAccounts(accounts);

        if (status)
            return status;
    }

    std::vector<ripple::Book> books;
    boost::json::array snapshot;
    if (request.contains(JS(books)))
    {
        auto parsed =
            validateAndGetBooks(context.yield, request, context.backend);
        if (auto status = std::get_if<Status>(&parsed))
            return *status;
        auto [bks, snap] =
            std::get<std::pair<std::vector<ripple::Book>, boost::json::array>>(
                parsed);
        books = std::move(bks);
        snapshot = std::move(snap);
    }

    boost::json::object response;
    if (request.contains(JS(streams)))
        response = subscribeToStreams(
            context.yield, request, context.session, *context.subscriptions);

    if (request.contains(JS(accounts)))
        subscribeToAccounts(request, context.session, *context.subscriptions);

    if (request.contains(JS(accounts_proposed)))
        subscribeToAccountsProposed(
            request, context.session, *context.subscriptions);

    if (request.contains(JS(books)))
        subscribeToBooks(books, context.session, *context.subscriptions);

    if (snapshot.size())
        response[JS(offers)] = snapshot;
    return response;
}

Result
doUnsubscribe(Context const& context)
{
    auto request = context.params;

    if (!request.contains(JS(streams)) && !request.contains(JS(accounts)) &&
        !request.contains(JS(accounts_proposed)) &&
        !request.contains(JS(books)))
        return Status{
            Error::rpcINVALID_PARAMS, "does not contain valid subscription"};

    if (request.contains(JS(streams)))
    {
        if (!request.at(JS(streams)).is_array())
            return Status{Error::rpcINVALID_PARAMS, "streamsNotArray"};

        auto status = validateStreams(request);

        if (status)
            return status;
    }

    if (request.contains(JS(accounts)))
    {
        if (!request.at(JS(accounts)).is_array())
            return Status{Error::rpcINVALID_PARAMS, "accountsNotArray"};

        boost::json::array accounts = request.at(JS(accounts)).as_array();
        auto status = validateAccounts(accounts);

        if (status)
            return status;
    }

    if (request.contains(JS(accounts_proposed)))
    {
        if (!request.at(JS(accounts_proposed)).is_array())
            return Status{Error::rpcINVALID_PARAMS, "accountsProposedNotArray"};

        boost::json::array accounts =
            request.at(JS(accounts_proposed)).as_array();
        auto status = validateAccounts(accounts);

        if (status)
            return status;
    }

    std::vector<ripple::Book> books;
    if (request.contains(JS(books)))
    {
        auto parsed =
            validateAndGetBooks(context.yield, request, context.backend);

        if (auto status = std::get_if<Status>(&parsed))
            return *status;

        auto [bks, snap] =
            std::get<std::pair<std::vector<ripple::Book>, boost::json::array>>(
                parsed);

        books = std::move(bks);
    }

    if (request.contains(JS(streams)))
        unsubscribeToStreams(request, context.session, *context.subscriptions);

    if (request.contains(JS(accounts)))
        unsubscribeToAccounts(request, context.session, *context.subscriptions);

    if (request.contains(JS(accounts_proposed)))
        unsubscribeToAccountsProposed(
            request, context.session, *context.subscriptions);

    if (request.contains("books"))
        unsubscribeToBooks(books, context.session, *context.subscriptions);

    boost::json::object response = {{"status", "success"}};
    return response;
}

}  // namespace RPC
