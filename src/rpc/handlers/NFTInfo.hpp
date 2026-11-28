#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/nft_info/Types.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace rpc {

/**
 * @brief The nft_info command asks the Clio server for information about the NFT being queried.
 *
 * For more details see: https://xrpl.org/nft_info.html
 */
class NFTInfoHandler : public rpc::HandlerFor<rpc::spec::handlers::nft_info::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string nftID;     ///< ID of the NFT
        uint32_t ledgerIndex;  ///< Sequence of the ledger used
        std::string owner;     ///< Address of the current owner
        bool isBurned;         ///< Whether the NFT has been burned
        uint32_t flags;        ///< Flags set on the NFT
        uint32_t transferFee;  ///< Fee on secondary sales, in 1/100000 units
        std::string issuer;    ///< Address of the issuer
        uint32_t taxon;        ///< Taxon of the NFT
        /** @brief Serial number of the NFT within its issuer */
        uint32_t serial;  // TODO: documented as 'nft_sequence' atm.
                          // https://github.com/XRPLF/xrpl-dev-portal/issues/1841
        std::string uri;  ///< Hex-encoded URI of the NFT

        // validated should be sent via framework
        bool validated = true;  ///< Whether the ledger is validated
    };

    /**
     * @brief The result type of the command: either the Output or an error status
     */
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new NFTInfoHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    NFTInfoHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the NFTInfo command
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
