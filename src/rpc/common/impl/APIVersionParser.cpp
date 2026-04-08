#include "rpc/common/impl/APIVersionParser.hpp"

#include "util/JsonUtils.hpp"
#include "util/config/ObjectView.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/object.hpp>
#include <fmt/format.h>

#include <cstdint>
#include <expected>
#include <string>

using namespace std;

namespace rpc::impl {

ProductionAPIVersionParser::ProductionAPIVersionParser(
    uint32_t defaultVersion,
    uint32_t minVersion,
    uint32_t maxVersion
)
    : defaultVersion_{defaultVersion}, minVersion_{minVersion}, maxVersion_{maxVersion}
{
    LOG(log_.info()) << "API version settings: [min = " << minVersion_ << "; max = " << maxVersion_
                     << "; default = " << defaultVersion_ << "]";
}

ProductionAPIVersionParser::ProductionAPIVersionParser(util::config::ObjectView const& config)
    : ProductionAPIVersionParser(
          config.get<uint32_t>("default"),
          config.get<uint32_t>("min"),
          config.get<uint32_t>("max")
      )
{
}

std::expected<uint32_t, std::string>
ProductionAPIVersionParser::parse(boost::json::object const& request) const
{
    using Error = std::unexpected<std::string>;

    if (request.contains("api_version")) {
        if (!request.at("api_version").is_int64())
            return Error{"API version must be an integer"};

        auto const version = util::integralValueAs<uint32_t>(request.at("api_version"));

        if (version > maxVersion_) {
            return Error{fmt::format(
                "Requested API version is higher than maximum supported ({})", maxVersion_
            )};
        }

        if (version < minVersion_) {
            return Error{fmt::format(
                "Requested API version is lower than minimum supported ({})", minVersion_
            )};
        }

        return version;
    }

    return defaultVersion_;
}

}  // namespace rpc::impl
