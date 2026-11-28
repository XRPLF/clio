#pragma once

#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/book_offers/Types.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace rpc {

/**
 * @brief The book_offers method retrieves a list of Offers between two currencies, also known as an
 * order book.
 *
 * For more details see: https://xrpl.org/book_offers.html
 */
class BookOffersHandler : public rpc::HandlerFor<rpc::spec::handlers::book_offers::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<data::AmendmentCenterInterface const> amendmentCenter_;

public:
    /** @brief Minimum allowed value of the `limit` parameter */
    static constexpr auto kLimitMin = rpc::spec::handlers::book_offers::kLimitMin;
    /** @brief Maximum allowed value of the `limit` parameter */
    static constexpr auto kLimitMax = rpc::spec::handlers::book_offers::kLimitMax;
    /** @brief Default value of the `limit` parameter */
    static constexpr auto kLimitDefault = rpc::spec::handlers::book_offers::kLimitDefault;

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string ledgerHash;     ///< Hash of the ledger the offers were read from
        uint32_t ledgerIndex;       ///< Sequence of the ledger the offers were read from
        boost::json::array offers;  ///< Offers in the order book, as JSON objects
        bool validated = true;      ///< Whether the ledger is validated
    };

    /**
     * @brief The result type of the command: either the Output or an error status
     */
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new BookOffersHandler object
     *
     * @param sharedPtrBackend The backend to use
     * @param amendmentCenter The amendmentCenter to use
     */
    BookOffersHandler(
        std::shared_ptr<BackendInterface> sharedPtrBackend,
        std::shared_ptr<data::AmendmentCenterInterface const> const& amendmentCenter
    )
        : sharedPtrBackend_(std::move(sharedPtrBackend)), amendmentCenter_{amendmentCenter}
    {
    }

    /**
     * @brief Process the BookOffers command
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
