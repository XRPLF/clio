#include "data/cassandra/impl/SslContext.hpp"

#include "data/cassandra/impl/ManagedObject.hpp"

#include <cassandra.h>

#include <stdexcept>
#include <string>

namespace {
constexpr auto kCONTEXT_DELETER = [](CassSsl* ptr) { cass_ssl_free(ptr); };
}  // namespace

namespace data::cassandra::impl {

SslContext::SslContext(std::string const& certificate)
    : ManagedObject{cass_ssl_new(), kCONTEXT_DELETER}
{
    cass_ssl_set_verify_flags(*this, CASS_SSL_VERIFY_NONE);
    if (auto const rc = cass_ssl_add_trusted_cert(*this, certificate.c_str()); rc != CASS_OK) {
        throw std::runtime_error(
            std::string{"Error setting Cassandra SSL Context: "} + cass_error_desc(rc)
        );
    }
}

}  // namespace data::cassandra::impl
