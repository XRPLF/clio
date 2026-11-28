#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/BookChangesHelper.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/book_changes/Types.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

/**
 * @brief BookChangesHandler returns the order book changes for a given ledger.
 *
 * This API is not documented in the rippled API documentation.
 */
class BookChangesHandler : public rpc::HandlerFor<rpc::spec::handlers::book_changes::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string ledgerHash;  ///< Hash of the ledger the changes were computed for
        uint32_t ledgerIndex{};  ///< Sequence of the ledger
        uint32_t ledgerTime{};   ///< Close time of the ledger, in seconds since the Ripple epoch
        /**
         * @brief Order book changes caused by the ledger's transactions
         */
        std::vector<BookChange> bookChanges;
        bool validated = true;  ///< Whether the ledger is validated
    };

    /**
     * @brief The result type of the command: either the Output or an error status
     */
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new BookChangesHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    BookChangesHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the BookChanges command
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
