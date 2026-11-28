#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/account_offers/Types.hpp>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STLedgerEntry.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

/**
 * @brief The account_offers method retrieves a list of offers made by a given account.
 *
 * For more details see: https://xrpl.org/account_offers.html
 */
class AccountOffersHandler : public rpc::HandlerFor<rpc::spec::handlers::account_offers::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief Minimum accepted value for the `limit` request field
     */
    static constexpr auto kLimitMin = rpc::spec::handlers::account_offers::kLimitMin;
    /**
     * @brief Maximum accepted value for the `limit` request field
     */
    static constexpr auto kLimitMax = rpc::spec::handlers::account_offers::kLimitMax;
    /**
     * @brief Value of `limit` used when the request does not specify one
     */
    static constexpr auto kLimitDefault = rpc::spec::handlers::account_offers::kLimitDefault;

    /**
     * @brief A struct to hold data for one offer response
     */
    struct Offer {
        uint32_t flags{};          ///< Flags set on the offer
        uint32_t seq{};            ///< Sequence number of the transaction that created the offer
        xrpl::STAmount takerGets;  ///< Amount the offer creator is selling
        xrpl::STAmount takerPays;  ///< Amount the offer creator wants in return
        std::string quality;       ///< Exchange rate of the offer (takerPays / takerGets)
        std::optional<uint32_t> expiration;  ///< Time after which the offer is no longer active
    };

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string account;                ///< The account whose offers are returned
        std::string ledgerHash;             ///< Hash of the ledger used for the request
        uint32_t ledgerIndex{};             ///< Sequence of the ledger used for the request
        std::vector<Offer> offers;          ///< Offers owned by the account
        std::optional<std::string> marker;  ///< Pagination marker to resume from, if more results
        // validated should be sent via framework
        bool validated = true;  ///< Whether the ledger is validated
    };

    /**
     * @brief The result type of the command: the Output on success or an error Status
     */
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new AccountOffersHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    AccountOffersHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the AccountOffers command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const;

private:
    static void
    addOffer(std::vector<Offer>& offers, xrpl::SLE const& offerSle);

    /**
     * @brief Convert the Output to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param output The output to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);

    /**
     * @brief Convert the Offer to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param offer The offer to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Offer const& offer);
};
}  // namespace rpc
