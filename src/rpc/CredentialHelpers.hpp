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
checkExpired(ripple::SLE const& sleCred, ripple::LedgerHeader const& ledger);

/**
 * @brief Creates authentication credential field (which is a set of pairs of AccountID and Credential ID)
 *
 * @param in The array of Credential objects to check
 * @return Auth Credential array
 */
std::set<std::pair<ripple::AccountID, ripple::Slice>>
createAuthCredentials(ripple::STArray const& in);

/**
 * @brief Parses each credential object and makes sure the credential type and values are correct
 *
 * @param jv The boost json array of credentials to parse
 * @return Array of credentials after parsing
 */
ripple::STArray
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
std::expected<ripple::STArray, Status>
fetchCredentialArray(
    std::optional<boost::json::array> const& credID,
    ripple::AccountID const& srcAcc,
    BackendInterface const& backend,
    ripple::LedgerHeader const& info,
    boost::asio::yield_context const& yield
);

}  // namespace rpc::credentials
