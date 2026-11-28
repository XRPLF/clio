#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/nfts_by_issuer/Types.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

/**
 * @brief Handler for the `nfts_by_issuer` command
 */
class NFTsByIssuerHandler : public rpc::HandlerFor<rpc::spec::handlers::nfts_by_issuer::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /** @brief Minimum allowed value of the `limit` parameter */
    static constexpr auto kLimitMin = rpc::spec::handlers::nfts_by_issuer::kLimitMin;
    /** @brief Maximum allowed value of the `limit` parameter */
    static constexpr auto kLimitMax = rpc::spec::handlers::nfts_by_issuer::kLimitMax;
    /** @brief Default value of the `limit` parameter */
    static constexpr auto kLimitDefault = rpc::spec::handlers::nfts_by_issuer::kLimitDefault;

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        boost::json::array nfts;            ///< NFTs minted by the issuer, as JSON
        uint32_t ledgerIndex;               ///< Sequence of the ledger used
        std::string issuer;                 ///< Address of the issuer
        bool validated = true;              ///< Whether the ledger is validated
        std::optional<uint32_t> nftTaxon;   ///< Taxon filter from the request, if any
        uint32_t limit;                     ///< Limit used for this page
        std::optional<std::string> marker;  ///< Marker to fetch the next page, if any
    };

    /**
     * @brief The result type of the command: either the Output or an error status
     */
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new NFTsByIssuerHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    NFTsByIssuerHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the NFTsByIssuer command
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
