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

#include "web/impl/ServerSslContext.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <fmt/compile.h>
#include <fmt/core.h>

#include <expected>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace web::impl {

namespace {

std::optional<std::string>
readFile(std::string const& path)
{
    std::ifstream const file(path, std::ios::in | std::ios::binary);
    if (!file)
        return {};

    std::stringstream contents;
    contents << file.rdbuf();
    return std::move(contents).str();
}

}  // namespace

std::expected<boost::asio::ssl::context, std::string>
makeServerSslContext(std::string const& certFilePath, std::string const& keyFilePath)
{
    auto const certContent = readFile(certFilePath);
    if (!certContent)
        return std::unexpected{"Can't read SSL certificate: " + certFilePath};

    auto const keyContent = readFile(keyFilePath);
    if (!keyContent)
        return std::unexpected{"Can't read SSL key: " + keyFilePath};

    using namespace boost::asio;

    ssl::context ctx{ssl::context::tls_server};
    ctx.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2);

    try {
        ctx.use_certificate_chain(buffer(certContent->data(), certContent->size()));
        ctx.use_private_key(buffer(keyContent->data(), keyContent->size()), ssl::context::file_format::pem);
    } catch (...) {
        return std::unexpected{
            fmt::format("Error loading SSL certificate ({}) or SSL key ({}).", certFilePath, keyFilePath)
        };
    }

    return ctx;
}

}  // namespace web::impl
