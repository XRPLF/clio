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

#include "data/DBHelpers.hpp"
#include "data/Types.hpp"
#include "etlng/Models.hpp"
#include "etlng/impl/ext/Successor.hpp"
#include "util/Assert.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/MockAssert.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockLedgerCache.hpp"
#include "util/MockPrometheus.hpp"
#include "util/StringUtils.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/UintTypes.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace etlng::impl;
using namespace data;

namespace {
constinit auto const kSEQ = 123u;
constinit auto const kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";

auto
createTestData(std::vector<etlng::model::Object> objects)
{
    auto transactions = std::vector{
        util::createTransaction(ripple::TxType::ttNFTOKEN_BURN),
        util::createTransaction(ripple::TxType::ttNFTOKEN_BURN),
        util::createTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER),
    };

    auto const header = createLedgerHeader(kLEDGER_HASH, kSEQ);
    return etlng::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = std::move(objects),
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = kSEQ
    };
}

[[maybe_unused]] auto
createInitialTestData(std::vector<ripple::uint256> edgeKeys)
{
    // initial data expects objects to be empty as well as non-empty edgeKeys
    ASSERT(not edgeKeys.empty(), "Initial data requires edgeKeys");

    auto ret = createTestData({});
    ret.edgeKeys = std::make_optional<std::vector<std::string>>();
    std::ranges::transform(edgeKeys, std::back_inserter(ret.edgeKeys.value()), &uint256ToString);

    return ret;
}

}  // namespace

struct SuccessorExtTests : util::prometheus::WithPrometheus, MockBackendTest {
protected:
    MockLedgerCache cache_;
    etlng::impl::SuccessorExt ext_{backend_, cache_};
};

TEST_F(SuccessorExtTests, OnLedgerDataLogicErrorIfCacheIsNotFullButSuccessorsNotPresent)
{
    auto const data = createTestData({});

    EXPECT_CALL(cache_, isFull()).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(cache_, latestLedgerSequence()).WillRepeatedly(testing::Return(kSEQ));

    EXPECT_THROW(ext_.onLedgerData(data), std::logic_error);
}

TEST_F(SuccessorExtTests, OnLedgerDataLogicErrorIfCacheIsFullButLatestSeqDiffersAndSuccessorsNotPresent)
{
    auto const data = createTestData({});

    EXPECT_CALL(cache_, isFull()).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(cache_, latestLedgerSequence()).WillRepeatedly(testing::Return(kSEQ - 1));

    EXPECT_THROW(ext_.onLedgerData(data), std::logic_error);
}

TEST_F(SuccessorExtTests, OnLedgerDataWithDeletedObjectButWithoutCachedPredecessorAndSuccessorAndNoBookBase)
{
    using namespace etlng::model;

    auto const objKey = "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960D";
    auto const deletedObj = util::createObject(Object::ModType::Deleted, objKey);
    auto const data = createTestData({
        deletedObj,
        util::createObject(Object::ModType::Modified),
    });

    EXPECT_CALL(cache_, isFull()).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(cache_, latestLedgerSequence()).WillRepeatedly(testing::Return(kSEQ));

    EXPECT_CALL(cache_, getPredecessor(deletedObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));
    EXPECT_CALL(cache_, getSuccessor(deletedObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));

    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(data::kFIRST_KEY), kSEQ, uint256ToString(data::kLAST_KEY)));
    EXPECT_CALL(cache_, getDeleted(deletedObj.key, kSEQ - 1)).WillRepeatedly(testing::Return(Blob{'0'}));

    ext_.onLedgerData(data);
}

TEST_F(SuccessorExtTests, OnLedgerDataWithCreatedObjectButWithoutCachedPredecessorAndSuccessorAndNoBookBase)
{
    using namespace etlng::model;

    auto const objKey = "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960D";
    auto const createdObj = util::createObject(Object::ModType::Created, objKey);
    auto const data = createTestData({
        createdObj,
        util::createObject(Object::ModType::Modified),
    });

    EXPECT_CALL(cache_, isFull()).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(cache_, latestLedgerSequence()).WillRepeatedly(testing::Return(kSEQ));

    EXPECT_CALL(cache_, getPredecessor(createdObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));
    EXPECT_CALL(cache_, getSuccessor(createdObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));

    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(data::kFIRST_KEY), kSEQ, uint256ToString(createdObj.key)));
    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(createdObj.key), kSEQ, uint256ToString(data::kLAST_KEY)));

    ext_.onLedgerData(data);
}

