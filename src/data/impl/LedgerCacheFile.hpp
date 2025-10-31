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
#include "data/Types.hpp"
#include "util/Assert.hpp"

#include <fmt/format.h>
#include <xrpl/basics/base_uint.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace data::impl {

class InputFile {
    std::ifstream file_;

protected:
    bool
    readFromFile(char* data, size_t size)
    {
        file_.read(data, size);
        return not file_.fail();
    }

    size_t
    fileSize()
    {
        auto const previousPosition = file_.tellg();
        file_.seekg(std::fstream::end);
        auto const size = file_.tellg();
        file_.seekg(previousPosition);
        return size;
    }

public:
    InputFile(std::string const& path, [[maybe_unused]] bool useCompression)
        : file_(path, std::ios::binary | std::ios::in)
    {
    }

    virtual ~InputFile() = default;

    bool
    isOpen() const
    {
        return file_.is_open();
    }

    template <typename T>
    bool
    read(T& t)
    {
        return readRaw(reinterpret_cast<char*>(&t), sizeof(T));
    }

    virtual bool
    readRaw(char* data, size_t size)
    {
        file_.read(data, size);
        return not file_.fail();
    }
};

class BufferedInputFile : public InputFile {
    std::vector<char> buffer_;
    char* cursor_ = nullptr;
    size_t cursorPosition_ = 0;
    bool failed_ = false;

public:
    BufferedInputFile(std::string const& path, bool useCompression) : InputFile(path, useCompression)
    {
        if (isOpen()) {
            buffer_.resize(fileSize());
            failed_ |= readFromFile(buffer_.data(), buffer_.size());
            cursor_ = buffer_.data();
            cursorPosition_ = 0;
        } else {
            failed_ = true;
        }
    }

    bool
    readRaw(char* data, size_t size) override
    {
        if (failed_ || (buffer_.size() < cursorPosition_ + size)) {
            return false;
        }
        std::memcpy(data, cursor_, size);
        cursor_ += size;
        cursorPosition_ += size;
        return true;
    }
};

class OutputFile {
    std::ofstream file_;

protected:
    void
    writeToFile(char const* data, size_t size)
    {
        file_.write(data, size);
    }

public:
    OutputFile(std::string const& path, [[maybe_unused]] bool useCompression)
        : file_(path, std::ios::binary | std::ios::out)
    {
    }

    virtual ~OutputFile() = default;

    bool
    isOpen() const
    {
        return file_.is_open();
    }

    template <typename T>
    void
    write(T&& data)
    {
        writeRaw(reinterpret_cast<char const*>(&data), sizeof(T));
    }

    template <typename T>
    void
    write(T const* data, size_t const size)
    {
        writeRaw(reinterpret_cast<char const*>(&data), size);
    }

    virtual void
    writeRaw(char const* data, size_t size)
    {
        writeToFile(data, size);
    }
};

class BufferedOutputFile : public OutputFile {
    std::vector<char> buffer_;
    char* cursor_ = nullptr;
    size_t cursorPosition_ = 0;

public:
    BufferedOutputFile(std::string const& path, bool useCompression, size_t bufferSize)
        : OutputFile(path, useCompression)
    {
        buffer_.resize(bufferSize);
        cursor_ = buffer_.data();
        cursorPosition_ = 0;
    }

    ~BufferedOutputFile() override
    {
        flush();
    }

    void
    writeRaw(char const* data, size_t size) override
    {
        ASSERT(cursorPosition_ + size <= buffer_.size(), "Not enough space in buffer");
        std::memcpy(cursor_, data, size);
        cursor_ += size;
        cursorPosition_ += size;
    }

    void
    flush()
    {
        if (buffer_.empty()) {
            return;
        }

        writeToFile(buffer_.data(), buffer_.size());
        buffer_.clear();
        cursor_ = buffer_.data();
    }
};

class LedgerCacheFile {
    std::string path_;
    bool isBuffered_;
    bool useCompression_;

    static constexpr uint32_t kVERSION = 1;
    using Separator = std::array<char, 16>;
    static constexpr Separator kSEPARATOR = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    struct Header {
        uint32_t version = kVERSION;
        uint64_t datetime{};
        uint32_t latestSeq{};
        uint64_t mapSize{};
        uint64_t deletedSize{};
    };

public:
    template <typename T>
    struct DataBase {
        uint32_t latestSeq{0};
        T map;
        T deleted;
    };

    using DataView = DataBase<LedgerCache::CacheMap const&>;
    using Data = DataBase<LedgerCache::CacheMap>;

    LedgerCacheFile(std::string path, bool isBuffered, bool useCompression)
        : path_(std::move(path)), isBuffered_(isBuffered), useCompression_(useCompression)
    {
    }

