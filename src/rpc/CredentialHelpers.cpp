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
checkExpired(xrpl::SLE const& sleCred, xrpl::LedgerHeader const& ledger)
{
    if (sleCred.isFieldPresent(xrpl::sfExpiration)) {
        std::uint32_t const exp = sleCred.getFieldU32(xrpl::sfExpiration);
        std::uint32_t const now = ledger.parentCloseTime.time_since_epoch().count();
        return now > exp;
    }
    return false;
}

std::set<std::pair<xrpl::AccountID, xrpl::Slice>>
createAuthCredentials(xrpl::STArray const& in)
{
    std::set<std::pair<xrpl::AccountID, xrpl::Slice>> out;
    for (auto const& cred : in)
        out.insert({cred[xrpl::sfIssuer], cred[xrpl::sfCredentialType]});

    return out;
}

xrpl::STArray
parseAuthorizeCredentials(boost::json::array const& jv)
{
    xrpl::STArray arr;
    for (auto const& jo : jv) {
        ASSERT(
            jo.at(JS(issuer)).is_string(),
            "issuer must be string, should already be checked in AuthorizeCredentialValidator"
        );
        auto const issuer = xrpl::parseBase58<xrpl::AccountID>(
            static_cast<std::string>(jo.at(JS(issuer)).as_string())
        );
        ASSERT(
            issuer.has_value(),
            "issuer must be present, should already be checked in AuthorizeCredentialValidator."
        );

        ASSERT(
            jo.at(JS(credential_type)).is_string(),
            "credential_type must be string, should already be checked in "
            "AuthorizeCredentialValidator"
        );
        auto const credentialType =
            xrpl::strUnHex(static_cast<std::string>(jo.at(JS(credential_type)).as_string()));
        ASSERT(
            credentialType.has_value(),
            "credential_type must be present, should already be checked in "
            "AuthorizeCredentialValidator."
        );

        auto credential = xrpl::STObject::makeInnerObject(xrpl::sfCredential);

        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        credential.setAccountID(xrpl::sfIssuer, *issuer);
        credential.setFieldVL(xrpl::sfCredentialType, *credentialType);
        // NOLINTEND(bugprone-unchecked-optional-access)

        arr.push_back(std::move(credential));
    }

    return arr;
}

std::expected<xrpl::STArray, Status>
fetchCredentialArray(
    std::optional<boost::json::array> const& credID,
    xrpl::AccountID const& srcAcc,
    BackendInterface const& backend,
    xrpl::LedgerHeader const& info,
    boost::asio::yield_context const& yield
)
{
    xrpl::STArray authCreds;
    std::unordered_set<std::string_view> elems;
    for (auto const& elem : *credID) {  // NOLINT(bugprone-unchecked-optional-access)
        ASSERT(
            elem.is_string(), "should already be checked in validators.hpp that elem is a string."
        );

        if (elems.contains(elem.as_string()))
            return Error{Status{RippledError::RpcBadCredentials, "duplicates in credentials."}};
        elems.insert(elem.as_string());

        xrpl::uint256 credHash;
        ASSERT(
            credHash.parseHex(boost::json::value_to<std::string>(elem)),
            "should already be checked in validators.hpp that elem is a uint256 hex"
        );

        auto const credKeylet = xrpl::keylet::credential(credHash).key;
        auto const credLedgerObject = backend.fetchLedgerObject(credKeylet, info.seq, yield);
        if (!credLedgerObject)
            return Error{Status{RippledError::RpcBadCredentials, "credentials don't exist."}};

        auto credIt = xrpl::SerialIter{credLedgerObject->data(), credLedgerObject->size()};
        auto const sleCred = xrpl::SLE{credIt, credKeylet};

        if ((sleCred.getType() != xrpl::ltCREDENTIAL) ||
            ((sleCred.getFieldU32(xrpl::sfFlags) & xrpl::lsfAccepted) == 0u))
            return Error{Status{RippledError::RpcBadCredentials, "credentials aren't accepted"}};

        if (credentials::checkExpired(sleCred, info))
            return Error{Status{RippledError::RpcBadCredentials, "credentials are expired"}};

        if (sleCred.getAccountID(xrpl::sfSubject) != srcAcc) {
            return Error{Status{
                RippledError::RpcBadCredentials, "credentials don't belong to the root account"
            }};
        }

        auto credential = xrpl::STObject::makeInnerObject(xrpl::sfCredential);
        credential.setAccountID(xrpl::sfIssuer, sleCred.getAccountID(xrpl::sfIssuer));
        credential.setFieldVL(xrpl::sfCredentialType, sleCred.getFieldVL(xrpl::sfCredentialType));
        authCreds.push_back(std::move(credential));
    }

    return authCreds;
}

}  // namespace rpc::credentials
