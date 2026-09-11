#pragma once

#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/HandlerFor.hpp>
#include <rpcspec/handlers/feature/Types.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace rpc {

/**
 * @brief Contains common functionality for handling the `server_info` command
 */
class FeatureHandler : public rpc::spec::HandlerFor<rpc::spec::handlers::feature::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<data::AmendmentCenterInterface const> amendmentCenter_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        /**
         * @brief Represents an amendment/feature
         */
        struct Feature {
            std::string name;
            std::string key;
            bool supported = false;
            bool enabled = false;
        };

        std::map<std::string, Feature> features;
        std::string ledgerHash;
        uint32_t ledgerIndex{};

        // when set to true, output will be inlined in `result` instead of wrapping as `features`
        // inside of `result`.
        bool inlineResult = false;

        // validated should be sent via framework
        bool validated = true;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new FeatureHandler object
     *
     * @param sharedPtrBackend The backend to use
     * @param amendmentCenter The amendment center to use
     */
    FeatureHandler(
        std::shared_ptr<BackendInterface> sharedPtrBackend,
        std::shared_ptr<data::AmendmentCenterInterface const> const& amendmentCenter
    )
        : sharedPtrBackend_(std::move(sharedPtrBackend)), amendmentCenter_(amendmentCenter)
    {
    }

    /**
     * @brief Process the Feature command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    [[nodiscard]] Result
    process(
        Input const& input,
        Context const& ctx
    ) const;  // NOLINT(readability-convert-member-functions-to-static)

private:
    /**
     * @brief Convert the Output to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param output The output to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);

    /**
     * @brief Convert the Feature to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param feature The feature to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output::Feature const& feature);
};

}  // namespace rpc
