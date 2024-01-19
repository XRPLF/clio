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

#include "util/requests/impl/TcpStreamData.h"

#include "util/Expected.h"
#include "util/requests/Types.h"

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/beast/core/error.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <utility>

namespace util::requests::impl {

namespace asio = boost::asio;
namespace ssl = asio::ssl;

namespace {

// Taken from https://go.dev/src/crypto/x509/root_linux.go
constexpr std::array CERT_FILE_PATHS{
    "/etc/ssl/certs/ca-certificates.crt",                 // Debian/Ubuntu/Gentoo etc.
    "/etc/pki/tls/certs/ca-bundle.crt",                   // Fedora/RHEL 6
    "/etc/ssl/ca-bundle.pem",                             // OpenSUSE
    "/etc/pki/tls/cacert.pem",                            // OpenELEC
    "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",  // CentOS/RHEL 7
    "/etc/ssl/cert.pem",                                  // Alpine Linux
    "/etc/ssl/certs",                                     // SLES10/SLES11, https://golang.org/issue/12139
    "/etc/pki/tls/certs",                                 // Fedora/RHEL
    "/system/etc/security/cacerts",                       // Android
};

Expected<std::string, RequestError>
getRootCertificate()
{
    for (auto const& path : CERT_FILE_PATHS) {
        if (std::filesystem::exists(path)) {
            std::ifstream fileStream{path, std::ios::in};
            if (not fileStream.is_open()) {
                continue;
            }
            std::stringstream buffer;
            buffer << fileStream.rdbuf();
            return std::move(buffer).str();
        }
    }
    return Unexpected{RequestError{"SSL setup failed: could not find root certificate"}};
}

Expected<ssl::context, RequestError>
makeSslContext()
{
    ssl::context context{ssl::context::tlsv13_client};
    context.set_verify_mode(ssl::verify_peer);
    auto const rootCertificate = getRootCertificate();
    if (not rootCertificate.has_value()) {
    }
    context.add_certificate_authority(asio::buffer(rootCertificate->data(), rootCertificate->size()));
    return context;
}

}  // namespace

TcpStreamData::TcpStreamData(asio::yield_context yield) : stream(asio::get_associated_executor(yield))
{
}

boost::beast::error_code
TcpStreamData::doHandshake(boost::asio::yield_context)
{
    return {};
}

SslTcpStreamData::SslTcpStreamData(ssl::context sslContext, boost::asio::yield_context yield)
    : sslContext_(std::move(sslContext)), stream(asio::get_associated_executor(yield), sslContext_)
{
}

Expected<SslTcpStreamData, RequestError>
SslTcpStreamData::create(boost::asio::yield_context yield)
{
    auto sslContext = makeSslContext();
    if (not sslContext.has_value()) {
        return Unexpected{std::move(sslContext.error())};
    }
    return SslTcpStreamData{std::move(sslContext.value()), yield};
}

boost::beast::error_code
SslTcpStreamData::doHandshake(boost::asio::yield_context yield)
{
    boost::beast::error_code errorCode;
    stream.async_handshake(ssl::stream_base::client, yield[errorCode]);
    return errorCode;
}

}  // namespace util::requests::impl
