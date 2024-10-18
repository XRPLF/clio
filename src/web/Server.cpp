//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "web/Server.hpp"

#include "util/newconfig/ConfigDefinition.hpp"

#include <boost/asio/ssl/context.hpp>

#include <optional>
#include <string>

namespace web {

std::expected<std::optional<boost::asio::ssl::context>, std::string>
makeServerSslContext(util::config::ClioConfigDefinition const& config)
{
    bool const configHasCertFile = config.getValue("ssl_cert_file").hasValue();
    bool const configHasKeyFile = config.getValue("ssl_key_file").hasValue();

    if (configHasCertFile != configHasKeyFile)
        return std::unexpected{"Config entries 'ssl_cert_file' and 'ssl_key_file' must be set or unset together."};

    if (not configHasCertFile)
        return std::nullopt;

    auto const certFilename = config.getValue("ssl_cert_file").asString();
    auto const keyFilename = config.getValue("ssl_key_file").asString();

    return impl::makeServerSslContext(certFilename, keyFilename);
}
}  // namespace web
