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

#include "data/Types.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/filters/impl/DelegateTransactionsFilter.hpp"
#include "util/TestObject.hpp"

#include <gtest/gtest.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>

#include <optional>
#include <string_view>
#include <utility>

using namespace rpc;
using namespace xrpl;

namespace {
auto const kAccountOwner = *parseBase58<AccountID>("rnrx6w8Z2VJERMMpk9jv9Y2YZKTekFAZaK");
auto const kAccountDelegator = *parseBase58<AccountID>("rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh");
auto const kAccountDestination = *parseBase58<AccountID>("rMAXACCrp3Y8PpswXcg3bKggHX76V3F8M4");
auto constexpr kMaxSeq = 30u;
}  // namespace

class DelegateTransactionFilterTest : public ::testing::Test {
protected:
    static data::TransactionAndMetadata
    createBlob(std::string_view owner, std::string_view delegate)
    {
        data::TransactionAndMetadata ret;
        ret.transaction = createDelegateBlob(owner, delegate);
        ret.ledgerSequence = kMaxSeq;
        return ret;
    }
};

TEST_F(DelegateTransactionFilterTest, ReturnsFalseIfNoDelegateField)
{
    DelegateFilter const filterParams{
        .delegateType = DelegateFilter::Role::Authorizer, .counterParty = std::nullopt
    };
    DelegateTransactionFilter const filter(filterParams, kAccountOwner);

    // Create standard tx (no delegate field) using standard TestObject helper
    auto obj = createPaymentTransactionObject(
        to_string(kAccountOwner), to_string(kAccountDestination), 100, 10, 1
    );

    STTx const tx(std::move(obj));
    Serializer s;
    tx.add(s);

    data::TransactionAndMetadata blob;
    blob.transaction = s.getData();

    auto const& result = filter.check(blob);
    EXPECT_FALSE(result.shouldInclude);
}

TEST_F(DelegateTransactionFilterTest, RoleAuthorizer_MatchesWhenUserIsSigner)
{
    // I am Account B (the actor/signer). I want to see transactions where I acted as authorizer's
    // delegate (signed for someone).
    DelegateFilter const filterParams{
        .delegateType = DelegateFilter::Role::Authorizer, .counterParty = std::nullopt
    };
    DelegateTransactionFilter const filter(filterParams, kAccountDelegator);

    // Tx: Owner/authorizer=A, Signer/actor=B
    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const& result = filter.check(blob);
    EXPECT_TRUE(result.shouldInclude);
    ASSERT_TRUE(result.relevantAccount.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(*result.relevantAccount, kAccountOwner);
}

TEST_F(DelegateTransactionFilterTest, RoleAuthorizer_FailsWhenUserIsNotSigner)
{
    // I am Account C. I query for authorizer work.
    DelegateFilter const filterParams{
        .delegateType = DelegateFilter::Role::Authorizer, .counterParty = std::nullopt
    };
    DelegateTransactionFilter const filter(filterParams, kAccountDestination);

    // Tx: Owner/authorizer=A, Signer/actor=B (C is not involved)
    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const& result = filter.check(blob);
    EXPECT_FALSE(result.shouldInclude);
}

TEST_F(DelegateTransactionFilterTest, RoleAuthorizer_WithCounterparty_Match)
{
    // I am Account B (Signer). I want to see work I did specifically for Account A.
    DelegateFilter const filterParams{
        .delegateType = DelegateFilter::Role::Authorizer, .counterParty = to_string(kAccountOwner)
    };
    DelegateTransactionFilter const filter(filterParams, kAccountDelegator);

    // Tx: Owner/authorizer=A, Signer/actor=B
    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const& result = filter.check(blob);
    EXPECT_TRUE(result.shouldInclude);
    ASSERT_TRUE(result.relevantAccount.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(*result.relevantAccount, kAccountOwner);
}

TEST_F(DelegateTransactionFilterTest, RoleAuthorizer_WithCounterparty_Mismatch)
{
    // I am Account B (Signer). I want to see work I did for Account C.
    DelegateFilter const filterParams{
        .delegateType = DelegateFilter::Role::Authorizer,
        .counterParty = to_string(kAccountDestination)
    };
    DelegateTransactionFilter const filter(filterParams, kAccountDelegator);

    // Tx: Owner/authorizer=A, Signer/actor=B (Owner A != Counterparty C)
    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const& result = filter.check(blob);
    EXPECT_FALSE(result.shouldInclude);
}

TEST_F(DelegateTransactionFilterTest, RoleActor_MatchesWhenUserIsOwner)
{
    // I am Account A (Owner). I want to see who signed for me.
    DelegateFilter const filterParams{
        .delegateType = DelegateFilter::Role::Actor, .counterParty = std::nullopt
    };
    DelegateTransactionFilter const filter(filterParams, kAccountOwner);

    // Tx: Owner/authorizer=A, Signer/actor=B
    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const& result = filter.check(blob);
    EXPECT_TRUE(result.shouldInclude);
    ASSERT_TRUE(result.relevantAccount.has_value());
    // Should return Signer.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(*result.relevantAccount, kAccountDelegator);
}

TEST_F(DelegateTransactionFilterTest, RoleActor_FailsWhenUserIsNotOwner)
{
    // I am Account C. I query for actor work.
    DelegateFilter const filterParams{
        .delegateType = DelegateFilter::Role::Actor, .counterParty = std::nullopt
    };
    DelegateTransactionFilter const filter(filterParams, kAccountDestination);

    // Tx: Owner/authorizer=A, Signer/actor=B (C is not involved)
    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const& result = filter.check(blob);
    EXPECT_FALSE(result.shouldInclude);
}

TEST_F(DelegateTransactionFilterTest, RoleActor_WithCounterparty_Match)
{
    // I am Account A (Owner). I want to see work signed specifically by B.
    DelegateFilter const filterParams{
        .delegateType = DelegateFilter::Role::Actor, .counterParty = to_string(kAccountDelegator)
    };
    DelegateTransactionFilter const filter(filterParams, kAccountOwner);

    // Tx: Owner/authorizer=A, Signer/actor=B
    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const& result = filter.check(blob);
    EXPECT_TRUE(result.shouldInclude);
    ASSERT_TRUE(result.relevantAccount.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(*result.relevantAccount, kAccountDelegator);
}

TEST_F(DelegateTransactionFilterTest, RoleActor_WithCounterparty_Mismatch)
{
    // I am Account A (Owner). I want to see work signed by C.
    DelegateFilter const filterParams{
        .delegateType = DelegateFilter::Role::Actor, .counterParty = to_string(kAccountDestination)
    };
    DelegateTransactionFilter const filter(filterParams, kAccountOwner);

    // Tx: Owner/authorizer=A, Signer/actor=B (Signer B != Counterparty C)
    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const& result = filter.check(blob);
    EXPECT_FALSE(result.shouldInclude);
}
