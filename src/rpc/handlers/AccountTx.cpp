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

#include <log/Logger.h>
#include <rpc/RPCHelpers.h>
#include <util/Profiler.h>

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
    auto const maybeResponse = traverseTransactions(
        context,
        [&accountID, &outerFuncName](
            std::shared_ptr<Backend::BackendInterface const> const& backend,
            std::uint32_t const limit,
            bool const forward,
            std::optional<Backend::TransactionsCursor> const& cursorIn,
            boost::asio::yield_context& yield) {
            auto [txnsAndCursor, timeDiff] = util::timed(
                [&]() { return backend->fetchAccountTransactions(accountID, limit, forward, cursorIn, yield); });
            gLog.info() << outerFuncName << " db fetch took " << timeDiff
                        << " milliseconds - num blobs = " << txnsAndCursor.txns.size();
            return txnsAndCursor;
        });

    if (auto const status = std::get_if<Status>(&maybeResponse); status)
        return *status;
    auto response = std::get<boost::json::object>(maybeResponse);

    response[JS(account)] = ripple::to_string(accountID);
    return response;
}

}  // namespace RPC