TEST_F(SuccessorExtTests, OnLedgerDataWithCreatedObjectButWithoutCachedPredecessorAndSuccessorWithBookBase)
{
    using namespace etlng::model;

    auto const objKey = "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960D";
    auto const createdObj = util::createObjectWithBookBase(Object::ModType::Created, objKey);
    auto const data = createTestData({
        createdObj,
        util::createObject(Object::ModType::Modified),
    });
    auto const bookBase = getBookBase(createdObj.key);

    EXPECT_CALL(cache_, isFull()).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(cache_, latestLedgerSequence()).WillRepeatedly(testing::Return(kSEQ));

    EXPECT_CALL(cache_, getPredecessor(createdObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));
    EXPECT_CALL(cache_, getSuccessor(createdObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));

    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(data::kFIRST_KEY), kSEQ, uint256ToString(createdObj.key)));
    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(createdObj.key), kSEQ, uint256ToString(data::kLAST_KEY)));

    EXPECT_CALL(cache_, get(createdObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));
    EXPECT_CALL(cache_, getSuccessor(bookBase, kSEQ)).WillRepeatedly(testing::Return(LedgerObject{}));

    ext_.onLedgerData(data);
}

TEST_F(
    SuccessorExtTests,
    OnLedgerDataWithCreatedObjectButWithoutCachedPredecessorAndSuccessorWithBookBaseAndMatchingSuccessorInCache
)
{
    using namespace etlng::model;

    auto const objKey = "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960D";
    auto const createdObj = util::createObjectWithBookBase(Object::ModType::Created, objKey);
    auto const data = createTestData({
        createdObj,
        util::createObject(Object::ModType::Modified),
    });
    auto const bookBase = getBookBase(createdObj.key);

    [[maybe_unused]] testing::InSequence inSeq;
    EXPECT_CALL(cache_, isFull()).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(cache_, latestLedgerSequence()).WillRepeatedly(testing::Return(kSEQ));

    EXPECT_CALL(cache_, getPredecessor(createdObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));
    EXPECT_CALL(cache_, getSuccessor(createdObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));

    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(data::kFIRST_KEY), kSEQ, uint256ToString(createdObj.key)));
    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(createdObj.key), kSEQ, uint256ToString(data::kLAST_KEY)));

    EXPECT_CALL(cache_, get(createdObj.key, kSEQ)).WillRepeatedly(testing::Return(data::Blob{'0'}));
    EXPECT_CALL(cache_, getSuccessor(bookBase, kSEQ))
        .WillRepeatedly(testing::Return(LedgerObject{.key = createdObj.key, .blob = {}}));

    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(bookBase), kSEQ, testing::_));

    ext_.onLedgerData(data);
}

TEST_F(
    SuccessorExtTests,
    OnLedgerDataWithDeletedObjectButWithoutCachedPredecessorAndSuccessorWithBookBaseButNoCurrentObjAndNoSuccessorInCache
)
{
    using namespace etlng::model;

    auto const objKey = "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960D";
    auto const createdObj = util::createObjectWithBookBase(Object::ModType::Created, objKey);
    auto const deletedObj = util::createObjectWithBookBase(Object::ModType::Deleted, objKey);
    auto const data = createTestData({
        deletedObj,
        util::createObject(Object::ModType::Modified),
    });
    auto const bookBase = getBookBase(deletedObj.key);
    auto const oldCachedObj = createdObj.data;

    [[maybe_unused]] testing::InSequence inSeq;
    EXPECT_CALL(cache_, isFull()).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(cache_, latestLedgerSequence()).WillRepeatedly(testing::Return(kSEQ));

    EXPECT_CALL(cache_, getPredecessor(deletedObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));
    EXPECT_CALL(cache_, getSuccessor(deletedObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));

    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(data::kFIRST_KEY), kSEQ, uint256ToString(data::kLAST_KEY)));
    EXPECT_CALL(cache_, getDeleted(deletedObj.key, kSEQ - 1)).WillOnce(testing::Return(oldCachedObj));

    EXPECT_CALL(cache_, get(deletedObj.key, kSEQ)).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(cache_, getSuccessor(bookBase, kSEQ)).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(bookBase), kSEQ, uint256ToString(data::kLAST_KEY)));

    ext_.onLedgerData(data);
}

