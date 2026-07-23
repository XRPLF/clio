#pragma once

#include "util/config/ConfigDefinition.hpp"

#include <boost/beast/http.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace web {

/**
 * @brief Interface for admin verification strategies.
 */
class AdminVerificationStrategy {
public:
    using RequestHeader = boost::beast::http::request<boost::beast::http::string_body>::header_type;
    virtual ~AdminVerificationStrategy() = default;

    /**
     * @brief Checks whether request is from a host that is considered authorized as admin.
     *
     * @param request The http request from the client
     * @param ip The ip addr of the client
     * @return true if authorized; false otherwise
     */
    [[nodiscard]] virtual bool
    isAdmin(RequestHeader const& request, std::string_view ip) const = 0;
};

/**
 * @brief Admin verification strategy that checks the ip address of the client.
 */
class IPAdminVerificationStrategy : public AdminVerificationStrategy {
public:
    /**
     * @brief Checks whether request is from a host that is considered authorized as admin
     * by checking the ip address.
     *
     * @param ip The ip addr of the client
     * @return true if authorized; false otherwise
     */
    [[nodiscard]] bool
    isAdmin(RequestHeader const&, std::string_view ip) const override;
};

/**
 * @brief Admin verification strategy that checks the password from the request header.
 */
class PasswordAdminVerificationStrategy : public AdminVerificationStrategy {
private:
    std::string passwordSha256_;

public:
    /**
     * @brief The prefix for the password in the request header.
     */
    static constexpr std::string_view kPasswordPrefix = "Password ";

    /**
     * @brief Construct a new PasswordAdminVerificationStrategy object
     *
     * @param password The password to check
     */
    PasswordAdminVerificationStrategy(std::string const& password);

    /**
     * @brief Checks whether request is from a host that is considered authorized as admin using
     * the password (if any) from the request.
     *
     * @param request The request from a host
     * @return true if the password from request matches admin password from config
     */
    [[nodiscard]] bool
    isAdmin(RequestHeader const& request, std::string_view) const override;
};

/**
 * @brief Factory function for creating an admin verification strategy.
 *
 * @param password The optional password to check.
 * @return Admin verification strategy. If password is provided, it will be
 * PasswordAdminVerificationStrategy. Otherwise, it will be IPAdminVerificationStrategy.
 */
std::shared_ptr<AdminVerificationStrategy>
makeAdminVerificationStrategy(std::optional<std::string> password);

/**
 * @brief Factory function for creating an admin verification strategy from server config.
 *
 * @param serverConfig The clio config.
 * @return Admin verification strategy according to the config or an error message.
 */
std::expected<std::shared_ptr<AdminVerificationStrategy>, std::string>
makeAdminVerificationStrategy(util::config::ClioConfigDefinition const& serverConfig);

}  // namespace web
