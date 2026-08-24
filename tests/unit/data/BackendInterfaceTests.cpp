#include "data/BackendInterface.hpp"
#include "etl/CorruptionDetector.hpp"
#include "etl/SystemState.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/Retry.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/post.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/XRPAmount.h>

#include <chrono>
#include <cstddef>
#include <exception>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

using namespace data;
using namespace util::prometheus;
using namespace testing;

namespace {

constexpr auto kMaxSeq = 30;
constexpr auto kMinSeq = 10;

}  // namespace

struct BackendInterfaceTest : WithPrometheus, MockBackendTestNaggy, SyncAsioContextTest {
    BackendInterfaceTest()
    {
        backend_->setRange(kMinSeq, kMaxSeq);
    }
};

TEST_F(BackendInterfaceTest, FetchFeesSuccessPath)
{
    using namespace xrpl;

    // New fee setting (after XRPFees amendment)
    EXPECT_CALL(*backend_, doFetchLedgerObject(keylet::feeSettings().key, kMaxSeq, _))
        .WillRepeatedly(Return(createFeeSettingBlob(XRPAmount(1), XRPAmount(2), XRPAmount(3), 0)));

    runSpawn([this](auto yield) {
        auto fees = backend_->fetchFees(kMaxSeq, yield);

        EXPECT_TRUE(fees.has_value());
        EXPECT_EQ(fees->base, XRPAmount(1));
        EXPECT_EQ(fees->increment, XRPAmount(2));
        EXPECT_EQ(fees->reserve, XRPAmount(3));
    });
}

TEST_F(BackendInterfaceTest, FetchFeesLegacySuccessPath)
{
    using namespace xrpl;

    // Legacy fee setting (before XRPFees amendment)
    EXPECT_CALL(*backend_, doFetchLedgerObject(keylet::feeSettings().key, kMaxSeq, _))
        .WillRepeatedly(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    runSpawn([this](auto yield) {
        auto fees = backend_->fetchFees(kMaxSeq, yield);

        EXPECT_TRUE(fees.has_value());
        EXPECT_EQ(fees->base, XRPAmount(1));
        EXPECT_EQ(fees->increment, XRPAmount(2));
        EXPECT_EQ(fees->reserve, XRPAmount(3));
    });
}

TEST_F(BackendInterfaceTest, FetchLedgerPageSuccessPath)
{
    using namespace xrpl;

    auto state = etl::SystemState{};
    backend_->setCorruptionDetector(etl::CorruptionDetector{state, backend_->cache()});

    EXPECT_FALSE(backend_->cache().isDisabled());
    EXPECT_CALL(*backend_, doFetchSuccessorKey(_, _, _))
        .Times(10)
        .WillRepeatedly(
            Return(uint256{"1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"})
        );
    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, _, _))
        .WillOnce(Return(std::vector<Blob>(10, Blob{'s'})));

    runSpawn([this](auto yield) {
        backend_->fetchLedgerPage(std::nullopt, kMaxSeq, 10, false, yield);
    });
    EXPECT_FALSE(backend_->cache().isDisabled());
}

TEST_F(BackendInterfaceTest, FetchLedgerPageDisablesCacheOnMissingData)
{
    using namespace xrpl;

    auto state = etl::SystemState{};
    backend_->setCorruptionDetector(etl::CorruptionDetector{state, backend_->cache()});

    EXPECT_FALSE(backend_->cache().isDisabled());
    EXPECT_CALL(*backend_, doFetchSuccessorKey(_, _, _))
        .Times(10)
        .WillRepeatedly(
            Return(uint256{"1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"})
        );
    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, _, _))
        .WillOnce(Return(
            std::vector<Blob>{
                Blob{'s'},
                Blob{'s'},
                Blob{'s'},
                Blob{'s'},
                Blob{'s'},
                Blob{'s'},
                Blob{'s'},
                Blob{'s'},
                Blob{'s'},
                Blob{}
            }
        ));

    runSpawn([this](auto yield) {
        backend_->fetchLedgerPage(std::nullopt, kMaxSeq, 10, false, yield);
    });
    EXPECT_TRUE(backend_->cache().isDisabled());
}

TEST_F(
    BackendInterfaceTest,
    FetchLedgerPageWithoutCorruptionDetectorDoesNotDisableCacheOnMissingData
)
{
    using namespace xrpl;

    EXPECT_FALSE(backend_->cache().isDisabled());
    EXPECT_CALL(*backend_, doFetchSuccessorKey(_, _, _))
        .Times(10)
        .WillRepeatedly(
            Return(uint256{"1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"})
        );
    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, _, _))
        .WillOnce(Return(
            std::vector<Blob>{
                Blob{'s'},
                Blob{'s'},
                Blob{'s'},
                Blob{'s'},
                Blob{'s'},
                Blob{'s'},
                Blob{'s'},
                Blob{'s'},
                Blob{'s'},
                Blob{}
            }
        ));

    runSpawn([this](auto yield) {
        backend_->fetchLedgerPage(std::nullopt, kMaxSeq, 10, false, yield);
    });
    EXPECT_FALSE(backend_->cache().isDisabled());
}

