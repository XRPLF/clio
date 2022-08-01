#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <boost/json.hpp>

#include <clio/main/Application.h>
#include <clio/rpc/RPCHelpers.h>

// {
//   account: <ident>,
//   strict: <bool>        // optional (default false)
//                         //   if true only allow public keys and addresses.
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
//   signer_lists : <bool> // optional (default false)
//                         //   if true return SignerList(s).
//   queue : <bool>        // optional (default false)
//                         //   if true return information about transactions
//                         //   in the current TxQ, only if the requested
//                         //   ledger is open. Otherwise if true, returns an
//                         //   error.
// }

namespace RPC {

Result
doAccountInfo(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    std::string strIdent;
    if (request.contains(JS(account)))
        strIdent = request.at(JS(account)).as_string().c_str();
    else if (request.contains(JS(ident)))
        strIdent = request.at(JS(ident)).as_string().c_str();
    else
        return Status{Error::rpcACT_MALFORMED};

    // We only need to fetch the ledger header because the ledger hash is
    // supposed to be included in the response. The ledger sequence is specified
    // in the request
    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    // Get info on account.
    auto accountID = accountFromStringStrict(strIdent);
    if (!accountID)
        return Status{Error::rpcACT_MALFORMED};

    assert(accountID.has_value());

    auto key = ripple::keylet::account(accountID.value());

    auto start = std::chrono::system_clock::now();
    std::optional<std::vector<unsigned char>> dbResponse =
        context.app.backend().fetchLedgerObject(
            key.key, lgrInfo.seq, context.yield);
    auto end = std::chrono::system_clock::now();

    if (!dbResponse)
    {
        return Status{Error::rpcACT_NOT_FOUND};
    }

    ripple::STLedgerEntry sle{
        ripple::SerialIter{dbResponse->data(), dbResponse->size()}, key.key};

    if (!key.check(sle))
        return Status{Error::rpcDB_DESERIALIZATION};

    // if (!binary)
    //     response[JS(account_data)] = getJson(sle);
    // else
    //     response[JS(account_data)] = ripple::strHex(*dbResponse);
    // response[JS(db_time)] = time;

    response[JS(account_data)] = toJson(sle);
    response[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
    response[JS(ledger_index)] = lgrInfo.seq;

    // Return SignerList(s) if that is requested.
    if (request.contains(JS(signer_lists)) &&
        request.at(JS(signer_lists)).as_bool())
    {
        // We put the SignerList in an array because of an anticipated
        // future when we support multiple signer lists on one account.
        boost::json::array signerList;
        auto signersKey = ripple::keylet::signers(*accountID);

        // This code will need to be revisited if in the future we
        // support multiple SignerLists on one account.
        auto const signers = context.app.backend().fetchLedgerObject(
            signersKey.key, lgrInfo.seq, context.yield);
        if (signers)
        {
            ripple::STLedgerEntry sleSigners{
                ripple::SerialIter{signers->data(), signers->size()},
                signersKey.key};
            if (!signersKey.check(sleSigners))
                return Status{Error::rpcDB_DESERIALIZATION};

            signerList.push_back(toJson(sleSigners));
        }

        response[JS(account_data)].as_object()[JS(signer_lists)] =
            std::move(signerList);
    }

    return response;
}

}  // namespace RPC
