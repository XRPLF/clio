#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/get_aggregate_price/Types.hpp>
#include <xrpl/basics/Number.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STObject.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

/**
 *@brief The get_aggregate_price method.
 */
class GetAggregatePriceHandler
    : public rpc::HandlerFor<rpc::spec::handlers::get_aggregate_price::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief A struct to hold the statistics
     */
    struct Stats {
        xrpl::STAmount avg{};  // NOLINT(readability-redundant-member-init)
        // standard deviation
        xrpl::Number sd{};  // NOLINT(readability-redundant-member-init)
        uint32_t size{0};
    };

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        uint32_t time;
        Stats extireStats{};
        std::optional<Stats> trimStats;
        std::string ledgerHash;
        uint32_t ledgerIndex;
        std::string median;
        bool validated = true;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new GetAggregatePrice handler object
     *
     * @param sharedPtrBackend The backend to use
     */
    GetAggregatePriceHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the GetAggregatePrice command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const;

private:
    /**
     * @brief Calls callback on the oracle ledger entry
     *
     * If the oracle entry does not contains the price pair, search up to three previous metadata
     * objects. Stops early if the callback returns true.
     */
    void
    tracebackOracleObject(
        boost::asio::yield_context yield,
        xrpl::STObject const& oracleObject,
        std::function<bool(xrpl::STObject const&)> const& callback
    ) const;

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
