#pragma once

#include "util/Shasum.hpp"

#include <xrpl/basics/base_uint.h>

#include <cstddef>
#include <cstring>
#include <fstream>
#include <iosfwd>
#include <string>

namespace data::impl {

class InputFile {
    std::ifstream file_;
    util::Sha256sum shasum_;

public:
    InputFile(std::string const& path);

    bool
    isOpen() const;

    template <typename T>
    bool
    read(T& t)
    {
        return readRaw(reinterpret_cast<char*>(&t), sizeof(T));
    }

    bool
    readRaw(char* data, size_t size);

    ripple::uint256
    hash() const;
};
}  // namespace data::impl