TEST_F(
    SuccessorExtTests,
    OnLedgerDataWithDeletedObjectButWithoutCachedPredecessorAndSuccessorWithBookBaseAndCurrentObjAndSuccessorInCache
)
{
    using namespace etlng::model;

    auto const objKey = "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960D";
    auto const createdObj = util::createObjectWithBookBase(Object::ModType::Created, objKey);
    auto const deletedObj = util::createObjectWithBookBase(Object::ModType::Deleted, objKey);
    auto const data = createTestData({
        deletedObj,
        util::createObject(Object::ModType::Modified),
    });
    auto const bookBase = getBookBase(deletedObj.key);
    auto const oldCachedObj = createdObj.data;

    [[maybe_unused]] testing::InSequence inSeq;
    EXPECT_CALL(cache_, isFull()).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(cache_, latestLedgerSequence()).WillRepeatedly(testing::Return(kSEQ));

    EXPECT_CALL(cache_, getPredecessor(deletedObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));
    EXPECT_CALL(cache_, getSuccessor(deletedObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));

    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(data::kFIRST_KEY), kSEQ, uint256ToString(data::kLAST_KEY)));
    EXPECT_CALL(cache_, getDeleted(deletedObj.key, kSEQ - 1)).WillOnce(testing::Return(oldCachedObj));

    EXPECT_CALL(cache_, get(deletedObj.key, kSEQ)).WillOnce(testing::Return(data::Blob{'0'}));
    EXPECT_CALL(cache_, getSuccessor(bookBase, kSEQ))
        .WillRepeatedly(testing::Return(LedgerObject{.key = deletedObj.key, .blob = {}}));
    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(bookBase), kSEQ, uint256ToString(deletedObj.key)));

    ext_.onLedgerData(data);
}

TEST_F(SuccessorExtTests, OnLedgerDataWithDeletedObjectAndWithCachedPredecessorAndSuccessor)
{
    using namespace etlng::model;

    auto const objKey = "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960D";
    auto const predKey =
        binaryStringToUint256(hexStringToBinaryString("B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960C"
        ));
    auto const succKey =
        binaryStringToUint256(hexStringToBinaryString("B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960E"
        ));
    auto const createdObj = util::createObject(Object::ModType::Created, objKey);
    auto const data = createTestData({
        createdObj,
        util::createObject(Object::ModType::Modified),
    });

    EXPECT_CALL(cache_, isFull()).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(cache_, latestLedgerSequence()).WillRepeatedly(testing::Return(kSEQ));

    EXPECT_CALL(cache_, getPredecessor(createdObj.key, kSEQ))
        .WillOnce(testing::Return(data::LedgerObject{.key = predKey, .blob = {}}));
    EXPECT_CALL(cache_, getSuccessor(createdObj.key, kSEQ))
        .WillOnce(testing::Return(data::LedgerObject{.key = succKey, .blob = {}}));

    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(predKey), kSEQ, uint256ToString(createdObj.key)));
    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(createdObj.key), kSEQ, uint256ToString(succKey)));

    ext_.onLedgerData(data);
}

TEST_F(SuccessorExtTests, OnLedgerDataWithCreatedObjectAndIncludedSuccessors)
{
    using namespace etlng::model;

    auto const objKey = "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960D";
    auto const createdObj = util::createObject(Object::ModType::Created, objKey);
    auto data = createTestData({
        createdObj,
        util::createObject(Object::ModType::Modified),
    });
    auto const succ = util::createSuccessor();
    data.successors = {succ, succ, succ};

    EXPECT_CALL(*backend_, writeSuccessor(auto{succ.bookBase}, kSEQ, auto{succ.firstBook}))
        .Times(data.successors->size());

    EXPECT_CALL(*backend_, writeSuccessor(auto{createdObj.predecessor}, kSEQ, auto{createdObj.keyRaw}));
    EXPECT_CALL(*backend_, writeSuccessor(auto{createdObj.keyRaw}, kSEQ, auto{createdObj.successor}));

    ext_.onLedgerData(data);
}

