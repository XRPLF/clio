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

#pragma once

#include "util/config/Config.hpp"

#include <boost/beast/http.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace web::impl {

class AdminVerificationStrategy {
public:
    using RequestType = boost::beast::http::request<boost::beast::http::string_body>;
    virtual ~AdminVerificationStrategy() = default;

    /**
     * @brief Checks whether request is from a host that is considered authorized as admin.
     *
     * @param request The http request from the client
     * @param ip The ip addr of the client
     * @return true if authorized; false otherwise
     */
    virtual bool
    isAdmin(RequestType const& request, std::string_view ip) const = 0;
};

class IPAdminVerificationStrategy : public AdminVerificationStrategy {
public:
    /**
     * @brief Checks whether request is from a host that is considered authorized as admin
     * by checking the ip address.
     *
     * @param ip The ip addr of the client
     * @return true if authorized; false otherwise
     */
    bool
    isAdmin(RequestType const&, std::string_view ip) const override;
};

class PasswordAdminVerificationStrategy : public AdminVerificationStrategy {
private:
    std::string passwordSha256_;

public:
    static constexpr std::string_view passwordPrefix = "Password ";

    PasswordAdminVerificationStrategy(std::string const& password);

    /**
     * @brief Checks whether request is from a host that is considered authorized as admin using
     * the password (if any) from the request.
     *
     * @param request The request from a host
     * @return true if the password from request matches admin password from config
     */
    bool
    isAdmin(RequestType const& request, std::string_view) const override;
};

std::shared_ptr<AdminVerificationStrategy>
make_AdminVerificationStrategy(std::optional<std::string> password);

std::expected<std::shared_ptr<AdminVerificationStrategy>, std::string>
make_AdminVerificationStrategy(util::Config const& serverConfig);

}  // namespace web::impl
