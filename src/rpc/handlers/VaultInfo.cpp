//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "rpc/handlers/VaultInfo.hpp"

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace rpc {

VaultInfoHandler::VaultInfoHandler(std::shared_ptr<BackendInterface> const& backend) : sharedPtrBackend_{backend}
{
}

VaultInfoHandler::Result
VaultInfoHandler::process(VaultInfoHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "AccountInfo's ledger range must be available");

    auto const lgrInfoOrStatus = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, std::nullopt, input.vaultObj.ledgerIndex, range->maxSequence
    );

    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);

    // Extract the vault owner and construct a keylet for the vault object
    auto const accountStr = input.vaultObj.owner;
    auto const accountID = accountFromStringStrict(accountStr);
    auto const accountKeylet = ripple::keylet::account(*accountID);

    // Fetch the account ledger object
    auto const accountLedgerObject = sharedPtrBackend_->fetchLedgerObject(accountKeylet.key, lgrInfo.seq, ctx.yield);

    if (!accountLedgerObject)
        return Error{Status{RippledError::rpcACT_NOT_FOUND}};

    // use account to get vault ledger object
    ripple::STLedgerEntry const sle{
        ripple::SerialIter{accountLedgerObject->data(), accountLedgerObject->size()}, accountKeylet.key
    };

    auto const vaultKeylet = ripple::keylet::vault(*accountID, input.vaultObj.ledgerIndex);

    // Fetch the vault object
    auto const vaultLedgerObject = sharedPtrBackend_->fetchLedgerObject(vaultKeylet.key, lgrInfo.seq, ctx.yield);

    if (!vaultLedgerObject)
        return Error{Status{"entryNotFound"}};

    ripple::STLedgerEntry const vaultSle{
        ripple::SerialIter{vaultLedgerObject->data(), vaultLedgerObject->size()}, vaultKeylet.key
    };

    // Prepare output
    return Output(vaultSle, lgrInfo.seq);
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, VaultInfoHandler::Output const& output)
{
    jv = boost::json::object{
        {JS(ledger_index), output.ledgerIndex}, {JS(validated), output.validated}, {JS(vault), toJson(output.vault)}
    };
}

VaultInfoHandler::Input
tag_invoke(boost::json::value_to_tag<VaultInfoHandler::Input>, boost::json::value const& jv)
{
    auto input = VaultInfoHandler::Input{};
    auto const& jsonObject = jv.as_object();

    // vault guarentees to exist from spec
    auto const& vaultJson = jsonObject.at(JS(vault)).as_object();
    input.vaultObj = VaultInfoHandler::VaultInfoResponse{
        .owner = std::string{vaultJson.at(JS(owner)).as_string()},
        .ledgerIndex = static_cast<uint32_t>(vaultJson.at(JS(seq)).as_uint64())
    };
    return input;
}

}  // namespace rpc
