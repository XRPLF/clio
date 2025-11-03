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
#include <iosfwd>
#include <string>
#include <vector>

namespace data::impl {

class InputFile {
    std::ifstream file_;

public:
    InputFile(std::string const& path, bool useCompression);
    virtual ~InputFile() = default;
    bool
    isOpen() const;

    template <typename T>
    bool
    read(T& t)
    {
        return readRaw(reinterpret_cast<char*>(&t), sizeof(T));
    }

    virtual bool
    readRaw(char* data, size_t size);

protected:
    bool
    readFromFile(char* data, size_t size);
    size_t
    fileSize();
};

class BufferedInputFile : public InputFile {
    std::vector<char> buffer_;
    char* cursor_ = nullptr;
    size_t cursorPosition_ = 0;
    bool failed_ = false;

public:
    BufferedInputFile(std::string const& path, bool useCompression);
    bool
    readRaw(char* data, size_t size) override;
};

}  // namespace data::impl