TEST_F(SuccessorExtTests, OnLedgerDataWithDeletedObjectAndIncludedSuccessorsWithoutFirstBook)
{
    using namespace etlng::model;

    auto const objKey = "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960D";
    auto const deletedObj = util::createObject(Object::ModType::Deleted, objKey);
    auto data = createTestData({
        deletedObj,
        util::createObject(Object::ModType::Modified),
    });
    auto succ = util::createSuccessor();
    succ.firstBook = {};  // empty will be transformed into kLAST_KEY
    data.successors = {succ, succ};

    EXPECT_CALL(*backend_, writeSuccessor(auto{succ.bookBase}, kSEQ, uint256ToString(data::kLAST_KEY)))
        .Times(data.successors->size());

    EXPECT_CALL(*backend_, writeSuccessor(auto{deletedObj.predecessor}, kSEQ, auto{deletedObj.successor}));

    ext_.onLedgerData(data);
}

TEST_F(SuccessorExtTests, OnInitialDataWithSuccessorsButNotBookDirAndNoSuccessorsForEdgeKeys)
{
    using namespace etlng::model;

    auto const firstKey = ripple::uint256("B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960C");
    auto const secondKey = ripple::uint256("B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960E");
    auto const data = createInitialTestData({firstKey, secondKey});

    auto successorChain = std::queue<ripple::uint256>();
    successorChain.push(firstKey);
    successorChain.push(secondKey);

    [[maybe_unused]] testing::Sequence inSeq;
    EXPECT_CALL(cache_, isFull()).WillOnce(testing::Return(true));

    EXPECT_CALL(cache_, getSuccessor(testing::_, kSEQ))
        .Times(3)
        .InSequence(inSeq)
        .WillRepeatedly([&](auto&&, auto&&) -> std::optional<data::LedgerObject> {
            if (successorChain.empty())
                return std::nullopt;

            auto v = successorChain.front();
            successorChain.pop();
            return data::LedgerObject{.key = v, .blob = {'0'}};
        });

    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(data::kFIRST_KEY), kSEQ, uint256ToString(firstKey)));
    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(secondKey), kSEQ, uint256ToString(data::kLAST_KEY)));

    for (auto const& key : data.edgeKeys.value()) {
        EXPECT_CALL(cache_, getSuccessor(*ripple::uint256::fromVoidChecked(key), kSEQ))
            .InSequence(inSeq)
            .WillOnce(testing::Return(std::nullopt));
    }

    ext_.onInitialData(data);
}

TEST_F(SuccessorExtTests, OnInitialDataWithSuccessorsButNotBookDirAndSuccessorsForEdgeKeys)
{
    using namespace etlng::model;

    auto const firstKey = ripple::uint256("B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960C");
    auto const secondKey = ripple::uint256("B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960E");
    auto const data = createInitialTestData({firstKey, secondKey});

    auto successorChain = std::queue<ripple::uint256>();
    successorChain.push(firstKey);
    successorChain.push(secondKey);

    [[maybe_unused]] testing::Sequence inSeq;
    EXPECT_CALL(cache_, isFull()).WillOnce(testing::Return(true));

    EXPECT_CALL(cache_, getSuccessor(testing::_, kSEQ))
        .Times(3)
        .InSequence(inSeq)
        .WillRepeatedly([&](auto&&, auto&&) -> std::optional<data::LedgerObject> {
            if (successorChain.empty())
                return std::nullopt;

            auto v = successorChain.front();
            successorChain.pop();
            return data::LedgerObject{.key = v, .blob = {'0'}};
        });

    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(data::kFIRST_KEY), kSEQ, uint256ToString(firstKey)));
    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(secondKey), kSEQ, uint256ToString(data::kLAST_KEY)));

    for (auto const& key : data.edgeKeys.value()) {
        EXPECT_CALL(cache_, getSuccessor(*ripple::uint256::fromVoidChecked(key), kSEQ))
            .InSequence(inSeq)
            .WillOnce(testing::Return(data::LedgerObject{.key = firstKey, .blob = {}}));
        EXPECT_CALL(*backend_, writeSuccessor(auto{key}, kSEQ, uint256ToString(firstKey)));
    }

    ext_.onInitialData(data);
}

