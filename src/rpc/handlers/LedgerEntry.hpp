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
#include "rpc/common/Checkers.hpp"
#include "rpc/common/MetaProcessors.hpp"
#include "rpc/common/Modifiers.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"
#include "util/AccountUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/beast/core/LexicalCast.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol/tokens.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace rpc {

/**
 * @brief The ledger_entry method returns a single ledger object from the XRP Ledger in its raw format.
 *
 * For more details see: https://xrpl.org/ledger_entry.html
 */
class LedgerEntryHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string index;
        uint32_t ledgerIndex;
        std::string ledgerHash;
        std::optional<boost::json::object> node;
        std::optional<std::string> nodeBinary;
        std::optional<uint32_t> deletedLedgerIndex;
        bool validated = true;
    };

    /**
     * @brief A struct to hold the input data for the command
     */
    struct Input {
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        bool binary = false;
        // id of this ledger entry: 256 bits hex string
        std::optional<std::string> index;
        // index can be extracted from payment_channel, check, escrow, offer
        // etc, expectedType is used to save the type of index
        ripple::LedgerEntryType expectedType = ripple::ltANY;
        // account id to address account root object
        std::optional<std::string> accountRoot;
        // account id to address did object
        std::optional<std::string> did;
        // TODO: extract into custom objects, remove json from Input
        std::optional<boost::json::object> directory;
        std::optional<boost::json::object> offer;
        std::optional<boost::json::object> rippleStateAccount;
        std::optional<boost::json::object> escrow;
        std::optional<boost::json::object> depositPreauth;
        std::optional<boost::json::object> ticket;
        std::optional<boost::json::object> amm;
        std::optional<ripple::STXChainBridge> bridge;
        std::optional<std::string> bridgeAccount;
        std::optional<uint32_t> chainClaimId;
        std::optional<uint32_t> createAccountClaimId;
        std::optional<ripple::uint256> oracleNode;
        bool includeDeleted = false;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new LedgerEntryHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    LedgerEntryHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend) : sharedPtrBackend_(sharedPtrBackend)
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
        // Validator only works in this handler
        // The accounts array must have two different elements
        // Each element must be a valid address
        static auto const rippleStateAccountsCheck =
            validation::CustomValidator{[](boost::json::value const& value, std::string_view /* key */) -> MaybeError {
                if (!value.is_array() || value.as_array().size() != 2 || !value.as_array()[0].is_string() ||
                    !value.as_array()[1].is_string() ||
                    value.as_array()[0].as_string() == value.as_array()[1].as_string()) {
                    return Error{Status{RippledError::rpcINVALID_PARAMS, "malformedAccounts"}};
                }

                auto const id1 =
                    util::parseBase58Wrapper<ripple::AccountID>(boost::json::value_to<std::string>(value.as_array()[0])
                    );
                auto const id2 =
                    util::parseBase58Wrapper<ripple::AccountID>(boost::json::value_to<std::string>(value.as_array()[1])
                    );

                if (!id1 || !id2)
                    return Error{Status{ClioError::rpcMALFORMED_ADDRESS, "malformedAddresses"}};

                return MaybeError{};
            }};

        static auto const malformedRequestHexStringValidator = meta::WithCustomError{
            validation::CustomValidators::Uint256HexStringValidator, Status(ClioError::rpcMALFORMED_REQUEST)
        };

        static auto const malformedRequestIntValidator =
            meta::WithCustomError{validation::Type<uint32_t>{}, Status(ClioError::rpcMALFORMED_REQUEST)};

        static auto const bridgeJsonValidator = meta::WithCustomError{
            meta::IfType<boost::json::object>{meta::Section{
                {ripple::sfLockingChainDoor.getJsonName().c_str(),
                 validation::Required{},
                 validation::CustomValidators::AccountBase58Validator},
                {ripple::sfIssuingChainDoor.getJsonName().c_str(),
                 validation::Required{},
                 validation::CustomValidators::AccountBase58Validator},
                {ripple::sfLockingChainIssue.getJsonName().c_str(),
                 validation::Required{},
                 validation::CustomValidators::CurrencyIssueValidator},
                {ripple::sfIssuingChainIssue.getJsonName().c_str(),
                 validation::Required{},
                 validation::CustomValidators::CurrencyIssueValidator},
            }},
            Status(ClioError::rpcMALFORMED_REQUEST)
        };

        static auto const rpcSpec = RpcSpec{
            {JS(binary), validation::Type<bool>{}},
            {JS(ledger_hash), validation::CustomValidators::Uint256HexStringValidator},
            {JS(ledger_index), validation::CustomValidators::LedgerIndexValidator},
            {JS(index), malformedRequestHexStringValidator},
            {JS(account_root), validation::CustomValidators::AccountBase58Validator},
            {JS(did), validation::CustomValidators::AccountBase58Validator},
            {JS(check), malformedRequestHexStringValidator},
            {JS(deposit_preauth),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{malformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {JS(owner),
                      validation::Required{},
                      meta::WithCustomError{
                          validation::CustomValidators::AccountBase58Validator, Status(ClioError::rpcMALFORMED_OWNER)
                      }},
                     {JS(authorized), validation::Required{}, validation::CustomValidators::AccountBase58Validator},
                 },
             }},
            {JS(directory),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{malformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{meta::Section{
                 {JS(owner), validation::CustomValidators::AccountBase58Validator},
                 {JS(dir_root), validation::CustomValidators::Uint256HexStringValidator},
                 {JS(sub_index), malformedRequestIntValidator}
             }}},
            {JS(escrow),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{malformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {JS(owner),
                      validation::Required{},
                      meta::WithCustomError{
                          validation::CustomValidators::AccountBase58Validator, Status(ClioError::rpcMALFORMED_OWNER)
                      }},
                     {JS(seq), validation::Required{}, malformedRequestIntValidator},
                 },
             }},
            {JS(offer),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{malformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {JS(account), validation::Required{}, validation::CustomValidators::AccountBase58Validator},
                     {JS(seq), validation::Required{}, malformedRequestIntValidator},
                 },
             }},
            {JS(payment_channel), malformedRequestHexStringValidator},
            {JS(ripple_state),
             validation::Type<boost::json::object>{},
             meta::Section{
                 {JS(accounts), validation::Required{}, rippleStateAccountsCheck},
                 {JS(currency), validation::Required{}, validation::CustomValidators::CurrencyValidator},
             }},
            {JS(ticket),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{malformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {JS(account), validation::Required{}, validation::CustomValidators::AccountBase58Validator},
                     {JS(ticket_seq), validation::Required{}, malformedRequestIntValidator},
                 },
             }},
            {JS(nft_page), malformedRequestHexStringValidator},
            {JS(amm),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{malformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {JS(asset),
                      meta::WithCustomError{validation::Required{}, Status(ClioError::rpcMALFORMED_REQUEST)},
                      meta::WithCustomError{
                          validation::Type<boost::json::object>{}, Status(ClioError::rpcMALFORMED_REQUEST)
                      },
                      validation::CustomValidators::CurrencyIssueValidator},
                     {JS(asset2),
                      meta::WithCustomError{validation::Required{}, Status(ClioError::rpcMALFORMED_REQUEST)},
                      meta::WithCustomError{
                          validation::Type<boost::json::object>{}, Status(ClioError::rpcMALFORMED_REQUEST)
                      },
                      validation::CustomValidators::CurrencyIssueValidator},
                 },
             }},
            {JS(bridge),
             meta::WithCustomError{validation::Type<boost::json::object>{}, Status(ClioError::rpcMALFORMED_REQUEST)},
             bridgeJsonValidator},
            {JS(bridge_account),
             meta::WithCustomError{
                 validation::CustomValidators::AccountBase58Validator, Status(ClioError::rpcMALFORMED_REQUEST)
             }},
            {JS(xchain_owned_claim_id),
             meta::WithCustomError{
                 validation::Type<std::string, boost::json::object>{}, Status(ClioError::rpcMALFORMED_REQUEST)
             },
             meta::IfType<std::string>{malformedRequestHexStringValidator},
             bridgeJsonValidator,
             meta::WithCustomError{
                 meta::IfType<boost::json::object>{
                     meta::Section{{JS(xchain_owned_claim_id), validation::Required{}, validation::Type<uint32_t>{}}}
                 },
                 Status(ClioError::rpcMALFORMED_REQUEST)
             }},
            {JS(xchain_owned_create_account_claim_id),
             meta::WithCustomError{
                 validation::Type<std::string, boost::json::object>{}, Status(ClioError::rpcMALFORMED_REQUEST)
             },
             meta::IfType<std::string>{malformedRequestHexStringValidator},
             bridgeJsonValidator,
             meta::WithCustomError{
                 meta::IfType<boost::json::object>{meta::Section{
                     {JS(xchain_owned_create_account_claim_id), validation::Required{}, validation::Type<uint32_t>{}}
                 }},
                 Status(ClioError::rpcMALFORMED_REQUEST)
             }},
            {JS(oracle),
             meta::WithCustomError{
                 validation::Type<std::string, boost::json::object>{}, Status(ClioError::rpcMALFORMED_REQUEST)
             },
             meta::IfType<std::string>{
                 meta::WithCustomError{malformedRequestHexStringValidator, Status(ClioError::rpcMALFORMED_ADDRESS)}
             },
             meta::IfType<boost::json::object>{meta::Section{
                 {JS(account),
                  meta::WithCustomError{validation::Required{}, Status(ClioError::rpcMALFORMED_REQUEST)},
                  meta::WithCustomError{
                      validation::CustomValidators::AccountBase58Validator, Status(ClioError::rpcMALFORMED_ADDRESS)
                  }},
                 // note: Unlike `rippled`, Clio only supports UInt as input, no string, no `null`, etc.:
                 {JS(oracle_document_id),
                  meta::WithCustomError{validation::Required{}, Status(ClioError::rpcMALFORMED_REQUEST)},
                  meta::WithCustomError{
                      validation::Type<uint32_t, std::string>{}, Status(ClioError::rpcMALFORMED_ORACLE_DOCUMENT_ID)
                  },
                  meta::WithCustomError{modifiers::ToNumber{}, Status(ClioError::rpcMALFORMED_ORACLE_DOCUMENT_ID)}},
             }}},
            {JS(ledger), check::Deprecated{}},
            {"include_deleted", validation::Type<bool>{}},
        };

        return rpcSpec;
    }

    /**
     * @brief Process the LedgerEntry command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    Result
    process(Input input, Context const& ctx) const;

private:
    // dir_root and owner can not be both empty or filled at the same time
    // This function will return an error if this is the case
    static std::variant<ripple::uint256, Status>
    composeKeyFromDirectory(boost::json::object const& directory) noexcept;

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
