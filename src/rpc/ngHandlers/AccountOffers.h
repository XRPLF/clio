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
#include <rpc/RPCHelpers.h>
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>

#include <boost/asio/spawn.hpp>

namespace RPCng {
class AccountOffersHandler
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    struct Offer
    {
        uint32_t flags;
        uint32_t seq;
        ripple::STAmount takerGets;
        ripple::STAmount takerPays;
        std::string quality;
        std::optional<uint32_t> expiration;
    };

    struct Output
    {
        std::string account;
        std::string ledgerHash;
        uint32_t ledgerIndex;
        std::vector<Offer> offers;
        std::optional<std::string> marker;
        // validated should be sent via framework
        bool validated = true;
    };

    // TODO:we did not implement the "strict" field
    struct Input
    {
        std::string account;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        uint32_t limit = 200;
        std::optional<std::string> marker;
    };

    using Result = RPCng::HandlerReturnType<Output>;

    AccountOffersHandler(
        std::shared_ptr<BackendInterface> const& sharedPtrBackend)
        : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    RpcSpecConstRef
    spec() const
    {
        static auto const rpcSpec = RpcSpec{
            {JS(account), validation::Required{}, validation::AccountValidator},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(marker), validation::AccountMarkerValidator},
            {JS(limit),
             validation::Type<uint32_t>{},
             validation::Between{10, 400}}};
        return rpcSpec;
    }

    Result
    process(Input input, Context ctx) const;

private:
    void
    addOffer(std::vector<Offer>& offers, ripple::SLE const& offerSle) const;
};

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountOffersHandler::Output const& output);

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountOffersHandler::Offer const& offer);

AccountOffersHandler::Input
tag_invoke(
    boost::json::value_to_tag<AccountOffersHandler::Input>,
    boost::json::value const& jv);
}  // namespace RPCng
