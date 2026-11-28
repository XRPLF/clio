#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/nft_offers_common/Types.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Keylet.h>
#include <xrpl/protocol/STLedgerEntry.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

/**
 * @brief Contains common functionality for handling the `nft_offers` command
 */
class NFTOffersHandlerBase : public rpc::HandlerFor<rpc::spec::handlers::nft_offers_common::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /** @brief Minimum allowed value of the `limit` parameter */
    static constexpr auto kLimitMin = rpc::spec::handlers::nft_offers_common::kLimitMin;
    /** @brief Maximum allowed value of the `limit` parameter */
    static constexpr auto kLimitMax = rpc::spec::handlers::nft_offers_common::kLimitMax;
    /** @brief Default value of the `limit` parameter */
    static constexpr auto kLimitDefault = rpc::spec::handlers::nft_offers_common::kLimitDefault;

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string nftID;              ///< ID of the NFT the offers are for
        std::vector<xrpl::SLE> offers;  ///< NFT offer ledger objects

        // validated should be sent via framework
        bool validated = true;              ///< Whether the ledger is validated
        std::optional<uint32_t> limit;      ///< Limit used, set only if more offers remain
        std::optional<std::string> marker;  ///< Key of the next page's first offer, if any
    };

    /**
     * @brief The result type of the command: either the Output or an error status
     */
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new NFTOffersHandlerBase object
     *
     * @param sharedPtrBackend The backend to use
     */
    NFTOffersHandlerBase(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

protected:
    /**
     * @brief Iterate the NFT offer directory
     *
     * @param input The input data for the command
     * @param tokenID The tokenID of the NFT
     * @param directory The directory to iterate
     * @param yield The coroutine context
     * @return The result of the iteration
     */
    [[nodiscard]] Result
    iterateOfferDirectory(
        Input input,
        xrpl::uint256 const& tokenID,
        xrpl::Keylet const& directory,
        boost::asio::yield_context yield
    ) const;

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
