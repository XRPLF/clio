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

#include "data/BackendInterface.h"
#include "rpc/RPCHelpers.h"
#include "rpc/common/MetaProcessors.h"
#include "rpc/common/Types.h"
#include "rpc/common/Validators.h"

namespace rpc {

/**
 * @brief The ledger_entry method returns a single ledger object from the XRP Ledger in its raw format.
 *
 * For more details see: https://xrpl.org/ledger_entry.html
 */
class LedgerEntryHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    struct Output {
        std::string index;
        uint32_t ledgerIndex;
        std::string ledgerHash;
        std::optional<boost::json::object> node;
        std::optional<std::string> nodeBinary;
        bool validated = true;
    };

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
        // mpt issuance id to address mptIssuance object
        std::optional<std::string> mptIssuanceID;
        // TODO: extract into custom objects, remove json from Input
        std::optional<boost::json::object> directory;
        std::optional<boost::json::object> offer;
        std::optional<boost::json::object> rippleStateAccount;
        std::optional<boost::json::object> escrow;
        std::optional<boost::json::object> depositPreauth;
        std::optional<boost::json::object> ticket;
        std::optional<boost::json::object> amm;
    };

    using Result = HandlerReturnType<Output>;

    LedgerEntryHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend) : sharedPtrBackend_(sharedPtrBackend)
    {
    }

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

                auto const id1 = ripple::parseBase58<ripple::AccountID>(value.as_array()[0].as_string().c_str());
                auto const id2 = ripple::parseBase58<ripple::AccountID>(value.as_array()[1].as_string().c_str());

                if (!id1 || !id2)
                    return Error{Status{ClioError::rpcMALFORMED_ADDRESS, "malformedAddresses"}};

                return MaybeError{};
            }};

        static auto const malformedRequestHexStringValidator =
            meta::WithCustomError{validation::Uint256HexStringValidator, Status(ClioError::rpcMALFORMED_REQUEST)};

        static auto const malformedRequestIntValidator =
            meta::WithCustomError{validation::Type<uint32_t>{}, Status(ClioError::rpcMALFORMED_REQUEST)};

        static auto const ammAssetValidator =
            validation::CustomValidator{[](boost::json::value const& value, std::string_view /* key */) -> MaybeError {
                if (!value.is_object()) {
                    return Error{Status{ClioError::rpcMALFORMED_REQUEST}};
                }

                Json::Value jvAsset;
                if (value.as_object().contains(JS(issuer)))
                    jvAsset["issuer"] = value.at(JS(issuer)).as_string().c_str();
                if (value.as_object().contains(JS(currency)))
                    jvAsset["currency"] = value.at(JS(currency)).as_string().c_str();
                // same as rippled
                try {
                    ripple::issueFromJson(jvAsset);
                } catch (std::runtime_error const&) {
                    return Error{Status{ClioError::rpcMALFORMED_REQUEST}};
                }

                return MaybeError{};
            }};

        static auto const rpcSpec = RpcSpec{
            {JS(binary), validation::Type<bool>{}},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(index), malformedRequestHexStringValidator},
            {JS(account_root), validation::AccountBase58Validator},
            {JS(did), validation::AccountBase58Validator},
            {JS(mpt_issuance_id), validation::Uint192HexStringValidator},
            {JS(check), malformedRequestHexStringValidator},
            {JS(deposit_preauth),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{malformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {JS(owner),
                      validation::Required{},
                      meta::WithCustomError{validation::AccountBase58Validator, Status(ClioError::rpcMALFORMED_OWNER)}},
                     {JS(authorized), validation::Required{}, validation::AccountBase58Validator},
                 },
             }},
            {JS(directory),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{malformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{meta::Section{
                 {JS(owner), validation::AccountBase58Validator},
                 {JS(dir_root), validation::Uint256HexStringValidator},
                 {JS(sub_index), malformedRequestIntValidator}
             }}},
            {JS(escrow),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{malformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {JS(owner),
                      validation::Required{},
                      meta::WithCustomError{validation::AccountBase58Validator, Status(ClioError::rpcMALFORMED_OWNER)}},
                     {JS(seq), validation::Required{}, malformedRequestIntValidator},
                 },
             }},
            {JS(offer),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{malformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {JS(account), validation::Required{}, validation::AccountBase58Validator},
                     {JS(seq), validation::Required{}, malformedRequestIntValidator},
                 },
             }},
            {JS(payment_channel), malformedRequestHexStringValidator},
            {JS(ripple_state),
             validation::Type<boost::json::object>{},
             meta::Section{
                 {JS(accounts), validation::Required{}, rippleStateAccountsCheck},
                 {JS(currency), validation::Required{}, validation::CurrencyValidator},
             }},
            {JS(ticket),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{malformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {JS(account), validation::Required{}, validation::AccountBase58Validator},
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
                      ammAssetValidator},
                     {JS(asset2),
                      meta::WithCustomError{validation::Required{}, Status(ClioError::rpcMALFORMED_REQUEST)},
                      meta::WithCustomError{
                          validation::Type<boost::json::object>{}, Status(ClioError::rpcMALFORMED_REQUEST)
                      },
                      ammAssetValidator},
                 },
             }}
        };

        return rpcSpec;
    }

    Result
    process(Input input, Context const& ctx) const;

private:
    // dir_root and owner can not be both empty or filled at the same time
    // This function will return an error if this is the case
    static std::variant<ripple::uint256, Status>
    composeKeyFromDirectory(boost::json::object const& directory) noexcept;

    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);

    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);
};

}  // namespace rpc
