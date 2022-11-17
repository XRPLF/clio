#include <log/Logger.h>
#include <rpc/RPCHelpers.h>

using namespace clio;

// local to compilation unit loggers
namespace {
clio::Logger gLog{"RPC"};
}  // namespace

namespace RPC {

Result
doAccountTx(Context const& context)
{
    ripple::AccountID accountID;
    if (auto const status = getAccount(context.params, accountID); status)
        return status;

    constexpr std::string_view outerFuncName = __func__;
    auto const maybeResponse =
        traverseTransactions(
            context,
            [&accountID, &outerFuncName](
                std::shared_ptr<Backend::BackendInterface const> const& backend,
                std::uint32_t const limit,
                bool const forward,
                std::optional<Backend::TransactionsCursor> const& cursorIn,
                boost::asio::yield_context& yield) {
                auto const start = std::chrono::system_clock::now();
                auto const txnsAndCursor = backend->fetchAccountTransactions(
                    accountID, limit, forward, cursorIn, yield);
                gLog.info()
                    << outerFuncName << " db fetch took "
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now() - start)
                           .count()
                    << " milliseconds - num blobs = "
                    << txnsAndCursor.txns.size();
                return txnsAndCursor;
            });

    if (auto const status = std::get_if<Status>(&maybeResponse); status)
        return *status;
    auto response = std::get<boost::json::object>(maybeResponse);

    response[JS(account)] = ripple::to_string(accountID);
    return response;
}

}  // namespace RPC
