#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

Result
doGatewayBalances(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    if (!request.contains("account"))
        return Status{Error::rpcINVALID_PARAMS, "missingAccount"};

    if (!request.at("account").is_string())
        return Status{Error::rpcINVALID_PARAMS, "accountNotString"};

    auto accountID =
        accountFromStringStrict(request.at("account").as_string().c_str());

    if (!accountID)
        return Status{Error::rpcINVALID_PARAMS, "malformedAccount"};

    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    std::map<ripple::Currency, ripple::STAmount> sums;
    std::map<ripple::AccountID, std::vector<ripple::STAmount>> hotBalances;
    std::map<ripple::AccountID, std::vector<ripple::STAmount>> assets;
    std::map<ripple::AccountID, std::vector<ripple::STAmount>> frozenBalances;
    std::set<ripple::AccountID> hotWallets;

    if (request.contains("hot_wallet"))
    {
        auto getAccountID =
            [](auto const& j) -> std::optional<ripple::AccountID> {
            if (j.is_string())
            {
                auto const pk = ripple::parseBase58<ripple::PublicKey>(
                    ripple::TokenType::AccountPublic, j.as_string().c_str());
                if (pk)
                {
                    return ripple::calcAccountID(*pk);
                }

                return ripple::parseBase58<ripple::AccountID>(
                    j.as_string().c_str());
            }
            return {};
        };

        auto const& hw = request.at("hot_wallet");
        bool valid = true;

        // null is treated as a valid 0-sized array of hotwallet
        if (hw.is_array())
        {
            auto const& arr = hw.as_array();
            for (unsigned i = 0; i < arr.size(); ++i)
            {
                if (auto id = getAccountID(arr[i]))
                    hotWallets.insert(*id);
                else
                    valid = false;
            }
        }
        else if (hw.is_string())
        {
            if (auto id = getAccountID(hw))
                hotWallets.insert(*id);
            else
                valid = false;
        }
        else
        {
            valid = false;
        }

        if (!valid)
        {
            response["error"] = "invalidHotWallet";
            return response;
        }
    }

    // Traverse the cold wallet's trust lines
    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (sle.getType() == ripple::ltRIPPLE_STATE)
        {
            ripple::STAmount balance = sle.getFieldAmount(ripple::sfBalance);

            auto lowLimit = sle.getFieldAmount(ripple::sfLowLimit);
            auto highLimit = sle.getFieldAmount(ripple::sfHighLimit);
            auto lowID = lowLimit.getIssuer();
            auto highID = highLimit.getIssuer();
            bool viewLowest = (lowLimit.getIssuer() == accountID);
            auto lineLimit = viewLowest ? lowLimit : highLimit;
            auto lineLimitPeer = !viewLowest ? lowLimit : highLimit;
            auto flags = sle.getFieldU32(ripple::sfFlags);
            auto freeze = flags &
                (viewLowest ? ripple::lsfLowFreeze : ripple::lsfHighFreeze);
            if (!viewLowest)
                balance.negate();

            int balSign = balance.signum();
            if (balSign == 0)
                return true;

            auto const& peer = !viewLowest ? lowID : highID;

            // Here, a negative balance means the cold wallet owes (normal)
            // A positive balance means the cold wallet has an asset
            // (unusual)

            if (hotWallets.count(peer) > 0)
            {
                // This is a specified hot wallet
                hotBalances[peer].push_back(balance);
            }
            else if (balSign > 0)
            {
                // This is a gateway asset
                assets[peer].push_back(balance);
            }
            else if (freeze)
            {
                // An obligation the gateway has frozen
                frozenBalances[peer].push_back(balance);
            }
            else
            {
                // normal negative balance, obligation to customer
                auto& bal = sums[balance.getCurrency()];
                if (bal == beast::zero)
                {
                    // This is needed to set the currency code correctly
                    bal = -balance;
                }
                else
                    bal -= balance;
            }
        }
        return true;
    };

    traverseOwnedNodes(
        *context.backend,
        *accountID,
        lgrInfo.seq,
        std::numeric_limits<std::uint32_t>::max(),
        {},
        context.yield,
        addToResponse);

    if (!sums.empty())
    {
        boost::json::object obj;
        for (auto const& [k, v] : sums)
        {
            obj[ripple::to_string(k)] = v.getText();
        }
        response["obligations"] = std::move(obj);
    }

    auto toJson =
        [](std::map<ripple::AccountID, std::vector<ripple::STAmount>> const&
               balances) {
            boost::json::object obj;
            if (!balances.empty())
            {
                for (auto const& [accId, accBalances] : balances)
                {
                    boost::json::array arr;
                    for (auto const& balance : accBalances)
                    {
                        boost::json::object entry;
                        entry["currency"] =
                            ripple::to_string(balance.issue().currency);
                        entry["value"] = balance.getText();
                        arr.push_back(std::move(entry));
                    }
                    obj[ripple::to_string(accId)] = std::move(arr);
                }
            }
            return obj;
        };

    if (auto balances = toJson(hotBalances); balances.size())
        response["balances"] = balances;
    if (auto balances = toJson(frozenBalances); balances.size())
        response["frozen_balances"] = balances;
    if (auto balances = toJson(assets); assets.size())
        response["assets"] = toJson(assets);
    response["account"] = request.at("account");
    response["ledger_index"] = lgrInfo.seq;
    response["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    return response;
}
}  // namespace RPC
