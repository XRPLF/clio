#include <gtest/gtest.h>

#include <clio/backend/SimpleCache.h>
#include <test/env/env.h>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

TEST(Cache, basicCache)
{
    using namespace Backend;
    boost::log::core::get()->set_filter(
        boost::log::trivial::severity >= boost::log::trivial::warning);
    SimpleCache cache;
    ASSERT_FALSE(cache.isFull());
    cache.setFull();

    // Nothing in cache
    {
        ASSERT_TRUE(cache.isFull());
        ASSERT_EQ(cache.size(), 0);
        ASSERT_FALSE(cache.get(ripple::uint256{12}, 0));
        ASSERT_FALSE(cache.getSuccessor(firstKey, 0));
        ASSERT_FALSE(cache.getPredecessor(lastKey, 0));
    }

    // insert
    uint32_t curSeq = 1;
    std::vector<LedgerObject> objs;
    objs.push_back({});
    objs[0] = {ripple::uint256{42}, {0xCC}};
    cache.update(objs, curSeq);
    {
        auto& obj = objs[0];
        ASSERT_TRUE(cache.isFull());
        ASSERT_EQ(cache.size(), 1);
        auto cacheObj = cache.get(obj.key, curSeq);
        ASSERT_TRUE(cacheObj);
        ASSERT_EQ(*cacheObj, obj.blob);
        ASSERT_FALSE(cache.get(obj.key, curSeq + 1));
        ASSERT_FALSE(cache.get(obj.key, curSeq - 1));
        ASSERT_FALSE(cache.getSuccessor(obj.key, curSeq));
        ASSERT_FALSE(cache.getPredecessor(obj.key, curSeq));
        auto succ = cache.getSuccessor(firstKey, curSeq);
        ASSERT_TRUE(succ);
        ASSERT_EQ(*succ, obj);
        auto pred = cache.getPredecessor(lastKey, curSeq);
        ASSERT_TRUE(pred);
        ASSERT_EQ(pred, obj);
    }

    // update
    curSeq++;
    objs[0].blob = {0x01};
    cache.update(objs, curSeq);
    {
        auto& obj = objs[0];
        ASSERT_EQ(cache.size(), 1);
        auto cacheObj = cache.get(obj.key, curSeq);
        ASSERT_TRUE(cacheObj);
        ASSERT_EQ(*cacheObj, obj.blob);
        ASSERT_FALSE(cache.get(obj.key, curSeq + 1));
        ASSERT_FALSE(cache.get(obj.key, curSeq - 1));
        ASSERT_TRUE(cache.isFull());
        ASSERT_FALSE(cache.getSuccessor(obj.key, curSeq));
        ASSERT_FALSE(cache.getPredecessor(obj.key, curSeq));
        auto succ = cache.getSuccessor(firstKey, curSeq);
        ASSERT_TRUE(succ);
        ASSERT_EQ(*succ, obj);
        auto pred = cache.getPredecessor(lastKey, curSeq);
        ASSERT_TRUE(pred);
        ASSERT_EQ(*pred, obj);
    }
    // empty update
    curSeq++;
    cache.update({}, curSeq);
    {
        auto& obj = objs[0];
        ASSERT_EQ(cache.size(), 1);
        auto cacheObj = cache.get(obj.key, curSeq);
        ASSERT_TRUE(cacheObj);
        ASSERT_EQ(*cacheObj, obj.blob);
        ASSERT_TRUE(cache.get(obj.key, curSeq - 1));
        ASSERT_FALSE(cache.get(obj.key, curSeq - 2));
        ASSERT_EQ(*cache.get(obj.key, curSeq - 1), obj.blob);
        ASSERT_FALSE(cache.getSuccessor(obj.key, curSeq));
        ASSERT_FALSE(cache.getPredecessor(obj.key, curSeq));
        auto succ = cache.getSuccessor(firstKey, curSeq);
        ASSERT_TRUE(succ);
        ASSERT_EQ(*succ, obj);
        auto pred = cache.getPredecessor(lastKey, curSeq);
        ASSERT_TRUE(pred);
        ASSERT_EQ(*pred, obj);
    }
    // delete
    curSeq++;
    objs[0].blob = {};
    cache.update(objs, curSeq);
    {
        auto& obj = objs[0];
        ASSERT_EQ(cache.size(), 0);
        auto cacheObj = cache.get(obj.key, curSeq);
        ASSERT_FALSE(cacheObj);
        ASSERT_FALSE(cache.get(obj.key, curSeq + 1));
        ASSERT_FALSE(cache.get(obj.key, curSeq - 1));
        ASSERT_TRUE(cache.isFull());
        ASSERT_FALSE(cache.getSuccessor(obj.key, curSeq));
        ASSERT_FALSE(cache.getPredecessor(obj.key, curSeq));
        ASSERT_FALSE(cache.getSuccessor(firstKey, curSeq));
        ASSERT_FALSE(cache.getPredecessor(lastKey, curSeq));
    }
    // random non-existent object
    {
        ASSERT_FALSE(cache.get(ripple::uint256{23}, curSeq));
    }

    // insert several objects
    curSeq++;
    objs.resize(10);
    for (size_t i = 0; i < objs.size(); ++i)
    {
        objs[i] = {
            ripple::uint256{i * 100 + 1},
            {(unsigned char)i, (unsigned char)i * 2, (unsigned char)i + 1}};
    }

    cache.update(objs, curSeq);
    {
        ASSERT_EQ(cache.size(), 10);
        for (auto& obj : objs)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, curSeq - 1));
            ASSERT_FALSE(cache.get(obj.key, curSeq + 1));
        }

        std::optional<LedgerObject> succ = {{firstKey, {}}};
        size_t idx = 0;
        while ((succ = cache.getSuccessor(succ->key, curSeq)))
        {
            ASSERT_EQ(*succ, objs[idx++]);
        }
        ASSERT_EQ(idx, objs.size());
    }

    // insert several more objects
    curSeq++;
    auto objs2 = objs;
    for (size_t i = 0; i < objs.size(); ++i)
    {
        objs2[i] = {
            ripple::uint256{i * 100 + 50},
            {(unsigned char)i, (unsigned char)i * 3, (unsigned char)i + 5}};
    }

    cache.update(objs2, curSeq);
    {
        ASSERT_EQ(cache.size(), 20);
        for (auto& obj : objs)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            cacheObj = cache.get(obj.key, curSeq - 1);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, curSeq - 2));
            ASSERT_FALSE(cache.get(obj.key, curSeq + 1));
        }
        for (auto& obj : objs2)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, curSeq - 1));
            ASSERT_FALSE(cache.get(obj.key, curSeq + 1));
        }
        std::optional<LedgerObject> succ = {{firstKey, {}}};
        size_t idx = 0;
        while ((succ = cache.getSuccessor(succ->key, curSeq)))
        {
            if (idx % 2 == 0)
                ASSERT_EQ(*succ, objs[(idx++) / 2]);
            else
                ASSERT_EQ(*succ, objs2[(idx++) / 2]);
        }
        ASSERT_EQ(idx, objs.size() + objs2.size());
    }

    // mix of inserts, updates and deletes
    curSeq++;
    for (size_t i = 0; i < objs.size(); ++i)
    {
        if (i % 2 == 0)
            objs[i].blob = {};
        else if (i % 2 == 1)
            std::reverse(objs[i].blob.begin(), objs[i].blob.end());
    }
    cache.update(objs, curSeq);
    {
        ASSERT_EQ(cache.size(), 15);
        for (size_t i = 0; i < objs.size(); ++i)
        {
            auto& obj = objs[i];
            auto cacheObj = cache.get(obj.key, curSeq);
            if (i % 2 == 0)
            {
                ASSERT_FALSE(cacheObj);
                ASSERT_FALSE(cache.get(obj.key, curSeq - 1));
                ASSERT_FALSE(cache.get(obj.key, curSeq - 2));
            }
            else
            {
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
                ASSERT_FALSE(cache.get(obj.key, curSeq - 1));
                ASSERT_FALSE(cache.get(obj.key, curSeq - 2));
            }
        }
        for (auto& obj : objs2)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            cacheObj = cache.get(obj.key, curSeq - 1);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, curSeq - 2));
        }

        auto allObjs = objs;
        allObjs.clear();
        std::copy_if(
            objs.begin(),
            objs.end(),
            std::back_inserter(allObjs),
            [](auto obj) { return obj.blob.size() > 0; });
        std::copy(objs2.begin(), objs2.end(), std::back_inserter(allObjs));
        std::sort(allObjs.begin(), allObjs.end(), [](auto a, auto b) {
            return a.key < b.key;
        });
        std::optional<LedgerObject> succ = {{firstKey, {}}};
        size_t idx = 0;
        while ((succ = cache.getSuccessor(succ->key, curSeq)))
        {
            ASSERT_EQ(*succ, allObjs[idx++]);
        }
        ASSERT_EQ(idx, allObjs.size());
    }
}