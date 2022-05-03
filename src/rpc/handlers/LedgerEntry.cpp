#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <boost/json.hpp>

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>
// {
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
//   ...
// }

namespace RPC {

using boost::json::value_to;

Result
doLedgerEntry(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    bool binary =
        request.contains("binary") ? request.at("binary").as_bool() : false;

    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    ripple::uint256 key;
    if (request.contains("index"))
    {
        if (!request.at("index").is_string())
            return Status{Error::rpcINVALID_PARAMS, "indexNotString"};

        if (!key.parseHex(request.at("index").as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "malformedIndex"};
    }
    else if (request.contains("account_root"))
    {
        if (!request.at("account_root").is_string())
            return Status{Error::rpcINVALID_PARAMS, "account_rootNotString"};

        auto const account = ripple::parseBase58<ripple::AccountID>(
            request.at("account_root").as_string().c_str());
        if (!account || account->isZero())
            return Status{Error::rpcINVALID_PARAMS, "malformedAddress"};
        else
            key = ripple::keylet::account(*account).key;
    }
    else if (request.contains("check"))
    {
        if (!request.at("check").is_string())
            return Status{Error::rpcINVALID_PARAMS, "checkNotString"};

        if (!key.parseHex(request.at("check").as_string().c_str()))
        {
            return Status{Error::rpcINVALID_PARAMS, "checkMalformed"};
        }
    }
    else if (request.contains("deposit_preauth"))
    {
        if (!request.at("deposit_preauth").is_object())
        {
            if (!request.at("deposit_preauth").is_string() ||
                !key.parseHex(
                    request.at("deposit_preauth").as_string().c_str()))
            {
                return Status{
                    Error::rpcINVALID_PARAMS, "deposit_preauthMalformed"};
            }
        }
        else if (
            !request.at("deposit_preauth").as_object().contains("owner") ||
            !request.at("deposit_preauth").as_object().at("owner").is_string())
        {
            return Status{Error::rpcINVALID_PARAMS, "ownerNotString"};
        }
        else if (
            !request.at("deposit_preauth").as_object().contains("authorized") ||
            !request.at("deposit_preauth")
                 .as_object()
                 .at("authorized")
                 .is_string())
        {
            return Status{Error::rpcINVALID_PARAMS, "authorizedNotString"};
        }
        else
        {
            boost::json::object const& deposit_preauth =
                request.at("deposit_preauth").as_object();

            auto const owner = ripple::parseBase58<ripple::AccountID>(
                deposit_preauth.at("owner").as_string().c_str());

            auto const authorized = ripple::parseBase58<ripple::AccountID>(
                deposit_preauth.at("authorized").as_string().c_str());

            if (!owner)
                return Status{Error::rpcINVALID_PARAMS, "malformedOwner"};
            else if (!authorized)
                return Status{Error::rpcINVALID_PARAMS, "malformedAuthorized"};
            else
                key = ripple::keylet::depositPreauth(*owner, *authorized).key;
        }
    }
    else if (request.contains("directory"))
    {
        if (!request.at("directory").is_object())
        {
            if (!request.at("directory").is_string())
                return Status{Error::rpcINVALID_PARAMS, "directoryNotString"};

            if (!key.parseHex(request.at("directory").as_string().c_str()))
            {
                return Status{Error::rpcINVALID_PARAMS, "malformedDirectory"};
            }
        }
        else if (
            request.at("directory").as_object().contains("sub_index") &&
            !request.at("directory").as_object().at("sub_index").is_int64())
        {
            return Status{Error::rpcINVALID_PARAMS, "sub_indexNotInt"};
        }
        else
        {
            auto directory = request.at("directory").as_object();
            std::uint64_t subIndex = directory.contains("sub_index")
                ? boost::json::value_to<std::uint64_t>(
                      directory.at("sub_index"))
                : 0;

            if (directory.contains("dir_root"))
            {
                ripple::uint256 uDirRoot;

                if (directory.contains("owner"))
                {
                    // May not specify both dir_root and owner.
                    return Status{
                        Error::rpcINVALID_PARAMS,
                        "mayNotSpecifyBothDirRootAndOwner"};
                }
                else if (!uDirRoot.parseHex(
                             directory.at("dir_root").as_string().c_str()))
                {
                    return Status{Error::rpcINVALID_PARAMS, "malformedDirRoot"};
                }
                else
                {
                    key = ripple::keylet::page(uDirRoot, subIndex).key;
                }
            }
            else if (directory.contains("owner"))
            {
                auto const ownerID = ripple::parseBase58<ripple::AccountID>(
                    directory.at("owner").as_string().c_str());

                if (!ownerID)
                {
                    return Status{Error::rpcINVALID_PARAMS, "malformedAddress"};
                }
                else
                {
                    key = ripple::keylet::page(
                              ripple::keylet::ownerDir(*ownerID), subIndex)
                              .key;
                }
            }
            else
            {
                return Status{
                    Error::rpcINVALID_PARAMS, "missingOwnerOrDirRoot"};
            }
        }
    }
    else if (request.contains("escrow"))
    {
        if (!request.at("escrow").is_object())
        {
            if (!key.parseHex(request.at("escrow").as_string().c_str()))
                return Status{Error::rpcINVALID_PARAMS, "malformedEscrow"};
        }
        else if (
            !request.at("escrow").as_object().contains("owner") ||
            !request.at("escrow").as_object().at("owner").is_string())
        {
            return Status{Error::rpcINVALID_PARAMS, "malformedOwner"};
        }
        else if (
            !request.at("escrow").as_object().contains("seq") ||
            !request.at("escrow").as_object().at("seq").is_int64())
        {
            return Status{Error::rpcINVALID_PARAMS, "malformedSeq"};
        }
        else
        {
            auto const id =
                ripple::parseBase58<ripple::AccountID>(request.at("escrow")
                                                           .as_object()
                                                           .at("owner")
                                                           .as_string()
                                                           .c_str());

            if (!id)
                return Status{Error::rpcINVALID_PARAMS, "malformedOwner"};
            else
            {
                std::uint32_t seq =
                    request.at("escrow").as_object().at("seq").as_int64();
                key = ripple::keylet::escrow(*id, seq).key;
            }
        }
    }
    else if (request.contains("offer"))
    {
        if (!request.at("offer").is_object())
        {
            if (!key.parseHex(request.at("offer").as_string().c_str()))
                return Status{Error::rpcINVALID_PARAMS, "malformedOffer"};
        }
        else if (
            !request.at("offer").as_object().contains("account") ||
            !request.at("offer").as_object().at("account").is_string())
        {
            return Status{Error::rpcINVALID_PARAMS, "malformedAccount"};
        }
        else if (
            !request.at("offer").as_object().contains("seq") ||
            !request.at("offer").as_object().at("seq").is_int64())
        {
            return Status{Error::rpcINVALID_PARAMS, "malformedSeq"};
        }
        else
        {
            auto offer = request.at("offer").as_object();
            auto const id = ripple::parseBase58<ripple::AccountID>(
                offer.at("account").as_string().c_str());

            if (!id)
                return Status{Error::rpcINVALID_PARAMS, "malformedAccount"};
            else
            {
                std::uint32_t seq =
                    boost::json::value_to<std::uint32_t>(offer.at("seq"));
                key = ripple::keylet::offer(*id, seq).key;
            }
        }
    }
    else if (request.contains("payment_channel"))
    {
        if (!request.at("payment_channel").is_string())
            return Status{Error::rpcINVALID_PARAMS, "paymentChannelNotString"};

        if (!key.parseHex(request.at("payment_channel").as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "malformedPaymentChannel"};
    }
    else if (request.contains("ripple_state"))
    {
        if (!request.at("ripple_state").is_object())
            return Status{Error::rpcINVALID_PARAMS, "rippleStateNotObject"};

        ripple::Currency currency;
        boost::json::object const& state =
            request.at("ripple_state").as_object();

        if (!state.contains("currency") || !state.at("currency").is_string())
        {
            return Status{Error::rpcINVALID_PARAMS, "malformedCurrency"};
        }

        if (!state.contains("accounts") || !state.at("accounts").is_array() ||
            2 != state.at("accounts").as_array().size() ||
            !state.at("accounts").as_array().at(0).is_string() ||
            !state.at("accounts").as_array().at(1).is_string() ||
            (state.at("accounts").as_array().at(0).as_string() ==
             state.at("accounts").as_array().at(1).as_string()))
        {
            return Status{Error::rpcINVALID_PARAMS, "malformedAccounts"};
        }

        auto const id1 = ripple::parseBase58<ripple::AccountID>(
            state.at("accounts").as_array().at(0).as_string().c_str());
        auto const id2 = ripple::parseBase58<ripple::AccountID>(
            state.at("accounts").as_array().at(1).as_string().c_str());

        if (!id1 || !id2)
            return Status{Error::rpcINVALID_PARAMS, "malformedAccounts"};

        else if (!ripple::to_currency(
                     currency, state.at("currency").as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "malformedCurrency"};

        key = ripple::keylet::line(*id1, *id2, currency).key;
    }
    else if (request.contains("ticket"))
    {
        if (!request.at("ticket").is_object())
        {
            if (!request.at("ticket").is_string())
                return Status{Error::rpcINVALID_PARAMS, "ticketNotString"};

            if (!key.parseHex(request.at("ticket").as_string().c_str()))
                return Status{Error::rpcINVALID_PARAMS, "malformedTicket"};
        }
        else if (
            !request.at("ticket").as_object().contains("account") ||
            !request.at("ticket").as_object().at("account").is_string())
        {
            return Status{Error::rpcINVALID_PARAMS, "malformedTicketAccount"};
        }
        else if (
            !request.at("ticket").as_object().contains("ticket_seq") ||
            !request.at("ticket").as_object().at("ticket_seq").is_int64())
        {
            return Status{Error::rpcINVALID_PARAMS, "malformedTicketSeq"};
        }
        else
        {
            auto const id =
                ripple::parseBase58<ripple::AccountID>(request.at("ticket")
                                                           .as_object()
                                                           .at("account")
                                                           .as_string()
                                                           .c_str());

            if (!id)
                return Status{
                    Error::rpcINVALID_PARAMS, "malformedTicketAccount"};
            else
            {
                std::uint32_t seq =
                    request.at("offer").as_object().at("ticket_seq").as_int64();

                key = ripple::getTicketIndex(*id, seq);
            }
        }
    }
    else
    {
        return Status{Error::rpcINVALID_PARAMS, "unknownOption"};
    }

    auto start = std::chrono::system_clock::now();
    auto dbResponse =
        context.backend->fetchLedgerObject(key, lgrInfo.seq, context.yield);
    auto end = std::chrono::system_clock::now();

    if (!dbResponse or dbResponse->size() == 0)
        return Status{Error::rpcOBJECT_NOT_FOUND, "entryNotFound"};

    response["index"] = ripple::strHex(key);
    response["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    response["ledger_index"] = lgrInfo.seq;

    if (binary)
    {
        response["node_binary"] = ripple::strHex(*dbResponse);
    }
    else
    {
        ripple::STLedgerEntry sle{
            ripple::SerialIter{dbResponse->data(), dbResponse->size()}, key};
        response["node"] = toJson(sle);
    }

    return response;
}

}  // namespace RPC
