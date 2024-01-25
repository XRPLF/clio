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

#include "util/requests/impl/SslContext.h"

#include "util/Expected.h"
#include "util/requests/Types.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/verify_mode.hpp>

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

}  // namespace

Expected<boost::asio::ssl::context, RequestError>
makeSslContext()
{
    ssl::context context{ssl::context::sslv23_client};
    context.set_verify_mode(ssl::verify_peer);
    auto const rootCertificate = getRootCertificate();
    if (not rootCertificate.has_value()) {
        return Unexpected{rootCertificate.error()};
    }
    context.add_certificate_authority(asio::buffer(rootCertificate->data(), rootCertificate->size()));
    return context;
}

}  // namespace util::requests::impl
