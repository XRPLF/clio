#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/account_nfts/Types.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace rpc {

/**
 * @brief The account_nfts method returns a list of NFToken objects for the specified account.
 *
 * For more details see: https://xrpl.org/account_nfts.html
 */
class AccountNFTsHandler : public rpc::HandlerFor<rpc::spec::handlers::account_nfts::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    static constexpr auto kLimitMin = rpc::spec::handlers::account_nfts::kLimitMin;
    static constexpr auto kLimitMax = rpc::spec::handlers::account_nfts::kLimitMax;
    static constexpr auto kLimitDefault = rpc::spec::handlers::account_nfts::kLimitDefault;

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string account;
        std::string ledgerHash;
        uint32_t ledgerIndex;
        // TODO: use better type than json
        boost::json::array nfts;
        uint32_t limit;
        std::optional<std::string> marker;
        bool validated = true;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new AccountNFTsHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    AccountNFTsHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Process the AccountNFTs command
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