// Loader and Extractor catch std::runtime_error to decide the server must amendment-block, so
// DatabaseError has to stay outside that hierarchy.
TEST(BackendInterfaceRetryTest, DatabaseErrorIsNotARuntimeError)
{
    static_assert(std::is_base_of_v<std::exception, DatabaseError>);
    static_assert(not std::is_base_of_v<std::runtime_error, DatabaseError>);

    try {
        throw DatabaseError{"transient"};
    } catch (std::runtime_error const&) {
        FAIL() << "DatabaseError must not be caught as std::runtime_error - doing so would "
                  "amendment-block the server on a transient database error";
    } catch (std::exception const& e) {
        EXPECT_STREQ(e.what(), "transient");
    }
}

TEST(BackendInterfaceRetryTest, DatabaseErrorKeepsDefaultMessage)
{
    EXPECT_STREQ(DatabaseError{}.what(), "Transient database error. Please retry the request");
}

TEST(BackendInterfaceRetryTest, RetryOnTimeoutBlockingRetriesUntilSuccess)
{
    std::size_t calls = 0;

    auto const result = retryOnTimeout(
        [&calls]() -> int {
            if (++calls < 3)
                throw DatabaseError{};
            return 42;
        },
        util::Retry::Delays{
            .initial = std::chrono::milliseconds{1}, .max = std::chrono::milliseconds{2}
        }
    );

    EXPECT_EQ(result, 42);
    EXPECT_EQ(calls, 3);
}

TEST(BackendInterfaceRetryTest, RetryOnTimeoutBlockingDoesNotSwallowOtherExceptions)
{
    EXPECT_THROW(
        retryOnTimeout(
            []() -> int { throw std::runtime_error{"permanent"}; },
            util::Retry::Delays{
                .initial = std::chrono::milliseconds{1}, .max = std::chrono::milliseconds{1}
            }
        ),
        std::runtime_error
    );
}

struct BackendInterfaceRetryCoroTest : SyncAsioContextTest {};

TEST_F(BackendInterfaceRetryCoroTest, RetryOnTimeoutCoroRetriesUntilSuccess)
{
    std::size_t calls = 0;

    runSpawn([&calls](auto yield) {
        auto const result = retryOnTimeout(
            [&calls]() -> int {
                if (++calls < 3)
                    throw DatabaseError{};
                return 42;
            },
            yield,
            util::Retry::Delays{
                .initial = std::chrono::milliseconds{1}, .max = std::chrono::milliseconds{2}
            }
        );

        EXPECT_EQ(result, 42);
    });

    EXPECT_EQ(calls, 3);
}

TEST_F(BackendInterfaceRetryCoroTest, RetryOnTimeoutCoroDoesNotSwallowOtherExceptions)
{
    runSpawn([](auto yield) {
        EXPECT_THROW(
            retryOnTimeout(
                []() -> int { throw std::runtime_error{"permanent"}; },
                yield,
                util::Retry::Delays{
                    .initial = std::chrono::milliseconds{1}, .max = std::chrono::milliseconds{1}
                }
            ),
            std::runtime_error
        );
    });
}

// An exception left in flight across a suspension point is visible to whatever else runs on that
// thread meanwhile, and is corrupted when the handlers unwind out of order or on a different thread
// of the pool. We prevent this.
TEST_F(BackendInterfaceRetryCoroTest, RetryOnTimeoutCoroDoesNotWaitInsideCatchHandler)
{
    std::optional<bool> exceptionInFlightDuringWait;

    runSpawn([&exceptionInFlightDuringWait, this](auto yield) {
        // Runs on this thread while the retry below is suspended on its timer.
        boost::asio::post(ctx_, [&exceptionInFlightDuringWait]() {
            exceptionInFlightDuringWait = std::current_exception() != nullptr;
        });

        std::size_t calls = 0;
        retryOnTimeout(
            [&calls]() -> int {
                if (++calls < 2)
                    throw DatabaseError{};
                return 0;
            },
            yield,
            util::Retry::Delays{
                .initial = std::chrono::milliseconds{1}, .max = std::chrono::milliseconds{1}
            }
        );
    });

    ASSERT_TRUE(exceptionInFlightDuringWait.has_value())
        << "the posted handler was expected to run while the retry was waiting";
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_FALSE(*exceptionInFlightDuringWait)
        << "retry.wait() must not be reached from inside a catch handler";
}

TEST_F(BackendInterfaceRetryCoroTest, RetryOnTimeoutCoroDoesNotBlockItsThread)
{
    bool ran = false;

    runSpawn([&ran, this](auto yield) {
        boost::asio::post(ctx_, [&ran]() { ran = true; });

        std::size_t calls = 0;
        retryOnTimeout(
            [&calls]() -> int {
                if (++calls < 2)
                    throw DatabaseError{};
                return 0;
            },
            yield,
            util::Retry::Delays{
                .initial = std::chrono::milliseconds{1}, .max = std::chrono::milliseconds{1}
            }
        );

        EXPECT_TRUE(ran);
    });
}
