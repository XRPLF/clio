#pragma once

#include "data/LedgerHeaderCache.hpp"

#include <gmock/gmock.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <optional>

struct MockLedgerHeaderCache {
    MockLedgerHeaderCache() = default;
    using CacheEntry = data::FetchLedgerCache::CacheEntry;

    MOCK_METHOD(void, put, (CacheEntry), ());
    MOCK_METHOD(std::optional<CacheEntry>, get, (), (const));
};