TEST_F(SuccessorExtTests, OnInitialDataWithSuccessorsAndBookDirAndSuccessorsForEdgeKeys)
{
    using namespace etlng::model;

    auto const firstKey = ripple::uint256("B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960C");
    auto const secondKey = ripple::uint256("B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960E");
    auto const data = createInitialTestData({firstKey, secondKey});

    auto successorChain = std::queue<ripple::uint256>();
    successorChain.push(firstKey);
    successorChain.push(secondKey);

    auto const bookBaseObj = util::createObjectWithBookBase(Object::ModType::Created);
    auto const bookBase = getBookBase(bookBaseObj.key);

    [[maybe_unused]] testing::Sequence inSeq;
    EXPECT_CALL(cache_, isFull()).WillOnce(testing::Return(true));

    EXPECT_CALL(cache_, getSuccessor(testing::_, kSEQ))
        .Times(3)
        .InSequence(inSeq)
        .WillRepeatedly([&](auto&&, auto&&) -> std::optional<data::LedgerObject> {
            if (successorChain.empty())
                return std::nullopt;

            auto v = successorChain.front();
            successorChain.pop();
            return data::LedgerObject{.key = v, .blob = bookBaseObj.data};
        });

    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(data::kFIRST_KEY), kSEQ, uint256ToString(firstKey)));
    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(secondKey), kSEQ, uint256ToString(data::kLAST_KEY)));

    EXPECT_CALL(cache_, get(bookBase, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));
    EXPECT_CALL(cache_, getSuccessor(bookBase, kSEQ))
        .WillRepeatedly(testing::Return(data::LedgerObject{.key = firstKey, .blob = data::Blob{'1'}}));
    EXPECT_CALL(
        *backend_, writeSuccessor(uint256ToString(bookBase), kSEQ, testing::_)
    );  // Called once because firstKey returned repeatedly above

    for (auto const& key : data.edgeKeys.value()) {
        EXPECT_CALL(cache_, getSuccessor(*ripple::uint256::fromVoidChecked(key), kSEQ))
            .InSequence(inSeq)
            .WillOnce(testing::Return(data::LedgerObject{.key = firstKey, .blob = {'1'}}));
        EXPECT_CALL(*backend_, writeSuccessor(auto{key}, kSEQ, uint256ToString(firstKey))).InSequence(inSeq);
    }

    ext_.onInitialData(data);
}

TEST_F(SuccessorExtTests, OnInitialObjectsWithEmptyLastKey)
{
    using namespace etlng::model;

    auto const lastKey = std::string{};
    auto const data = std::vector{
        util::createObject(
            Object::ModType::Created, "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960E"
        ),
        util::createObject(
            Object::ModType::Created, "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960F"
        ),
        util::createObject(
            Object::ModType::Created, "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E89610"
        ),
    };

    std::string lk = lastKey;
    for (auto const& obj : data) {
        if (not lk.empty())
            EXPECT_CALL(*backend_, writeSuccessor(std::move(lk), kSEQ, uint256ToString(obj.key)));
        lk = uint256ToString(obj.key);
    }

    ext_.onInitialObjects(kSEQ, data, lastKey);
}

TEST_F(SuccessorExtTests, OnInitialObjectsWithNonEmptyLastKey)
{
    using namespace etlng::model;

    auto const lastKey =
        uint256ToString(ripple::uint256("B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960D"));
    auto const data = std::vector{
        util::createObject(
            Object::ModType::Created, "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960E"
        ),
        util::createObject(
            Object::ModType::Created, "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960F"
        ),
        util::createObject(
            Object::ModType::Created, "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E89610"
        ),
    };

    std::string lk = lastKey;
    for (auto const& obj : data) {
        EXPECT_CALL(*backend_, writeSuccessor(std::move(lk), kSEQ, uint256ToString(obj.key)));
        lk = uint256ToString(obj.key);
    }

    ext_.onInitialObjects(kSEQ, data, lastKey);
}

