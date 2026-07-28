#include "util/requests/SslContext.hpp"

#include "util/requests/impl/SslContext.hpp"

#include <expected>
#include <string>

namespace util::requests {

std::expected<void, std::string>
initClientSslContext()
{
    return impl::initClientSslContext();
}

}  // namespace util::requests
