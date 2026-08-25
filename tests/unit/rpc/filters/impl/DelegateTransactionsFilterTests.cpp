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
    DelegateFilter const filterParams{DelegateFilter::Role::Authorizer};
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

    auto const result = filter.check(blob);
    EXPECT_FALSE(result.has_value());
}

TEST_F(DelegateTransactionFilterTest, RoleAuthorizer_MatchesWhenUserIsSigner)
{
    DelegateFilter const filterParams{DelegateFilter::Role::Authorizer};
    DelegateTransactionFilter const filter(filterParams, kAccountDelegator);

    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const result = filter.check(blob);
    ASSERT_TRUE(result.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(result->account, kAccountOwner);
}

TEST_F(DelegateTransactionFilterTest, RoleAuthorizer_FailsWhenUserIsNotSigner)
{
    DelegateFilter const filterParams{DelegateFilter::Role::Authorizer};
    DelegateTransactionFilter const filter(filterParams, kAccountDestination);

    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const result = filter.check(blob);
    EXPECT_FALSE(result.has_value());
}

TEST_F(DelegateTransactionFilterTest, RoleAuthorizer_WithCounterparty_Match)
{
    DelegateFilter const filterParams{DelegateFilter::Role::Authorizer, to_string(kAccountOwner)};
    DelegateTransactionFilter const filter(filterParams, kAccountDelegator);

    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const result = filter.check(blob);
    ASSERT_TRUE(result.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(result->account, kAccountOwner);
}

TEST_F(DelegateTransactionFilterTest, RoleAuthorizer_WithCounterparty_Mismatch)
{
    DelegateFilter const filterParams{
        DelegateFilter::Role::Authorizer, to_string(kAccountDestination)
    };
    DelegateTransactionFilter const filter(filterParams, kAccountDelegator);

    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const result = filter.check(blob);
    EXPECT_FALSE(result.has_value());
}

TEST_F(DelegateTransactionFilterTest, RoleActor_MatchesWhenUserIsOwner)
{
    DelegateFilter const filterParams{DelegateFilter::Role::Actor};
    DelegateTransactionFilter const filter(filterParams, kAccountOwner);

    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const result = filter.check(blob);
    ASSERT_TRUE(result.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(result->account, kAccountDelegator);
}

TEST_F(DelegateTransactionFilterTest, RoleActor_FailsWhenUserIsNotOwner)
{
    DelegateFilter const filterParams{DelegateFilter::Role::Actor};
    DelegateTransactionFilter const filter(filterParams, kAccountDestination);

    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const result = filter.check(blob);
    EXPECT_FALSE(result.has_value());
}

TEST_F(DelegateTransactionFilterTest, RoleActor_WithCounterparty_Match)
{
    DelegateFilter const filterParams{DelegateFilter::Role::Actor, to_string(kAccountDelegator)};
    DelegateTransactionFilter const filter(filterParams, kAccountOwner);

    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const result = filter.check(blob);
    ASSERT_TRUE(result.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(result->account, kAccountDelegator);
}

TEST_F(DelegateTransactionFilterTest, RoleActor_WithCounterparty_Mismatch)
{
    DelegateFilter const filterParams{DelegateFilter::Role::Actor, to_string(kAccountDestination)};
    DelegateTransactionFilter const filter(filterParams, kAccountOwner);

    auto blob = createBlob(to_string(kAccountOwner), to_string(kAccountDelegator));

    auto const result = filter.check(blob);
    EXPECT_FALSE(result.has_value());
}
