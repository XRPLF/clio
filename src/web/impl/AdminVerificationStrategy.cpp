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

#include <web/impl/AdminVerificationStrategy.h>

#include <ripple/protocol/digest.h>

namespace web::detail {

bool
IPAdminVerificationStrategy::isAdmin(RequestType const&, std::string_view ip) const
{
    return ip == "127.0.0.1";
}

PasswordAdminVerificationStrategy::PasswordAdminVerificationStrategy(std::string const& password)
{
    ripple::sha256_hasher hasher;
    hasher(password.data(), password.size());
    auto const d = static_cast<ripple::sha256_hasher::result_type>(hasher);
    ripple::uint256 sha256;
    std::memcpy(sha256.data(), d.data(), d.size());
    passwordSha256_ = ripple::to_string(sha256);
    // make sure it's uppercase
    std::transform(passwordSha256_.begin(), passwordSha256_.end(), passwordSha256_.begin(), ::toupper);
}

bool
PasswordAdminVerificationStrategy::isAdmin(RequestType const& request, std::string_view) const
{
    auto it = request.find(boost::beast::http::field::authorization);
    if (it == request.end()) {
        // No Authorization header
        return false;
    }
    std::string userAuth(it->value());
    std::transform(userAuth.begin(), userAuth.end(), userAuth.begin(), ::toupper);
    return passwordSha256_ == userAuth;
}

std::unique_ptr<AdminVerificationStrategy>
make_AdminVerificationStrategy(std::optional<std::string> password)
{
    if (password.has_value()) {
        return std::make_unique<PasswordAdminVerificationStrategy>(std::move(*password));
    }
    return std::make_unique<IPAdminVerificationStrategy>();
}

}  // namespace web::detail
