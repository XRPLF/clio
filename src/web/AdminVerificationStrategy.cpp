#include "web/AdminVerificationStrategy.hpp"

#include "util/JsonUtils.hpp"
#include "util/Shasum.hpp"
#include "util/config/ConfigDefinition.hpp"

#include <boost/beast/http/field.hpp>

#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace web {

bool
IPAdminVerificationStrategy::isAdmin(RequestHeader const&, std::string_view ip) const
{
    return ip == "127.0.0.1";
}

PasswordAdminVerificationStrategy::PasswordAdminVerificationStrategy(std::string const& password)
    : passwordSha256_(util::toUpper(util::sha256sumString(password)))
{
}

bool
PasswordAdminVerificationStrategy::isAdmin(RequestHeader const& request, std::string_view) const
{
    auto it = request.find(boost::beast::http::field::authorization);
    if (it == request.end()) {
        // No Authorization header
        return false;
    }
    auto userAuth = it->value();
    if (!userAuth.starts_with(kPASSWORD_PREFIX)) {
        // Invalid Authorization header
        return false;
    }

    userAuth.remove_prefix(kPASSWORD_PREFIX.size());
    return passwordSha256_ == util::toUpper(userAuth);
}

std::shared_ptr<AdminVerificationStrategy>
makeAdminVerificationStrategy(std::optional<std::string> password)
{
    if (password.has_value()) {
        return std::make_shared<PasswordAdminVerificationStrategy>(std::move(*password));
    }
    return std::make_shared<IPAdminVerificationStrategy>();
}

std::expected<std::shared_ptr<AdminVerificationStrategy>, std::string>
makeAdminVerificationStrategy(util::config::ClioConfigDefinition const& config)
{
    auto adminPassword = config.maybeValue<std::string>("server.admin_password");
    auto const localAdmin = config.maybeValue<bool>("server.local_admin");

    if (adminPassword.has_value() and localAdmin.has_value() and *localAdmin) {
        return std::unexpected{
            "Admin config error: 'local_admin' and admin_password can not be set together."
        };
    }

    if (localAdmin.has_value() and !*localAdmin and !adminPassword.has_value()) {
        return std::unexpected{
            "Admin config error: either 'local_admin' should be enabled or 'admin_password' must "
            "be specified."
        };
    }

    return makeAdminVerificationStrategy(std::move(adminPassword));
}

}  // namespace web
