#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>

#include <expected>
#include <optional>
#include <set>
#include <utility>

namespace rpc::credentials {

/**
 * @brief Check if credential is expired
 *
 * @param sleCred The credential to check
 * @param ledger The ledger to check the closed time of
 * @return true if credential not expired, false otherwise
 */
bool
checkExpired(xrpl::SLE const& sleCred, xrpl::LedgerHeader const& ledger);

/**
 * @brief Creates authentication credential field (which is a set of pairs of AccountID and
 * Credential ID)
 *
 * @param in The array of Credential objects to check
 * @return Auth Credential array
 */
std::set<std::pair<xrpl::AccountID, xrpl::Slice>>
createAuthCredentials(xrpl::STArray const& in);

/**
 * @brief Parses each credential object and makes sure the credential type and values are correct
 *
 * @param jv The boost json array of credentials to parse
 * @return Array of credentials after parsing
 */
xrpl::STArray
parseAuthorizeCredentials(boost::json::array const& jv);

/**
 * @brief Get Array of Credential objects
 *
 * @param credID Array of CredentialID's to parse
 * @param srcAcc The Source Account
 * @param backend backend interface
 * @param info The ledger header
 * @param yield The coroutine context
 * @return Array of credential objects, error if failed otherwise
 */
std::expected<xrpl::STArray, Status>
fetchCredentialArray(
    std::optional<boost::json::array> const& credID,
    xrpl::AccountID const& srcAcc,
    BackendInterface const& backend,
    xrpl::LedgerHeader const& info,
    boost::asio::yield_context const& yield
);

}  // namespace rpc::credentials
