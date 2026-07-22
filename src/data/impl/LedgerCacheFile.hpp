#pragma once

#include "data/LedgerCache.hpp"
#include "data/impl/InputFile.hpp"
#include "data/impl/OutputFile.hpp"

#include <fmt/format.h>
#include <xrpl/basics/base_uint.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace data::impl {

class LedgerCacheFile {
public:
    struct Header {
        uint32_t version = kVersion;
        uint32_t latestSeq{};
        uint64_t mapSize{};
        uint64_t deletedSize{};
    };

private:
    static constexpr uint32_t kVersion = 1;

    std::string path_;

public:
    template <typename T>
    struct DataBase {
        uint32_t latestSeq{0};
        T map;
        T deleted;
    };

    using DataView = DataBase<LedgerCache::CacheMap const&>;
    using Data = DataBase<LedgerCache::CacheMap>;

    LedgerCacheFile(std::string path);

    std::expected<void, std::string>
    write(DataView dataView);

    std::expected<Data, std::string>
    read(uint32_t minLatestSequence);
};

}  // namespace data::impl
