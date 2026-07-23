#include "data/impl/InputFile.hpp"

#include <xrpl/basics/base_uint.h>

#include <cstddef>
#include <cstring>
#include <ios>
#include <iosfwd>
#include <string>
#include <utility>

namespace data::impl {

InputFile::InputFile(std::string const& path) : file_(path, std::ios::binary | std::ios::in)
{
}

bool
InputFile::isOpen() const
{
    return file_.is_open();
}

bool
InputFile::readRaw(char* data, size_t size)
{
    file_.read(data, size);
    shasum_.update(data, size);
    return not file_.fail();
}

xrpl::uint256
InputFile::hash() const
{
    auto sum = shasum_;
    return std::move(sum).finalize();
}

}  // namespace data::impl
