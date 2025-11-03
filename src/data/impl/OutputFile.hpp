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

#include "util/Assert.hpp"

#include <cstddef>
#include <cstring>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

namespace data::impl {

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
        writeRaw(reinterpret_cast<char const*>(data), size);
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

private:
    void
    flush()
    {
        if (cursorPosition_ == 0) {
            return;
        }

        writeToFile(buffer_.data(), cursorPosition_);
        cursorPosition_ = 0;
        cursor_ = buffer_.data();
    }
};

}  // namespace data::impl
