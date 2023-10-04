//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#pragma once

#include <data/BackendInterface.h>
#include <etl/ETLService.h>
#include <rpc/RPCHelpers.h>
#include <rpc/common/MetaProcessors.h>
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>

namespace rpc {

template <typename ETLServiceType>
class BaseTransactionEntryHandler
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<ETLServiceType const> etl_;

public:
    struct Output
    {
        uint32_t ledgerIndex{};
        std::string ledgerHash;
        // TODO: use a better type for this
        boost::json::object metadata;
        boost::json::object tx;
        // validated should be sent via framework
        bool validated = true;
    };

    struct Input
    {
        std::string txHash;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
    };

    using Result = HandlerReturnType<Output>;

    BaseTransactionEntryHandler(
        std::shared_ptr<BackendInterface> const& sharedPtrBackend,
        std::shared_ptr<ETLServiceType const> const& etl)
        : sharedPtrBackend_(sharedPtrBackend), etl_(etl)
    {
    }

    static RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion)
    {
        static auto const rpcSpec = RpcSpec{
            {JS(tx_hash),
             meta::WithCustomError{validation::Required{}, Status(ClioError::rpcFIELD_NOT_FOUND_TRANSACTION)},
             validation::Uint256HexStringValidator},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
        };

        return rpcSpec;
    }

    Result
    process(Input input, Context const& ctx) const
    {
        auto const range = sharedPtrBackend_->fetchLedgerRange();
        auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
            *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence);

        if (auto status = std::get_if<Status>(&lgrInfoOrStatus))
            return Error{*status};

        auto const lgrInfo = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);
        auto const dbRet = sharedPtrBackend_->fetchTransaction(ripple::uint256{input.txHash.c_str()}, ctx.yield);
        // Note: transaction_entry is meant to only search a specified ledger for
        // the specified transaction. tx searches the entire range of history. For
        // rippled, having two separate commands made sense, as tx would use SQLite
        // and transaction_entry used the nodestore. For clio though, there is no
        // difference between the implementation of these two, as clio only stores
        // transactions in a transactions table, where the key is the hash. However,
        // the API for transaction_entry says the method only searches the specified
        // ledger; we simulate that here by returning not found if the transaction
        // is in a different ledger than the one specified.
        if (!dbRet || dbRet->ledgerSequence != lgrInfo.seq)
            return Error{Status{RippledError::rpcTXN_NOT_FOUND, "transactionNotFound", "Transaction not found."}};

        auto output = BaseTransactionEntryHandler::Output{};
        auto [txn, meta] = toExpandedJson(*dbRet, NFTokenjson::DISABLE, etl_->getNetworkID());

        output.tx = std::move(txn);
        output.metadata = std::move(meta);
        output.ledgerIndex = lgrInfo.seq;
        output.ledgerHash = ripple::strHex(lgrInfo.hash);

        return output;
    }

private:
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output)
    {
        jv = {
            {JS(validated), output.validated},
            {JS(metadata), output.metadata},
            {JS(tx_json), output.tx},
            {JS(ledger_index), output.ledgerIndex},
            {JS(ledger_hash), output.ledgerHash},
        };
    }

    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv)
    {
        auto input = BaseTransactionEntryHandler::Input{};
        auto const& jsonObject = jv.as_object();

        input.txHash = jv.at(JS(tx_hash)).as_string().c_str();

        if (jsonObject.contains(JS(ledger_hash)))
            input.ledgerHash = jv.at(JS(ledger_hash)).as_string().c_str();

        if (jsonObject.contains(JS(ledger_index)))
        {
            if (!jsonObject.at(JS(ledger_index)).is_string())
            {
                input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
            }
            else if (jsonObject.at(JS(ledger_index)).as_string() != "validated")
            {
                input.ledgerIndex = std::stoi(jv.at(JS(ledger_index)).as_string().c_str());
            }
        }

        return input;
    }
};

/**
 * @brief The transaction_entry method retrieves information on a single transaction from a specific ledger version.
 *
 * For more details see: https://xrpl.org/transaction_entry.html
 */
using TransactionEntryHandler = BaseTransactionEntryHandler<etl::ETLService>;

}  // namespace rpc
