//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "util/Assert.hpp"

#include <boost/json/array.hpp>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STArray.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <expected>
#include <set>
#include <string>
#include <utility>

namespace rpc::credentials {

bool
checkExpired(ripple::SLE const& sleCred, ripple::LedgerHeader const& ledger)
{
    if (sleCred.isFieldPresent(ripple::sfExpiration)) {
        std::uint32_t const exp = sleCred.getFieldU32(ripple::sfExpiration);
        std::uint32_t const now = ledger.parentCloseTime.time_since_epoch().count();
        return now > exp;
    }
    return false;
}

std::expected<std::set<std::pair<ripple::AccountID, ripple::Slice>>, Status>
createAuthCredentials(ripple::STArray const& in)
{
    std::set<std::pair<ripple::AccountID, ripple::Slice>> out;
    for (auto const& cred : in) {
        auto [it, ins] = out.insert({cred[ripple::sfIssuer], cred[ripple::sfCredentialType]});
        if (!ins)
            return std::unexpected{Status{RippledError::rpcBAD_CREDENTIALS, "duplicates in credentials."}};
    }
    return out;
}

ripple::STArray
parseAuthorizeCredentials(boost::json::array const& jv)
{
    ripple::STArray arr;
    for (auto const& jo : jv) {
        auto const issuer = ripple::parseBase58<ripple::AccountID>(
            static_cast<std::string>(jo.at(ripple::jss::issuer.c_str()).as_string())
        );
        ASSERT(
            issuer.has_value(), "issuer must be present, should already be checked in AuthorizeCredentialValidator."
        );

        auto const credentialType =
            ripple::strUnHex(static_cast<std::string>(jo.at(ripple::jss::credential_type.c_str()).as_string()));

        ASSERT(
            credentialType.has_value(),
            "credential_type must be present, should already be checked in AuthorizeCredentialValidator."
        );

        auto credential = ripple::STObject::makeInnerObject(ripple::sfCredential);
        credential.setAccountID(ripple::sfIssuer, *issuer);
        credential.setFieldVL(ripple::sfCredentialType, *credentialType);
        arr.push_back(std::move(credential));
    }

    return arr;
}

}  // namespace rpc::credentials
