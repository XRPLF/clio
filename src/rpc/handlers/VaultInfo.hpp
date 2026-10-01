#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/vault_info/Types.hpp>

#include <cstdint>
#include <memory>

namespace rpc {

/**
 * @brief The vault_info command retrieves information about a vault, currency, shares etc.
 */
class VaultInfoHandler : public rpc::HandlerFor<rpc::spec::handlers::vault_info::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief Construct a new VaultInfo object
     *
     * @param sharedPtrBackend The backend to use
     */
    VaultInfoHandler(std::shared_ptr<BackendInterface> sharedPtrBackend);

    /**
     * @brief A struct to hold the output data for the command
     */
    struct Output {
        boost::json::value vault;
        uint32_t ledgerIndex{};
        bool validated = true;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Process the VaultInfo command
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
     * @param jv The JSON object to convert to
     * @param output The output to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);
};

}  // namespace rpc