struct SuccessorExtAssertTests : common::util::WithMockAssert, SuccessorExtTests {};

TEST_F(SuccessorExtAssertTests, OnLedgerDataWithDeletedObjectAssertsIfGetDeletedIsNotInCache)
{
    using namespace etlng::model;

    auto const objKey = "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960D";
    auto const deletedObj = util::createObject(Object::ModType::Deleted, objKey);
    auto const data = createTestData({
        deletedObj,
        util::createObject(Object::ModType::Modified),
    });

    EXPECT_CALL(cache_, isFull()).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(cache_, latestLedgerSequence()).WillRepeatedly(testing::Return(kSEQ));

    EXPECT_CALL(cache_, getPredecessor(deletedObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));
    EXPECT_CALL(cache_, getSuccessor(deletedObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));

    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(data::kFIRST_KEY), kSEQ, uint256ToString(data::kLAST_KEY)));
    EXPECT_CALL(cache_, getDeleted(deletedObj.key, kSEQ - 1)).WillRepeatedly(testing::Return(std::nullopt));

    EXPECT_CLIO_ASSERT_FAIL({ ext_.onLedgerData(data); });
}

TEST_F(
    SuccessorExtAssertTests,
    OnLedgerDataWithCreatedObjectButWithoutCachedPredecessorAndSuccessorWithBookBaseAndBookSuccessorNotInCache
)
{
    using namespace etlng::model;

    auto const objKey = "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960D";
    auto const createdObj = util::createObjectWithBookBase(Object::ModType::Created, objKey);
    auto const data = createTestData({
        createdObj,
        util::createObject(Object::ModType::Modified),
    });
    auto const bookBase = getBookBase(createdObj.key);

    EXPECT_CALL(cache_, isFull()).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(cache_, latestLedgerSequence()).WillRepeatedly(testing::Return(kSEQ));

    EXPECT_CALL(cache_, getPredecessor(createdObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));
    EXPECT_CALL(cache_, getSuccessor(createdObj.key, kSEQ)).WillRepeatedly(testing::Return(std::nullopt));

    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(data::kFIRST_KEY), kSEQ, uint256ToString(createdObj.key)));
    EXPECT_CALL(*backend_, writeSuccessor(uint256ToString(createdObj.key), kSEQ, uint256ToString(data::kLAST_KEY)));

    EXPECT_CALL(cache_, get(createdObj.key, kSEQ)).WillOnce(testing::Return(data::Blob{'0'}));
    EXPECT_CALL(cache_, getSuccessor(bookBase, kSEQ)).WillOnce(testing::Return(std::nullopt));

    EXPECT_CLIO_ASSERT_FAIL({ ext_.onLedgerData(data); });
}

TEST_F(SuccessorExtAssertTests, OnInitialDataNotIsFull)
{
    using namespace etlng::model;

    auto const data = createTestData({
        util::createObject(Object::ModType::Modified),
        util::createObject(Object::ModType::Created),
    });

    EXPECT_CALL(cache_, isFull()).WillOnce(testing::Return(false));
    EXPECT_CLIO_ASSERT_FAIL({ ext_.onInitialData(data); });
}

TEST_F(SuccessorExtAssertTests, OnInitialDataIsFullButNoEdgeKeys)
{
    using namespace etlng::model;

    auto data = createTestData({});

    EXPECT_CALL(cache_, isFull()).WillOnce(testing::Return(true));
    EXPECT_CLIO_ASSERT_FAIL({ ext_.onInitialData(data); });
}

TEST_F(SuccessorExtAssertTests, OnInitialDataIsFullWithEdgeKeysButHasObjects)
{
    using namespace etlng::model;

    auto const firstKey = ripple::uint256("B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960C");
    auto const secondKey = ripple::uint256("B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960E");
    auto data = createInitialTestData({firstKey, secondKey});
    data.objects = {util::createObject()};

    EXPECT_CALL(cache_, isFull()).WillOnce(testing::Return(true));
    EXPECT_CLIO_ASSERT_FAIL({ ext_.onInitialData(data); });
}
