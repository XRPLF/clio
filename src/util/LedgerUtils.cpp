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

#include "util/LedgerUtils.hpp"

#include "rpc/JS.hpp"

#include <ripple/protocol/LedgerFormats.h>
#include <ripple/protocol/jss.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace util {
namespace impl {

struct LedgerTypeAttributes {
    ripple::LedgerEntryType type = ripple::ltANY;
    bool deletionBlocker = false;

    LedgerTypeAttributes(ripple::LedgerEntryType type, bool blocker = false) : type(type), deletionBlocker(blocker)
    {
    }
};

// Ledger entry type filter list, add new types here to support filtering for ledger_data and
// account_objects
static std::unordered_map<std::string, LedgerTypeAttributes> const LEDGER_TYPES_MAP{{
    {JS(account), LedgerTypeAttributes(ripple::ltACCOUNT_ROOT)},
    {JS(amendments), LedgerTypeAttributes(ripple::ltAMENDMENTS)},
    {JS(check), LedgerTypeAttributes(ripple::ltCHECK, true)},
    {JS(deposit_preauth), LedgerTypeAttributes(ripple::ltDEPOSIT_PREAUTH)},
    {JS(directory), LedgerTypeAttributes(ripple::ltDIR_NODE)},
    {JS(escrow), LedgerTypeAttributes(ripple::ltESCROW, true)},
    {JS(fee), LedgerTypeAttributes(ripple::ltFEE_SETTINGS)},
    {JS(hashes), LedgerTypeAttributes(ripple::ltLEDGER_HASHES)},
    {JS(offer), LedgerTypeAttributes(ripple::ltOFFER)},
    {JS(payment_channel), LedgerTypeAttributes(ripple::ltPAYCHAN, true)},
    {JS(signer_list), LedgerTypeAttributes(ripple::ltSIGNER_LIST)},
    {JS(state), LedgerTypeAttributes(ripple::ltRIPPLE_STATE, true)},
    {JS(ticket), LedgerTypeAttributes(ripple::ltTICKET)},
    {JS(nft_offer), LedgerTypeAttributes(ripple::ltNFTOKEN_OFFER)},
    {JS(nft_page), LedgerTypeAttributes(ripple::ltNFTOKEN_PAGE, true)},
    {JS(amm), LedgerTypeAttributes(ripple::ltAMM)},
    {JS(bridge), LedgerTypeAttributes(ripple::ltBRIDGE, true)},
    {JS(xchain_owned_claim_id), LedgerTypeAttributes(ripple::ltXCHAIN_OWNED_CLAIM_ID, true)},
    {JS(xchain_owned_create_account_claim_id),
     LedgerTypeAttributes(ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID, true)},
    {JS(did), LedgerTypeAttributes(ripple::ltDID)},
    {JS(oracle), LedgerTypeAttributes(ripple::ltORACLE)},
    {JS(nunl), LedgerTypeAttributes(ripple::ltNEGATIVE_UNL)},
}};
}  // namespace impl

std::unordered_set<std::string> const&
getLedgerEntryTypeStrs()
{
    static std::unordered_set<std::string> const typesKeys = []() {
        std::unordered_set<std::string> keys;
        std::transform(
            impl::LEDGER_TYPES_MAP.begin(),
            impl::LEDGER_TYPES_MAP.end(),
            std::inserter(keys, keys.begin()),
            [](auto const& item) { return item.first; }
        );
        return keys;
    }();

    return typesKeys;
}

ripple::LedgerEntryType
getLedgerEntryTypeFromStr(std::string const& entryName)
{
    if (impl::LEDGER_TYPES_MAP.find(entryName) == impl::LEDGER_TYPES_MAP.end())
        return ripple::ltANY;

    return impl::LEDGER_TYPES_MAP.at(entryName).type;
}

std::vector<ripple::LedgerEntryType> const&
getDeletionBlockerLedgerTypes()
{
    static std::vector<ripple::LedgerEntryType> const deletionBlockerLedgerTypes = []() {
        // TODO: Move to std::ranges::views::filter when move to higher clang
        auto ret = std::vector<ripple::LedgerEntryType>{};
        std::for_each(impl::LEDGER_TYPES_MAP.cbegin(), impl::LEDGER_TYPES_MAP.cend(), [&ret](auto const& item) {
            if (item.second.deletionBlocker)
                ret.push_back(item.second.type);
        });
        return ret;
    }();

    return deletionBlockerLedgerTypes;
}

}  // namespace util
