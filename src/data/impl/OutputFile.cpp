#include "data/impl/OutputFile.hpp"

#include <xrpl/basics/base_uint.h>

#include <cstddef>
#include <cstring>
#include <expected>
#include <ios>
#include <string>
#include <utility>

namespace data::impl {

OutputFile::OutputFile(std::string const& path) : file_(path, std::ios::binary | std::ios::out)
{
}

bool
OutputFile::isOpen() const
{
    return file_.is_open();
}

void
OutputFile::writeRaw(char const* data, size_t size)
{
    writeToFile(data, size);
}

void
OutputFile::writeToFile(char const* data, size_t size)
{
    file_.write(data, size);
    shasum_.update(data, size);
}

xrpl::uint256
OutputFile::hash() const
{
    auto sum = shasum_;
    return std::move(sum).finalize();
}

std::expected<void, std::string>
OutputFile::close()
{
    file_.close();
    if (not file_) {
        return std::unexpected{"Error closing cache file"};
    }
    return {};
}

}  // namespace data::impl
