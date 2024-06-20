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

#include "rpc/JS.hpp"
#include "util/LedgerUtils.hpp"

#include <gtest/gtest.h>
#include <ripple/protocol/LedgerFormats.h>
#include <ripple/protocol/jss.h>

#include <algorithm>
#include <iterator>

TEST(LedgerUtilsTests, LedgerObjectTypeList)
{
    auto const& types = util::getLedgerEntryTypeStrs();
    static char const* typesList[] = {
        JS(account),
        JS(amendments),
        JS(check),
        JS(deposit_preauth),
        JS(directory),
        JS(escrow),
        JS(fee),
        JS(hashes),
        JS(offer),
        JS(payment_channel),
        JS(signer_list),
        JS(state),
        JS(ticket),
        JS(nft_offer),
        JS(nft_page),
        JS(amm),
        JS(bridge),
        JS(xchain_owned_claim_id),
        JS(xchain_owned_create_account_claim_id),
        JS(did),
        JS(oracle),
        JS(nunl)
    };
    ASSERT_TRUE(std::size(typesList) == types.size());
    EXPECT_TRUE(std::all_of(std::cbegin(typesList), std::cend(typesList), [&types](auto const& type) {
        return std::find(std::cbegin(types), std::cend(types), type) != std::cend(types);
    }));
}

TEST(LedgerUtilsTests, StrToType)
{
    EXPECT_EQ(util::getLedgerEntryTypeFromStr("mess"), ripple::ltANY);
    EXPECT_EQ(util::getLedgerEntryTypeFromStr("tomato"), ripple::ltANY);
    EXPECT_EQ(util::getLedgerEntryTypeFromStr("account"), ripple::ltACCOUNT_ROOT);

    auto const& types = util::getLedgerEntryTypeStrs();
    std::for_each(types.cbegin(), types.cend(), [](auto const& typeStr) {
        EXPECT_NE(util::getLedgerEntryTypeFromStr(typeStr), ripple::ltANY);
    });
}

TEST(LedgerUtilsTests, DeletionBlockerTypes)
{
    auto const& testedTypes = util::getDeletionBlockerLedgerTypes();

    static ripple::LedgerEntryType constexpr deletionBlockers[] = {
        ripple::ltCHECK,
        ripple::ltESCROW,
        ripple::ltNFTOKEN_PAGE,
        ripple::ltPAYCHAN,
        ripple::ltRIPPLE_STATE,
        ripple::ltXCHAIN_OWNED_CLAIM_ID,
        ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID,
        ripple::ltBRIDGE
    };

    ASSERT_TRUE(std::size(deletionBlockers) == testedTypes.size());
    EXPECT_TRUE(std::any_of(testedTypes.cbegin(), testedTypes.cend(), [](auto const& type) {
        return std::find(std::cbegin(deletionBlockers), std::cend(deletionBlockers), type) !=
            std::cend(deletionBlockers);
    }));
}
