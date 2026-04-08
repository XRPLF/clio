#include "web/ng/Connection.hpp"

#include "util/Taggable.hpp"

#include <boost/beast/core/flat_buffer.hpp>

#include <string>
#include <utility>

namespace web::ng {

ConnectionMetadata::ConnectionMetadata(
    std::string ip,
    util::TagDecoratorFactory const& tagDecoratorFactory
)
    : util::Taggable(tagDecoratorFactory), ip_{std::move(ip)}
{
}

std::string const&
ConnectionMetadata::ip() const
{
    return ip_;
}

bool
ConnectionMetadata::isAdmin() const
{
    return isAdmin_.value_or(false);
}

Connection::Connection(
    std::string ip,
    boost::beast::flat_buffer buffer,
    util::TagDecoratorFactory const& tagDecoratorFactory
)
    : ConnectionMetadata{std::move(ip), tagDecoratorFactory}, buffer_{std::move(buffer)}
{
}

}  // namespace web::ng
