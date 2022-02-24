#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

Result
doAccountCurrencies(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    ripple::AccountID accountID;
    if (auto const status = getAccount(request, accountID); status)
        return status;

    std::set<std::string> send, receive;
    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (sle.getType() == ripple::ltRIPPLE_STATE)
        {
            ripple::STAmount balance = sle.getFieldAmount(ripple::sfBalance);

            auto lowLimit = sle.getFieldAmount(ripple::sfLowLimit);
            auto highLimit = sle.getFieldAmount(ripple::sfHighLimit);
            bool viewLowest = (lowLimit.getIssuer() == accountID);
            auto lineLimit = viewLowest ? lowLimit : highLimit;
            auto lineLimitPeer = !viewLowest ? lowLimit : highLimit;
            if (!viewLowest)
                balance.negate();

            if (balance < lineLimit)
                receive.insert(ripple::to_string(balance.getCurrency()));
            if ((-balance) < lineLimitPeer)
                send.insert(ripple::to_string(balance.getCurrency()));
        }

        return true;
    };

    traverseOwnedNodes(
        *context.backend,
        accountID,
        lgrInfo.seq,
        std::numeric_limits<std::uint32_t>::max(),
        {},
        context.yield,
        addToResponse);

    response[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
    response[JS(ledger_index)] = lgrInfo.seq;

    response[JS(receive_currencies)] =
        boost::json::value(boost::json::array_kind);
    boost::json::array& jsonReceive =
        response.at(JS(receive_currencies)).as_array();

    for (auto const& currency : receive)
        jsonReceive.push_back(currency.c_str());

    response[JS(send_currencies)] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonSend = response.at(JS(send_currencies)).as_array();

    for (auto const& currency : send)
        jsonSend.push_back(currency.c_str());

    return response;
}

}  // namespace RPC
