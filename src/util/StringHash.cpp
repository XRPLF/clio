#include "util/StringHash.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace util {

size_t
StringHash::operator()(char const* str) const
{
    return hash_type{}(str);
}

size_t
StringHash::operator()(std::string_view str) const
{
    return hash_type{}(str);
}

size_t
StringHash::operator()(std::string const& str) const
{
    return hash_type{}(str);
}

}  // namespace util
