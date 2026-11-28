#pragma once

#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/amm_info/Types.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

/**
 * @brief AMMInfoHandler returns information about AMM pools.
 *
 * For more info see: https://xrpl.org/amm_info.html
 */
class AMMInfoHandler : public rpc::HandlerFor<rpc::spec::handlers::amm_info::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<data::AmendmentCenterInterface const> amendmentCenter_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        // todo: use better type than json types
        boost::json::value amount1;        ///< Balance of the first asset in the AMM pool
        boost::json::value amount2;        ///< Balance of the second asset in the AMM pool
        boost::json::value lpToken;        ///< LP tokens of `account` if given, else total
        boost::json::array voteSlots;      ///< Current trading fee votes (account, fee, weight)
        boost::json::value auctionSlot;    ///< Current auction slot holder info; null if none
        std::uint16_t tradingFee = 0;      ///< Trading fee in units of 1/100,000
        std::string ammAccount;            ///< Address of the AMM's special account
        std::optional<bool> asset1Frozen;  ///< Whether the first asset is frozen; unset for XRP
        std::optional<bool> asset2Frozen;  ///< Whether the second asset is frozen; unset for XRP

        std::string ledgerHash;  ///< Hash of the ledger the info was taken from
        uint32_t ledgerIndex{};  ///< Sequence of the ledger the info was taken from
        bool validated = true;   ///< Whether the ledger is validated
    };

    /**
     * @brief The result type of the command: the Output on success or an error Status
     */
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new AMMInfoHandler object
     *
     * @param sharedPtrBackend The backend to use
     * @param amendmentCenter The amendmentCenter to use
     */
    AMMInfoHandler(
        std::shared_ptr<BackendInterface> sharedPtrBackend,
        std::shared_ptr<data::AmendmentCenterInterface const> const& amendmentCenter
    )
        : sharedPtrBackend_(std::move(sharedPtrBackend)), amendmentCenter_{amendmentCenter}
    {
    }

    /**
     * @brief Process the AMMInfo command
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
