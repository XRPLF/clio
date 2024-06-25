//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/MetaProcessors.hpp"
#include "rpc/common/Modifiers.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace rpc {

/**
 * @brief The book_offers method retrieves a list of Offers between two currencies, also known as an order book.
 *
 * For more details see: https://xrpl.org/book_offers.html
 */
class BookOffersHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    static auto constexpr LIMIT_MIN = 1;
    static auto constexpr LIMIT_MAX = 100;
    static auto constexpr LIMIT_DEFAULT = 60;

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string ledgerHash;
        uint32_t ledgerIndex;
        boost::json::array offers;
        bool validated = true;
    };

    /**
     * @brief A struct to hold the input data for the command
     *
     * @note The taker is not really used in both Clio and `rippled`, both of them return all the offers regardless of
     * the funding status
     */
    struct Input {
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        uint32_t limit = LIMIT_DEFAULT;
        std::optional<ripple::AccountID> taker;
        ripple::Currency paysCurrency;
        ripple::Currency getsCurrency;
        // accountID will be filled by input converter, if no issuer is given, will use XRP issuer
        ripple::AccountID paysID = ripple::xrpAccount();
        ripple::AccountID getsID = ripple::xrpAccount();
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new BookOffersHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    BookOffersHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend) : sharedPtrBackend_(sharedPtrBackend)
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
        static auto const rpcSpec = RpcSpec{
            {JS(taker_gets),
             validation::Required{},
             validation::Type<boost::json::object>{},
             meta::Section{
                 {JS(currency),
                  validation::Required{},
                  meta::WithCustomError{validation::CurrencyValidator, Status(RippledError::rpcDST_AMT_MALFORMED)}},
                 {JS(issuer),
                  meta::WithCustomError{validation::IssuerValidator, Status(RippledError::rpcDST_ISR_MALFORMED)}}
             }},
            {JS(taker_pays),
             validation::Required{},
             validation::Type<boost::json::object>{},
             meta::Section{
                 {JS(currency),
                  validation::Required{},
                  meta::WithCustomError{validation::CurrencyValidator, Status(RippledError::rpcSRC_CUR_MALFORMED)}},
                 {JS(issuer),
                  meta::WithCustomError{validation::IssuerValidator, Status(RippledError::rpcSRC_ISR_MALFORMED)}}
             }},
            // return INVALID_PARAMS if account format is wrong for "taker"
            {JS(taker),
             meta::WithCustomError{
                 validation::AccountValidator, Status(RippledError::rpcINVALID_PARAMS, "Invalid field 'taker'.")
             }},
            {JS(limit),
             validation::Type<uint32_t>{},
             validation::Min(1u),
             modifiers::Clamp<int32_t>{LIMIT_MIN, LIMIT_MAX}},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
        };

        return rpcSpec;
    }

    /**
     * @brief Process the BookOffers command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    Result
    process(Input input, Context const& ctx) const;

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
