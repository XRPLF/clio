#pragma once

#include "util/Shasum.hpp"

#include <xrpl/basics/base_uint.h>

#include <cstddef>
#include <cstring>
#include <expected>
#include <fstream>
#include <string>

namespace data::impl {

class OutputFile {
    std::ofstream file_;
    util::Sha256sum shasum_;

public:
    OutputFile(std::string const& path);

    bool
    isOpen() const;

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

    void
    writeRaw(char const* data, size_t size);

    ripple::uint256
    hash() const;

    std::expected<void, std::string>
    close();

private:
    void
    writeToFile(char const* data, size_t size);
};

}  // namespace data::impl
