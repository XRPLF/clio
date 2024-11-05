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

#include <xrpl/basics/Slice.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>

#include <expected>
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
 * @brief Returns a set of Account and CredentialID pairs. If there are any
 * duplicate CredentialID's, return empty set.
 *
 * @param in The array of Credential objects to check
 * @return Set of Issuer and CredentialType
 */
std::set<std::pair<ripple::AccountID, ripple::Slice>>
makeSorted(ripple::STArray const& in);

/**
 * @brief Creates authentication credential field (which is a set of pairs of AccountID and Credential ID)
 *
 * @param in The array of Credential objects to check
 * @return Auth Credential array or error Status
 */
std::expected<std::set<std::pair<ripple::AccountID, ripple::Slice>>, Status>
createAuthCredentials(ripple::STArray const& in);

}  // namespace rpc::credentials
