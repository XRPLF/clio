#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/Types.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/HandlerFor.hpp>
#include <rpcspec/handlers/nft_history/Types.hpp>

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

/**
 * @brief The nft_history command asks the Clio server for past transaction metadata for the NFT
 * being queried.
 *
 * For more details see: https://xrpl.org/nft_history.html#nft_history
 */
class NFTHistoryHandler : public rpc::spec::HandlerFor<rpc::spec::handlers::nft_history::Input> {
    util::Logger log_{"RPC"};
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    static constexpr auto kLimitMin = rpc::spec::handlers::nft_history::kLimitMin;
    static constexpr auto kLimitMax = rpc::spec::handlers::nft_history::kLimitMax;
    static constexpr auto kLimitDefault = rpc::spec::handlers::nft_history::kLimitDefault;

    /**
     * @brief A struct to hold the marker data
     */
    using Marker = rpc::spec::handlers::nft_history::Marker;

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string nftID;
        uint32_t ledgerIndexMin{0};
        uint32_t ledgerIndexMax{0};
        std::optional<uint32_t> limit;
        std::optional<Marker> marker;
        // TODO: use a better type than json
        boost::json::array transactions;
        // validated should be sent via framework
        bool validated = true;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new NFTHistoryHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    NFTHistoryHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the NFTHistory command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const;

private:
    /**
     * @brief Convert the Output to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param output The output to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);
};

}  // namespace rpc

namespace rpc::spec::handlers::nft_history {

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Marker const& marker);

}  // namespace rpc::spec::handlers::nft_history
