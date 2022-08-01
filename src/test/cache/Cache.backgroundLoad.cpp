#include <gtest/gtest.h>

#include <clio/backend/SimpleCache.h>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

TEST(Cache, backgroundLoad)
{
    using namespace Backend;
    boost::log::core::get()->set_filter(
        boost::log::trivial::severity >= boost::log::trivial::warning);
    SimpleCache cache;
    ASSERT_FALSE(cache.isFull());
    ASSERT_EQ(cache.size(), 0);

    uint32_t startSeq = 10;
    uint32_t curSeq = startSeq;

    std::vector<LedgerObject> bObjs;
    bObjs.resize(100);
    for (size_t i = 0; i < bObjs.size(); ++i)
    {
        bObjs[i].key = ripple::uint256{i * 3 + 1};
        bObjs[i].blob = {(unsigned char)i + 1};
    }
    {
        auto objs = bObjs;
        objs.clear();
        std::copy(bObjs.begin(), bObjs.begin() + 10, std::back_inserter(objs));
        cache.update(objs, startSeq);
        ASSERT_EQ(cache.size(), 10);
        ASSERT_FALSE(cache.isFull());
        for (auto& obj : objs)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
        }
    }
    // some updates
    curSeq++;
    std::vector<LedgerObject> objs1;
    for (size_t i = 0; i < bObjs.size(); ++i)
    {
        if (i % 5 == 0)
            objs1.push_back(bObjs[i]);
    }
    for (auto& obj : objs1)
    {
        std::reverse(obj.blob.begin(), obj.blob.end());
    }
    cache.update(objs1, curSeq);

    {
        for (auto& obj : objs1)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (size_t i = 0; i < 10; i++)
        {
            auto& obj = bObjs[i];
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            auto newObj = std::find_if(objs1.begin(), objs1.end(), [&](auto o) {
                return o.key == obj.key;
            });
            if (newObj == objs1.end())
            {
                ASSERT_EQ(*cacheObj, obj.blob);
                cacheObj = cache.get(obj.key, startSeq);
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
            }
            else
            {
                ASSERT_EQ(*cacheObj, newObj->blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
        }
    }

    {
        auto objs = bObjs;
        objs.clear();
        std::copy(
            bObjs.begin() + 10, bObjs.begin() + 20, std::back_inserter(objs));
        cache.update(objs, startSeq, true);
    }
    {
        for (auto& obj : objs1)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (size_t i = 0; i < 20; i++)
        {
            auto& obj = bObjs[i];
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            auto newObj = std::find_if(objs1.begin(), objs1.end(), [&](auto o) {
                return o.key == obj.key;
            });
            if (newObj == objs1.end())
            {
                ASSERT_EQ(*cacheObj, obj.blob);
                cacheObj = cache.get(obj.key, startSeq);
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
            }
            else
            {
                ASSERT_EQ(*cacheObj, newObj->blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
        }
    }

    // some inserts
    curSeq++;
    auto objs2 = objs1;
    objs2.clear();
    for (size_t i = 0; i < bObjs.size(); ++i)
    {
        if (i % 7 == 0)
        {
            auto obj = bObjs[i];
            obj.key = ripple::uint256{(i + 1) * 1000};
            obj.blob = {(unsigned char)(i + 1) * 100};
            objs2.push_back(obj);
        }
    }
    cache.update(objs2, curSeq);
    {
        for (auto& obj : objs1)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (auto& obj : objs2)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (size_t i = 0; i < 20; i++)
        {
            auto& obj = bObjs[i];
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            auto newObj = std::find_if(objs1.begin(), objs1.end(), [&](auto o) {
                return o.key == obj.key;
            });
            if (newObj == objs1.end())
            {
                ASSERT_EQ(*cacheObj, obj.blob);
                cacheObj = cache.get(obj.key, startSeq);
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
            }
            else
            {
                ASSERT_EQ(*cacheObj, newObj->blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
        }
    }

    {
        auto objs = bObjs;
        objs.clear();
        std::copy(
            bObjs.begin() + 20, bObjs.begin() + 30, std::back_inserter(objs));
        cache.update(objs, startSeq, true);
    }
    {
        for (auto& obj : objs1)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (auto& obj : objs2)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (size_t i = 0; i < 30; i++)
        {
            auto& obj = bObjs[i];
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            auto newObj = std::find_if(objs1.begin(), objs1.end(), [&](auto o) {
                return o.key == obj.key;
            });
            if (newObj == objs1.end())
            {
                ASSERT_EQ(*cacheObj, obj.blob);
                cacheObj = cache.get(obj.key, startSeq);
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
            }
            else
            {
                ASSERT_EQ(*cacheObj, newObj->blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
        }
    }

    // some deletes
    curSeq++;
    auto objs3 = objs1;
    objs3.clear();
    for (size_t i = 0; i < bObjs.size(); ++i)
    {
        if (i % 6 == 0)
        {
            auto obj = bObjs[i];
            obj.blob = {};
            objs3.push_back(obj);
        }
    }
    cache.update(objs3, curSeq);
    {
        for (auto& obj : objs1)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            if (std::find_if(objs3.begin(), objs3.end(), [&](auto o) {
                    return o.key == obj.key;
                }) == objs3.end())
            {
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
            else
            {
                ASSERT_FALSE(cacheObj);
            }
        }
        for (auto& obj : objs2)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (auto& obj : objs3)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_FALSE(cacheObj);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (size_t i = 0; i < 30; i++)
        {
            auto& obj = bObjs[i];
            auto cacheObj = cache.get(obj.key, curSeq);
            auto newObj = std::find_if(objs1.begin(), objs1.end(), [&](auto o) {
                return o.key == obj.key;
            });
            auto delObj = std::find_if(objs3.begin(), objs3.end(), [&](auto o) {
                return o.key == obj.key;
            });
            if (delObj != objs3.end())
            {
                ASSERT_FALSE(cacheObj);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
            else if (newObj == objs1.end())
            {
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
                cacheObj = cache.get(obj.key, startSeq);
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
            }
            else
            {
                ASSERT_EQ(*cacheObj, newObj->blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
        }
    }
    {
        auto objs = bObjs;
        objs.clear();
        std::copy(bObjs.begin() + 30, bObjs.end(), std::back_inserter(objs));
        cache.update(objs, startSeq, true);
    }
    {
        for (auto& obj : objs1)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            if (std::find_if(objs3.begin(), objs3.end(), [&](auto o) {
                    return o.key == obj.key;
                }) == objs3.end())
            {
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
            else
            {
                ASSERT_FALSE(cacheObj);
            }
        }
        for (auto& obj : objs2)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (auto& obj : objs3)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_FALSE(cacheObj);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (size_t i = 0; i < bObjs.size(); i++)
        {
            auto& obj = bObjs[i];
            auto cacheObj = cache.get(obj.key, curSeq);
            auto newObj = std::find_if(objs1.begin(), objs1.end(), [&](auto o) {
                return o.key == obj.key;
            });
            auto delObj = std::find_if(objs3.begin(), objs3.end(), [&](auto o) {
                return o.key == obj.key;
            });
            if (delObj != objs3.end())
            {
                ASSERT_FALSE(cacheObj);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
            else if (newObj == objs1.end())
            {
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
                cacheObj = cache.get(obj.key, startSeq);
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
            }
            else
            {
                ASSERT_EQ(*cacheObj, newObj->blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
        }
    }
    cache.setFull();
    auto allObjs = bObjs;
    allObjs.clear();
    for (size_t i = 0; i < bObjs.size(); i++)
    {
        auto& obj = bObjs[i];
        auto cacheObj = cache.get(obj.key, curSeq);
        auto newObj = std::find_if(objs1.begin(), objs1.end(), [&](auto o) {
            return o.key == obj.key;
        });
        auto delObj = std::find_if(objs3.begin(), objs3.end(), [&](auto o) {
            return o.key == obj.key;
        });
        if (delObj != objs3.end())
        {
            ASSERT_FALSE(cacheObj);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        else if (newObj == objs1.end())
        {
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            cacheObj = cache.get(obj.key, startSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            allObjs.push_back(obj);
        }
        else
        {
            allObjs.push_back(*newObj);
            ASSERT_EQ(*cacheObj, newObj->blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
    }
    for (auto& obj : objs2)
    {
        allObjs.push_back(obj);
    }
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