    static size_t
    outputSize(DataView dataView)
    {
        // TODO: implement correctly
        return sizeof(dataView);
    }

    std::expected<void, std::string>
    write(DataView dataView)
    {
        auto file = [&]() -> std::unique_ptr<OutputFile> {
            if (isBuffered_) {
                return std::make_unique<BufferedOutputFile>(path_, useCompression_, outputSize(dataView));
            }
            return std::make_unique<OutputFile>(path_, useCompression_);
        }();
        if (not file->isOpen()) {
            return std::unexpected{fmt::format("Couldn't open file: {}", path_)};
        }

        Header const header{
            .datetime = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()
            )
                            .count(),
            .latestSeq = dataView.latestSeq,
            .mapSize = dataView.map.size(),
            .deletedSize = dataView.deleted.size()
        };
        file->write(header);
        file->write(kSEPARATOR);
        for (auto const& [k, v] : dataView.map) {
            file->write(k.data(), decltype(k)::bytes);
            file->write(v.seq);
            file->write(v.blob.size());
        }
        // TODO: write hash of the map
        file->write(kSEPARATOR);
        for (auto const& [k, v] : dataView.deleted) {
            file->write(k.data(), decltype(k)::bytes);
            file->write(v.seq);
            file->write(v.blob.size());
        }
        file->write(kSEPARATOR);
        // TODO:`write hash of the deleted
        file->write(kSEPARATOR);
        return {};
    }

    std::expected<Data, std::string>
    read()
    {
        auto file = [&]() -> std::unique_ptr<InputFile> {
            if (isBuffered_) {
                return std::make_unique<BufferedInputFile>(path_, useCompression_);
            }
            return std::make_unique<InputFile>(path_, useCompression_);
        }();
        if (not file->isOpen()) {
            return std::unexpected{fmt::format("Couldn't open file: {}", path_)};
        }

        Data result;

        Header header{};
        if (not file->read(header)) {
            return std::unexpected{"Error reading cache header"};
        }
        result.latestSeq = header.latestSeq;

        Separator separator{};
        if (not file->readRaw(separator.data(), separator.size())) {
            return std::unexpected{"Error reading cache header"};
        }
        if (not std::ranges::all_of(separator, [](char c) { return c == 0; })) {
            return std::unexpected{"Cache file corruption detected"};
        }

        for (size_t i = 0; i < header.mapSize; ++i) {
            auto cacheEntryExpected = readCacheEntry(*file, i);
            if (not cacheEntryExpected.has_value()) {
                return std::unexpected{std::move(cacheEntryExpected).error()};
            }
            result.map.insert(result.map.end(), std::move(cacheEntryExpected).value());
        }

        if (not file->readRaw(separator.data(), separator.size())) {
            return std::unexpected{"Error reading separator"};
        }
        if (not std::ranges::all_of(separator, [](char c) { return c == 0; })) {
            return std::unexpected{"Cache file corruption detected"};
        }

        for (size_t i = 0; i < header.deletedSize; ++i) {
            auto cacheEntryExpected = readCacheEntry(*file, i);
            if (not cacheEntryExpected.has_value()) {
                return std::unexpected{std::move(cacheEntryExpected).error()};
            }
            result.deleted.insert(result.deleted.end(), std::move(cacheEntryExpected).value());
        }

        if (not file->readRaw(separator.data(), separator.size())) {
            return std::unexpected{"Error reading separator"};
        }
        if (not std::ranges::all_of(separator, [](char c) { return c == 0; })) {
            return std::unexpected{"Cache file corruption detected"};
        }

        return result;
    }

private:
    static std::expected<std::pair<ripple::uint256, LedgerCache::CacheEntry>, std::string>
    readCacheEntry(InputFile& file, size_t i)
    {
        ripple::uint256 key;
        if (not file.readRaw(reinterpret_cast<char*>(key.data()), ripple::base_uint<256>::bytes)) {
            return std::unexpected(fmt::format("Failed to read key at index {}", i));
        }

        uint32_t seq{};
        if (not file.read(seq)) {
            return std::unexpected(fmt::format("Failed to read sequence at index {}", i));
        }

        size_t blobSize{};
        if (not file.read(blobSize)) {
            return std::unexpected(fmt::format("Failed to read blob size at index {}", i));
        }

        Blob blob;
        blob.resize(blobSize);
        if (not file.readRaw(reinterpret_cast<char*>(blob.data()), blobSize)) {
            return std::unexpected(fmt::format("Failed to read blob data at index ", i));
        }
        return std::make_pair(key, LedgerCache::CacheEntry{.seq = seq, .blob = std::move(blob)});
    }
};

}  // namespace data::impl
