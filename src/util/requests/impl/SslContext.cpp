#include "util/requests/impl/SslContext.hpp"

#include "util/requests/Types.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/lexical_cast.hpp>
#include <fmt/format.h>
#include <openssl/err.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace util::requests::impl {

namespace asio = boost::asio;
namespace ssl = asio::ssl;

namespace {

// Taken from https://go.dev/src/crypto/x509/root_linux.go

constexpr std::array kCertFilePaths{
    "/etc/ssl/certs/ca-certificates.crt",                 // Debian/Ubuntu/Gentoo etc.
    "/etc/pki/tls/certs/ca-bundle.crt",                   // Fedora/RHEL 6
    "/etc/ssl/ca-bundle.pem",                             // OpenSUSE
    "/etc/pki/tls/cacert.pem",                            // OpenELEC
    "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",  // CentOS/RHEL 7
    "/etc/ssl/cert.pem",                                  // Alpine Linux
    "/etc/ssl/certs",                // SLES10/SLES11, https://golang.org/issue/12139
    "/etc/pki/tls/certs",            // Fedora/RHEL
    "/system/etc/security/cacerts",  // Android
};

std::optional<std::string>
readCertificateFile(std::filesystem::path const& path)
{
    if (not std::filesystem::exists(path))
        return std::nullopt;

    std::ifstream const fileStream{path, std::ios::in};
    if (not fileStream.is_open())
        return std::nullopt;

    std::stringstream buffer;
    buffer << fileStream.rdbuf();

    return std::move(buffer).str();
}

std::optional<std::string>
getRootCertificate()
{
    // Honor the OpenSSL-standard SSL_CERT_FILE environment variable first. Some
    // environments (e.g. the Nix-based CI/runtime image) point it at their CA
    // bundle instead of installing certificates at the well-known system paths.
    if (char const* const certFile = std::getenv("SSL_CERT_FILE"); certFile != nullptr) {
        if (auto contents = readCertificateFile(certFile); contents.has_value())
            return contents;
    }

    for (auto const& path : kCertFilePaths) {
        if (auto contents = readCertificateFile(path); contents.has_value())
            return contents;
    }

    return std::nullopt;
}

std::expected<ssl::context, RequestError>&
cachedClientSslContext()
{
    static std::expected<ssl::context, RequestError> context =
        makeClientSslContext(getRootCertificate());
    return context;
}

}  // namespace

std::expected<ssl::context, RequestError>
makeClientSslContext(std::optional<std::string> const& rootCertificate)
{
    if (not rootCertificate.has_value())
        return std::unexpected{RequestError{"SSL setup failed: could not find root certificate"}};

    ssl::context context{ssl::context::tls_client};
    context.set_verify_mode(ssl::verify_peer);
    context.add_certificate_authority(  //
        asio::buffer(rootCertificate->data(), rootCertificate->size())
    );

    return context;
}

std::expected<void, std::string>
initClientSslContext()
{
    auto const& context = cachedClientSslContext();
    if (not context.has_value())
        return std::unexpected{context.error().message()};

    return {};
}

ssl::context&
getClientSslContext()
{
    // initClientSslContext() called during startup
    return cachedClientSslContext().value();
}

std::optional<std::string>
sslErrorToString(boost::beast::error_code const& error)
{
    if (error.category() != boost::asio::error::get_ssl_category())
        return std::nullopt;

    std::string errorString = fmt::format(
        "({},{}) ",
        boost::lexical_cast<std::string>(ERR_GET_LIB(error.value())),
        boost::lexical_cast<std::string>(ERR_GET_REASON(error.value()))
    );

    static constexpr size_t kBufferSize = 128;
    char buf[kBufferSize];
    ::ERR_error_string_n(error.value(), buf, sizeof(buf));
    errorString += buf;

    return errorString;
}

}  // namespace util::requests::impl
