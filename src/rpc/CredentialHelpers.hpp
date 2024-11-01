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

#include <xrpl/basics/Slice.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>

#include <set>
#include <utility>

namespace rpc {

/** @brief Check if credential is expired
 *
 * @param sleCred The credential to check
 * @param closed The time that the ledger was closed
 * @return true if credential not expired, false otherwise
 */
bool
checkExpired(ripple::SLE const& sleCred, ripple::NetClock::time_point const& closed);

/** @brief Check if if there are duplicate Account and CredentialID pairs
 *
 * @param in The array of Credential objects to check
 * @return Set of Issuer and CredentialType
 */
std::set<std::pair<ripple::AccountID, ripple::Slice>>
makeSorted(ripple::STArray const& in);

}  // namespace rpc
