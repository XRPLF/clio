#pragma once

#include "rpc/common/APIVersion.hpp"
#include "util/config/ObjectView.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/object.hpp>

#include <cstdint>
#include <expected>
#include <string>

namespace rpc::impl {

class ProductionAPIVersionParser : public APIVersionParser {
    util::Logger log_{"RPC"};

    uint32_t defaultVersion_;
    uint32_t minVersion_;
    uint32_t maxVersion_;

public:
    ProductionAPIVersionParser(
        uint32_t defaultVersion = kApiVersionDefault,
        uint32_t minVersion = kApiVersionMin,
        uint32_t maxVersion = kApiVersionMax
    );

    ProductionAPIVersionParser(util::config::ObjectView const& config);

    [[nodiscard]] std::expected<uint32_t, std::string>
    parse(boost::json::object const& request) const override;

    [[nodiscard]] uint32_t
    getDefaultVersion() const
    {
        return defaultVersion_;
    }

    [[nodiscard]] uint32_t
    getMinVersion() const
    {
        return minVersion_;
    }

    [[nodiscard]] uint32_t
    getMaxVersion() const
    {
        return maxVersion_;
    }
};

}  // namespace rpc::impl
