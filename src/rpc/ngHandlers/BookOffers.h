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

#include <backend/BackendInterface.h>
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>

namespace RPCng {
class BookOffersHandler
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    struct Output
    {
        std::string ledgerHash;
        uint32_t ledgerIndex;
        boost::json::array offers;
        bool validated = true;
    };

    // the taker is not really used in both clio and rippled, both of them
    // return all the offers regardless the funding status
    struct Input
    {
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        uint32_t limit = 50;
        std::optional<ripple::AccountID> taker;
        ripple::Currency paysCurrency;
        ripple::Currency getsCurrency;
        // accountID will be filled by input converter, if no issuer is given,
        // will use XRP issuer
        ripple::AccountID paysID = ripple::xrpAccount();
        ripple::AccountID getsID = ripple::xrpAccount();
    };

    using Result = RPCng::HandlerReturnType<Output>;

    BookOffersHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend)
        : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    RpcSpecConstRef
    spec() const
    {
        static auto const rpcSpec = RpcSpec{
            {JS(taker_gets),
             validation::Required{},
             validation::Type<boost::json::object>{},
             validation::Section{
                 {JS(currency),
                  validation::Required{},
                  validation::WithCustomError{
                      validation::CurrencyValidator,
                      RPC::Status(RPC::RippledError::rpcDST_AMT_MALFORMED)}},
                 {JS(issuer),
                  validation::WithCustomError{
                      validation::IssuerValidator,
                      RPC::Status(RPC::RippledError::rpcDST_ISR_MALFORMED)}}}},
            {JS(taker_pays),
             validation::Required{},
             validation::Type<boost::json::object>{},
             validation::Section{
                 {JS(currency),
                  validation::Required{},
                  validation::WithCustomError{
                      validation::CurrencyValidator,
                      RPC::Status(RPC::RippledError::rpcSRC_CUR_MALFORMED)}},
                 {JS(issuer),
                  validation::WithCustomError{
                      validation::IssuerValidator,
                      RPC::Status(RPC::RippledError::rpcSRC_ISR_MALFORMED)}}}},
            {JS(taker), validation::AccountValidator},
            {JS(limit),
             validation::Type<uint32_t>{},
             validation::Between{1, 100}},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator}};
        return rpcSpec;
    }

    Result
    process(Input input, Context const& ctx) const;
};

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    BookOffersHandler::Output const& output);

BookOffersHandler::Input
tag_invoke(
    boost::json::value_to_tag<BookOffersHandler::Input>,
    boost::json::value const& jv);
}  // namespace RPCng
