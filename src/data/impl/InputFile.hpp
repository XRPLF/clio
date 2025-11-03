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

#include <cstddef>
#include <cstring>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <string>
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
        if (!file_.is_open()) {
            return 0;
        }

        auto const previousPosition = file_.tellg();
        if (previousPosition == std::streampos(-1)) {
            return 0;
        }

        file_.seekg(0, std::ios::end);
        auto const endPosition = file_.tellg();
        file_.seekg(previousPosition);

        if (endPosition == std::streampos(-1)) {
            return 0;
        }

        return static_cast<size_t>(endPosition);
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
            failed_ = !readFromFile(buffer_.data(), buffer_.size());
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

}  // namespace data::impl
