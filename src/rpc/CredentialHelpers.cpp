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

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STArray.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <expected>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
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

std::set<std::pair<ripple::AccountID, ripple::Slice>>
createAuthCredentials(ripple::STArray const& in)
{
    std::set<std::pair<ripple::AccountID, ripple::Slice>> out;
    for (auto const& cred : in)
        out.insert({cred[ripple::sfIssuer], cred[ripple::sfCredentialType]});

    return out;
}

ripple::STArray
parseAuthorizeCredentials(boost::json::array const& jv)
{
    ripple::STArray arr;
    for (auto const& jo : jv) {
        ASSERT(
            jo.at(JS(issuer)).is_string(),
            "issuer must be string, should already be checked in AuthorizeCredentialValidator"
        );
        auto const issuer =
            ripple::parseBase58<ripple::AccountID>(static_cast<std::string>(jo.at(JS(issuer)).as_string()));
        ASSERT(
            issuer.has_value(), "issuer must be present, should already be checked in AuthorizeCredentialValidator."
        );

        ASSERT(
            jo.at(JS(credential_type)).is_string(),
            "credential_type must be string, should already be checked in AuthorizeCredentialValidator"
        );
        auto const credentialType = ripple::strUnHex(static_cast<std::string>(jo.at(JS(credential_type)).as_string()));
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

std::expected<ripple::STArray, Status>
fetchCredentialArray(
    std::optional<boost::json::array> const& credID,
    ripple::AccountID const& srcAcc,
    BackendInterface const& backend,
    ripple::LedgerHeader const& info,
    boost::asio::yield_context const& yield
)
{
    ripple::STArray authCreds;
    std::unordered_set<std::string_view> elems;
    for (auto const& elem : credID.value()) {
        ASSERT(elem.is_string(), "should already be checked in validators.hpp that elem is a string.");

        if (elems.contains(elem.as_string()))
            return Error{Status{RippledError::rpcBAD_CREDENTIALS, "duplicates in credentials."}};
        elems.insert(elem.as_string());

        ripple::uint256 credHash;
        ASSERT(
            credHash.parseHex(boost::json::value_to<std::string>(elem)),
            "should already be checked in validators.hpp that elem is a uint256 hex"
        );

        auto const credKeylet = ripple::keylet::credential(credHash).key;
        auto const credLedgerObject = backend.fetchLedgerObject(credKeylet, info.seq, yield);
        if (!credLedgerObject)
            return Error{Status{RippledError::rpcBAD_CREDENTIALS, "credentials don't exist."}};

        auto credIt = ripple::SerialIter{credLedgerObject->data(), credLedgerObject->size()};
        auto const sleCred = ripple::SLE{credIt, credKeylet};

        if ((sleCred.getType() != ripple::ltCREDENTIAL) ||
            ((sleCred.getFieldU32(ripple::sfFlags) & ripple::lsfAccepted) == 0u))
            return Error{Status{RippledError::rpcBAD_CREDENTIALS, "credentials aren't accepted"}};

        if (credentials::checkExpired(sleCred, info))
            return Error{Status{RippledError::rpcBAD_CREDENTIALS, "credentials are expired"}};

        if (sleCred.getAccountID(ripple::sfSubject) != srcAcc)
            return Error{Status{RippledError::rpcBAD_CREDENTIALS, "credentials don't belong to the root account"}};

        auto credential = ripple::STObject::makeInnerObject(ripple::sfCredential);
        credential.setAccountID(ripple::sfIssuer, sleCred.getAccountID(ripple::sfIssuer));
        credential.setFieldVL(ripple::sfCredentialType, sleCred.getFieldVL(ripple::sfCredentialType));
        authCreds.push_back(std::move(credential));
    }

    return authCreds;
}

}  // namespace rpc::credentials
