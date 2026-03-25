#include "util/config/Types.hpp"

#include "util/OverloadSet.hpp"

#include <ostream>
#include <variant>

namespace util::config {

std::ostream&
operator<<(std::ostream& stream, ConfigType type)
{
    switch (type) {
        case ConfigType::Integer:
            stream << "int";
            break;
        case ConfigType::String:
            stream << "string";
            break;
        case ConfigType::Double:
            stream << "double";
            break;
        case ConfigType::Boolean:
            stream << "boolean";
            break;
        default:
            stream << "unsupported type";
    }
    return stream;
}

std::ostream&
operator<<(std::ostream& stream, Value value)
{
    std::visit(
        util::OverloadSet{
            [&stream](bool const& val) { stream << (val ? "True" : "False"); },
            [&stream](auto const& val) { stream << val; }
        },
        value
    );
    return stream;
}

}  // namespace util::config
