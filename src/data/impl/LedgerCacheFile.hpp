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
        uint32_t version = kVERSION;
        uint32_t latestSeq{};
        uint64_t mapSize{};
        uint64_t deletedSize{};
    };

private:
    static constexpr uint32_t kVERSION = 1;

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
