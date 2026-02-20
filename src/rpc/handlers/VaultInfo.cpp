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
#include "util/JsonUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Keylet.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STBase.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace rpc {

namespace {

/**
 * @brief Ensures that the input contains either a `vaultID` alone, or both `owner` and
 * `tnxSequence`. Any other combination is considered malformed.
 *
 * @param input The input object containing optional fields for the vault request.
 * @return Returns true if the input is valid, false otherwise.
 */
bool
validate(VaultInfoHandler::Input const& input)
{
    bool const hasVaultId = input.vaultID.has_value();
    bool const hasOwner = input.owner.has_value();
    bool const hasSeq = input.tnxSequence.has_value();

    // Only valid combinations: (vaultID) or (owner + ledgerIndex)
    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    return (hasVaultId && !hasOwner && !hasSeq) || (!hasVaultId && hasOwner && hasSeq);
}

}  // namespace

VaultInfoHandler::VaultInfoHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend)
    : sharedPtrBackend_{sharedPtrBackend}
{
}

VaultInfoHandler::Result
VaultInfoHandler::process(VaultInfoHandler::Input const& input, Context const& ctx) const
{
    // vault info input must either have owner and sequence, or vault_id only.
    if (not validate(input))
        return Error{ClioError::RpcMalformedRequest};

    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "VaultInfo's ledger range must be available");

    auto const expectedLgrInfo = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, std::nullopt, input.ledgerIndex, range->maxSequence
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;

    // Extract the vault keylet based on input
    auto const vaultKeylet = [&]() -> std::expected<ripple::Keylet, Status> {
        if (input.owner && input.tnxSequence) {
            auto const accountStr = *input.owner;
            auto const accountID = accountFromStringStrict(accountStr);

            // checks that account exists
            {
                auto const accountKeylet = ripple::keylet::account(*accountID);
                auto const accountLedgerObject =
                    sharedPtrBackend_->fetchLedgerObject(accountKeylet.key, lgrInfo.seq, ctx.yield);

                if (!accountLedgerObject)
                    return std::unexpected{Status{RippledError::rpcENTRY_NOT_FOUND}};
            }

            return ripple::keylet::vault(*accountID, *input.tnxSequence);
        }
        ripple::uint256 nodeIndex;
        if (nodeIndex.parseHex(*input.vaultID))
            return ripple::keylet::vault(nodeIndex);

        return std::unexpected{Status{RippledError::rpcENTRY_NOT_FOUND}};
    }();

    if (not vaultKeylet.has_value())
        return Error{vaultKeylet.error()};

    // Fetch the vault object and it's associated issuance ID
    auto const vaultLedgerObject =
        sharedPtrBackend_->fetchLedgerObject(vaultKeylet.value().key, lgrInfo.seq, ctx.yield);

    if (not vaultLedgerObject)
        return Error{Status{RippledError::rpcENTRY_NOT_FOUND, "vault object not found."}};

    ripple::STLedgerEntry const vaultSle{
        ripple::SerialIter{vaultLedgerObject->data(), vaultLedgerObject->size()},
        vaultKeylet.value().key
    };

    auto const issuanceKeylet = ripple::keylet::mptIssuance(vaultSle[ripple::sfShareMPTID]).key;
    auto const issuanceObject =
        sharedPtrBackend_->fetchLedgerObject(issuanceKeylet, lgrInfo.seq, ctx.yield);

    if (not issuanceObject)
        return Error{Status{RippledError::rpcENTRY_NOT_FOUND, "issuance object not found."}};

    ripple::STLedgerEntry const issuanceSle{
        ripple::SerialIter{issuanceObject->data(), issuanceObject->size()}, issuanceKeylet
    };

    // put issuance object into "shares" field of vault object
    // follows same logic as rippled:
    // https://github.com/XRPLF/rippled/pull/5224/files#diff-6cb544622c7942261f097d628f61f1c1fcf34a1bcfd954aedbada4238fc28f69R107
    Output response;
    response.vault = toBoostJson(vaultSle.getJson(ripple::JsonOptions::none));
    response.vault.as_object()[JS(shares)] =
        toBoostJson(issuanceSle.getJson(ripple::JsonOptions::none));
    response.ledgerIndex = lgrInfo.seq;

    return response;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    VaultInfoHandler::Output const& output
)
{
    jv = boost::json::object{
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(vault), output.vault}
    };
}

VaultInfoHandler::Input
tag_invoke(boost::json::value_to_tag<VaultInfoHandler::Input>, boost::json::value const& jv)
{
    auto input = VaultInfoHandler::Input{};
    auto const& jsonObject = jv.as_object();

    if (jsonObject.contains(JS(owner)))
        input.owner = jsonObject.at(JS(owner)).as_string();

    if (jsonObject.contains(JS(seq)))
        input.tnxSequence = util::integralValueAs<uint32_t>(jsonObject.at(JS(seq)));

    if (jsonObject.contains(JS(vault_id)))
        input.vaultID = jsonObject.at(JS(vault_id)).as_string();

    if (jsonObject.contains(JS(ledger_index))) {
        auto const expectedLedgerIndex = util::getLedgerIndex(jsonObject.at(JS(ledger_index)));
        if (expectedLedgerIndex.has_value())
            input.ledgerIndex = *expectedLedgerIndex;
    }

    return input;
}

}  // namespace rpc
