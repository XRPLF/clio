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

#pragma once

#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace rpc {

/**
 * @brief Contains common functionality for handling the `server_info` command
 */
class FeatureHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<data::AmendmentCenterInterface const> amendmentCenter_;

public:
    /**
     * @brief A struct to hold the input data for the command
     */
    struct Input {
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        std::optional<std::string> feature;
    };

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
            bool enabled = false;
        };

        std::map<std::string, Feature> features;
        std::string ledgerHash;
        uint32_t ledgerIndex{};

        // validated should be sent via framework
        bool validated = true;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new FeatureHandler object
     *
     * @param backend The backend to use
     * @param amendmentCenter The amendment center to use
     */
    FeatureHandler(
        std::shared_ptr<BackendInterface> const& backend,
        std::shared_ptr<data::AmendmentCenterInterface const> const& amendmentCenter
    )
        : sharedPtrBackend_(backend), amendmentCenter_(amendmentCenter)
    {
    }

    /**
     * @brief Returns the API specification for the command
     *
     * @param apiVersion The api version to return the spec for
     * @return The spec for the given apiVersion
     */
    static RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion);

    /**
     * @brief Process the Feature command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    Result
    process(Input input, Context const& ctx) const;  // NOLINT(readability-convert-member-functions-to-static)

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

    /**
     * @brief Convert a JSON object to Input type
     *
     * @param jv The JSON object to convert
     * @return Input parsed from the JSON object
     */
    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);
};

}  // namespace rpc
