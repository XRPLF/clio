#pragma once
#include "data/BackendInterface.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace rpc {

/**
 * @brief The ledger_index method fetches the latest closed ledger before the given date.
 *
 */
class LedgerIndexHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    static constexpr auto kDATE_FORMAT = "%Y-%m-%dT%TZ";

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        uint32_t ledgerIndex{};
        std::string ledgerHash;
        std::string closeTimeIso;
    };

    /**
     * @brief A struct to hold the input data for the command
     */
    struct Input {
        std::optional<std::string> date;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new LedgerIndexHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    LedgerIndexHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : sharedPtrBackend_(std::move(sharedPtrBackend))
    {
    }

    /**
     * @brief Returns the API specification for the command
     *
     * @param apiVersion The api version to return the spec for
     * @return The spec for the given apiVersion
     */
    static RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion)
    {
        static auto const kRPC_SPEC = RpcSpec{
            {JS(date),
             validation::Type<std::string>{},
             validation::TimeFormatValidator{kDATE_FORMAT}},
        };
        return kRPC_SPEC;
    }

    /**
     * @brief Process the LedgerIndex command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    Result
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

    /**
     * @brief Convert a JSON object to Input type
     *
     * @param jv The JSON object to convert
     * @return Input parsed from the JSON object
     */
    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);
};

}  // namespace rpc
