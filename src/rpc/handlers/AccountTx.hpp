#pragma once

#include "data/BackendInterface.hpp"
#include "etl/ETLServiceInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/account_tx/Types.hpp>

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

/**
 * @brief The account_tx method retrieves a list of transactions that involved the specified
 * account.
 *
 * For more details see: https://xrpl.org/account_tx.html
 */
class AccountTxHandler : public rpc::HandlerFor<rpc::spec::handlers::account_tx::Input> {
    util::Logger log_{"RPC"};
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<etl::ETLServiceInterface const> etl_;

public:
    /**
     * @brief Minimum accepted value for the `limit` request field
     */
    static constexpr auto kLimitMin = rpc::spec::handlers::account_tx::kLimitMin;
    /**
     * @brief Maximum accepted value for the `limit` request field
     */
    static constexpr auto kLimitMax = rpc::spec::handlers::account_tx::kLimitMax;
    /**
     * @brief Value of `limit` used when the request does not specify one
     */
    static constexpr auto kLimitDefault = rpc::spec::handlers::account_tx::kLimitDefault;

    /**
     * @brief Pagination marker identifying a transaction by ledger sequence and transaction index
     */
    using Marker = rpc::spec::handlers::account_tx::Marker;

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string account;            ///< The account whose transactions are returned
        uint32_t ledgerIndexMin{0};     ///< Lowest ledger sequence that was searched
        uint32_t ledgerIndexMax{0};     ///< Highest ledger sequence that was searched
        std::optional<uint32_t> limit;  ///< Maximum number of transactions requested
        std::optional<Marker> marker;   ///< Pagination marker to resume from, if more results
        // TODO: use a better type than json
        boost::json::array transactions;  ///< Transactions affecting the account
        // validated should be sent via framework
        bool validated = true;  ///< Whether the ledger range is validated
    };

    /**
     * @brief The result type of the command: the Output on success or an error Status
     */
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new AccountTxHandler object
     *
     * @param sharedPtrBackend The backend to use
     * @param etl The ETL service to use
     */
    AccountTxHandler(
        std::shared_ptr<BackendInterface> sharedPtrBackend,
        std::shared_ptr<etl::ETLServiceInterface const> const& etl
    )
        : sharedPtrBackend_(std::move(sharedPtrBackend)), etl_{etl}
    {
    }

    /**
     * @brief Process the AccountTx command
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

/**
 * @brief JSON conversions for the account_tx spec types used by AccountTxHandler.
 */
namespace rpc::spec::handlers::account_tx {

/**
 * @brief Convert the Marker to a JSON object
 *
 * @param [out] jv The JSON object to convert to
 * @param marker The marker to convert
 */
void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Marker const& marker);

}  // namespace rpc::spec::handlers::account_tx
