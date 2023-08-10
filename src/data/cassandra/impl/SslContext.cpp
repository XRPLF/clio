//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <data/cassandra/impl/SslContext.h>

namespace {
static constexpr auto contextDeleter = [](CassSsl* ptr) { cass_ssl_free(ptr); };
}  // namespace

namespace data::cassandra::detail {

SslContext::SslContext(std::string const& certificate) : ManagedObject{cass_ssl_new(), contextDeleter}
{
    cass_ssl_set_verify_flags(*this, CASS_SSL_VERIFY_NONE);
    if (auto const rc = cass_ssl_add_trusted_cert(*this, certificate.c_str()); rc != CASS_OK)
    {
        throw std::runtime_error(std::string{"Error setting Cassandra SSL Context: "} + cass_error_desc(rc));
    }
}

}  // namespace data::cassandra::detail
