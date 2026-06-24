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

#include <boost/json/array.hpp>
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
 * @brief The ledger_entry method returns a single ledger object from the XRP Ledger in its raw
 * format.
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
        xrpl::LedgerEntryType expectedType = xrpl::ltANY;
        // account id to address account root object
        std::optional<std::string> accountRoot;
        // account id to address did object
        std::optional<std::string> did;
        // mpt issuance id to address mptIssuance object
        std::optional<std::string> mptIssuance;
        // TODO: extract into custom objects, remove json from Input
        std::optional<boost::json::object> directory;
        std::optional<boost::json::object> offer;
        std::optional<boost::json::object> rippleStateAccount;
        std::optional<boost::json::object> escrow;
        std::optional<boost::json::object> depositPreauth;
        std::optional<boost::json::object> ticket;
        std::optional<boost::json::object> amm;
        std::optional<boost::json::object> mptoken;
        std::optional<boost::json::object> permissionedDomain;
        std::optional<boost::json::object> vault;
        std::optional<boost::json::object> loanBroker;
        std::optional<boost::json::object> loan;
        std::optional<xrpl::STXChainBridge> bridge;
        std::optional<std::string> bridgeAccount;
        std::optional<uint32_t> chainClaimId;
        std::optional<uint32_t> createAccountClaimId;
        std::optional<xrpl::uint256> oracleNode;
        std::optional<xrpl::uint256> credential;
        std::optional<boost::json::object> delegate;
        bool includeDeleted = false;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new LedgerEntryHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    LedgerEntryHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
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
        // Validator only works in this handler
        // The accounts array must have two different elements
        // Each element must be a valid address
        static auto const kRippleStateAccountsCheck = validation::CustomValidator{
            [](boost::json::value const& value, std::string_view /* key */) -> MaybeError {
                if (!value.is_array() || value.as_array().size() != 2 ||
                    !value.as_array()[0].is_string() || !value.as_array()[1].is_string() ||
                    value.as_array()[0].as_string() == value.as_array()[1].as_string()) {
                    return Error{Status{RippledError::RpcInvalidParams, "malformedAccounts"}};
                }

                auto const id1 = util::parseBase58Wrapper<xrpl::AccountID>(
                    boost::json::value_to<std::string>(value.as_array()[0])
                );
                auto const id2 = util::parseBase58Wrapper<xrpl::AccountID>(
                    boost::json::value_to<std::string>(value.as_array()[1])
                );

                if (!id1 || !id2)
                    return Error{Status{ClioError::RpcMalformedAddress, "malformedAddresses"}};

                return MaybeError{};
            }
        };

        static auto const kMalformedRequestHexStringValidator = meta::WithCustomError{
            validation::CustomValidators::uint256HexStringValidator,
            Status(ClioError::RpcMalformedRequest)
        };

        static auto const kMalformedRequestIntValidator = meta::WithCustomError{
            validation::Type<uint32_t>{}, Status(ClioError::RpcMalformedRequest)
        };

        static auto const kBridgeJsonValidator = meta::WithCustomError{
            meta::IfType<boost::json::object>{meta::Section{
                {xrpl::sfLockingChainDoor.getJsonName().cStr(),
                 validation::Required{},
                 validation::CustomValidators::accountBase58Validator},
                {xrpl::sfIssuingChainDoor.getJsonName().cStr(),
                 validation::Required{},
                 validation::CustomValidators::accountBase58Validator},
                {xrpl::sfLockingChainIssue.getJsonName().cStr(),
                 validation::Required{},
                 validation::CustomValidators::currencyIssueValidator},
                {xrpl::sfIssuingChainIssue.getJsonName().cStr(),
                 validation::Required{},
                 validation::CustomValidators::currencyIssueValidator},
            }},
            Status(ClioError::RpcMalformedRequest)
        };

        static auto const kRpcSpec = RpcSpec{
            {JS(binary), validation::Type<bool>{}},
            {JS(ledger_hash), validation::CustomValidators::uint256HexStringValidator},
            {JS(ledger_index), validation::CustomValidators::ledgerIndexValidator},
            {JS(index), kMalformedRequestHexStringValidator},
            {JS(account_root), validation::CustomValidators::accountBase58Validator},
            {JS(did), validation::CustomValidators::accountBase58Validator},
            {JS(check), kMalformedRequestHexStringValidator},
            {JS(deposit_preauth),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{kMalformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {JS(owner),
                      validation::Required{},
                      meta::WithCustomError{
                          validation::CustomValidators::accountBase58Validator,
                          Status(ClioError::RpcMalformedOwner)
                      }},
                     {JS(authorized), validation::CustomValidators::accountBase58Validator},
                     {JS(authorized_credentials),
                      validation::CustomValidators::authorizeCredentialValidator}
                 },
             }},
            {JS(directory),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{kMalformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{meta::Section{
                 {JS(owner), validation::CustomValidators::accountBase58Validator},
                 {JS(dir_root), validation::CustomValidators::uint256HexStringValidator},
                 {JS(sub_index), kMalformedRequestIntValidator}
             }}},
            {JS(escrow),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{kMalformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {JS(owner),
                      validation::Required{},
                      meta::WithCustomError{
                          validation::CustomValidators::accountBase58Validator,
                          Status(ClioError::RpcMalformedOwner)
                      }},
                     {JS(seq), validation::Required{}, kMalformedRequestIntValidator},
                 },
             }},
            {JS(offer),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{kMalformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {JS(account),
                      validation::Required{},
                      validation::CustomValidators::accountBase58Validator},
                     {JS(seq), validation::Required{}, kMalformedRequestIntValidator},
                 },
             }},
            {JS(payment_channel), kMalformedRequestHexStringValidator},
            {JS(ripple_state),
             validation::Type<boost::json::object>{},
             meta::Section{
                 {JS(accounts), validation::Required{}, kRippleStateAccountsCheck},
                 {JS(currency),
                  validation::Required{},
                  validation::CustomValidators::currencyValidator},
             }},
            {JS(ticket),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{kMalformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {JS(account),
                      validation::Required{},
                      validation::CustomValidators::accountBase58Validator},
                     {JS(ticket_seq), validation::Required{}, kMalformedRequestIntValidator},
                 },
             }},
            {JS(nft_page), kMalformedRequestHexStringValidator},
            {JS(amm),
             validation::Type<std::string, boost::json::object>{},
             meta::IfType<std::string>{kMalformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {JS(asset),
                      meta::WithCustomError{
                          validation::Required{}, Status(ClioError::RpcMalformedRequest)
                      },
                      meta::WithCustomError{
                          validation::Type<boost::json::object>{},
                          Status(ClioError::RpcMalformedRequest)
                      },
                      validation::CustomValidators::currencyIssueValidator},
                     {JS(asset2),
                      meta::WithCustomError{
                          validation::Required{}, Status(ClioError::RpcMalformedRequest)
                      },
                      meta::WithCustomError{
                          validation::Type<boost::json::object>{},
                          Status(ClioError::RpcMalformedRequest)
                      },
                      validation::CustomValidators::currencyIssueValidator},
                 },
             }},
            {JS(bridge),
             meta::WithCustomError{
                 validation::Type<boost::json::object>{}, Status(ClioError::RpcMalformedRequest)
             },
             kBridgeJsonValidator},
            {JS(bridge_account),
             meta::WithCustomError{
                 validation::CustomValidators::accountBase58Validator,
                 Status(ClioError::RpcMalformedRequest)
             }},
            {JS(xchain_owned_claim_id),
             meta::WithCustomError{
                 validation::Type<std::string, boost::json::object>{},
                 Status(ClioError::RpcMalformedRequest)
             },
             meta::IfType<std::string>{kMalformedRequestHexStringValidator},
             kBridgeJsonValidator,
             meta::WithCustomError{
                 meta::IfType<boost::json::object>{meta::Section{
                     {JS(xchain_owned_claim_id),
                      validation::Required{},
                      validation::Type<uint32_t>{}}
                 }},
                 Status(ClioError::RpcMalformedRequest)
             }},
            {JS(xchain_owned_create_account_claim_id),
             meta::WithCustomError{
                 validation::Type<std::string, boost::json::object>{},
                 Status(ClioError::RpcMalformedRequest)
             },
             meta::IfType<std::string>{kMalformedRequestHexStringValidator},
             kBridgeJsonValidator,
             meta::WithCustomError{
                 meta::IfType<boost::json::object>{meta::Section{
                     {JS(xchain_owned_create_account_claim_id),
                      validation::Required{},
                      validation::Type<uint32_t>{}}
                 }},
                 Status(ClioError::RpcMalformedRequest)
             }},
            {JS(oracle),
             meta::WithCustomError{
                 validation::Type<std::string, boost::json::object>{},
                 Status(ClioError::RpcMalformedRequest)
             },
             meta::IfType<std::string>{meta::WithCustomError{
                 kMalformedRequestHexStringValidator, Status(ClioError::RpcMalformedAddress)
             }},
             meta::IfType<boost::json::object>{meta::Section{
                 {JS(account),
                  meta::WithCustomError{
                      validation::Required{}, Status(ClioError::RpcMalformedRequest)
                  },
                  meta::WithCustomError{
                      validation::CustomValidators::accountBase58Validator,
                      Status(ClioError::RpcMalformedAddress)
                  }},
                 // note: Unlike `rippled`, Clio only supports UInt as input, no string, no `null`,
                 // etc.:
                 {JS(oracle_document_id),
                  meta::WithCustomError{
                      validation::Required{}, Status(ClioError::RpcMalformedRequest)
                  },
                  meta::WithCustomError{
                      validation::Type<uint32_t, std::string>{},
                      Status(ClioError::RpcMalformedOracleDocumentId)
                  },
                  meta::WithCustomError{
                      modifiers::ToNumber{}, Status(ClioError::RpcMalformedOracleDocumentId)
                  }},
             }}},
            {JS(credential),
             meta::WithCustomError{
                 validation::Type<std::string, boost::json::object>{},
                 Status(ClioError::RpcMalformedRequest)
             },
             meta::IfType<std::string>{meta::WithCustomError{
                 kMalformedRequestHexStringValidator, Status(ClioError::RpcMalformedAddress)
             }},
             meta::IfType<boost::json::object>{meta::Section{
                 {JS(subject),
                  meta::WithCustomError{
                      validation::Required{}, Status(ClioError::RpcMalformedRequest)
                  },
                  meta::WithCustomError{
                      validation::CustomValidators::accountBase58Validator,
                      Status(ClioError::RpcMalformedAddress)
                  }},
                 {JS(issuer),
                  meta::WithCustomError{
                      validation::Required{}, Status(ClioError::RpcMalformedRequest)
                  },
                  meta::WithCustomError{
                      validation::CustomValidators::accountBase58Validator,
                      Status(ClioError::RpcMalformedAddress)
                  }},
                 {
                     JS(credential_type),
                     meta::WithCustomError{
                         validation::Required{}, Status(ClioError::RpcMalformedRequest)
                     },
                     meta::WithCustomError{
                         validation::Type<std::string>{}, Status(ClioError::RpcMalformedRequest)
                     },
                 },
             }}},
            {JS(mpt_issuance),
             meta::WithCustomError{
                 validation::CustomValidators::uint192HexStringValidator,
                 Status(ClioError::RpcMalformedRequest)
             }},
            {JS(mptoken),
             meta::WithCustomError{
                 validation::Type<std::string, boost::json::object>{},
                 Status(ClioError::RpcMalformedRequest)
             },
             meta::IfType<std::string>{kMalformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{
                 meta::Section{
                     {
                         JS(account),
                         meta::WithCustomError{
                             validation::Required{}, Status(ClioError::RpcMalformedRequest)
                         },
                         meta::WithCustomError{
                             validation::CustomValidators::accountBase58Validator,
                             Status(ClioError::RpcMalformedAddress)
                         },
                     },
                     {
                         JS(mpt_issuance_id),
                         meta::WithCustomError{
                             validation::Required{}, Status(ClioError::RpcMalformedRequest)
                         },
                         meta::WithCustomError{
                             validation::CustomValidators::uint192HexStringValidator,
                             Status(ClioError::RpcMalformedRequest)
                         },
                     },
                 },
             }},
            {JS(permissioned_domain),
             meta::WithCustomError{
                 validation::Type<std::string, boost::json::object>{},
                 Status(ClioError::RpcMalformedRequest)
             },
             meta::IfType<std::string>{kMalformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{meta::Section{
                 {JS(seq),
                  meta::WithCustomError{
                      validation::Required{}, Status(ClioError::RpcMalformedRequest)
                  },
                  meta::WithCustomError{
                      validation::Type<uint32_t>{}, Status(ClioError::RpcMalformedRequest)
                  }},
                 {
                     JS(account),
                     meta::WithCustomError{
                         validation::Required{}, Status(ClioError::RpcMalformedRequest)
                     },
                     meta::WithCustomError{
                         validation::CustomValidators::accountBase58Validator,
                         Status(ClioError::RpcMalformedAddress)
                     },
                 },
             }}},
            {JS(vault),
             meta::WithCustomError{
                 validation::Type<std::string, boost::json::object>{},
                 Status(ClioError::RpcMalformedRequest)
             },
             meta::IfType<std::string>{kMalformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{meta::Section{
                 {JS(seq),
                  meta::WithCustomError{
                      validation::Required{}, Status(ClioError::RpcMalformedRequest)
                  },
                  meta::WithCustomError{
                      validation::Type<uint32_t>{}, Status(ClioError::RpcMalformedRequest)
                  }},
                 {
                     JS(owner),
                     meta::WithCustomError{
                         validation::Required{}, Status(ClioError::RpcMalformedRequest)
                     },
                     meta::WithCustomError{
                         validation::CustomValidators::accountBase58Validator,
                         Status(ClioError::RpcMalformedOwner)
                     },
                 },
             }}},
            {JS(loan_broker),
             meta::WithCustomError{
                 validation::Type<std::string, boost::json::object>{},
                 Status(ClioError::RpcMalformedRequest)
             },
             meta::IfType<std::string>{kMalformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{meta::Section{
                 {JS(seq),
                  meta::WithCustomError{
                      validation::Required{}, Status(ClioError::RpcMalformedRequest)
                  },
                  meta::WithCustomError{
                      validation::Type<uint32_t>{}, Status(ClioError::RpcMalformedRequest)
                  }},
                 {
                     JS(owner),
                     meta::WithCustomError{
                         validation::Required{}, Status(ClioError::RpcMalformedRequest)
                     },
                     meta::WithCustomError{
                         validation::CustomValidators::accountBase58Validator,
                         Status(ClioError::RpcMalformedOwner)
                     },
                 },
             }}},
            {JS(loan),
             meta::WithCustomError{
                 validation::Type<std::string, boost::json::object>{},
                 Status(ClioError::RpcMalformedRequest)
             },
             meta::IfType<std::string>{kMalformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{meta::Section{
                 {JS(loan_seq),
                  meta::WithCustomError{
                      validation::Required{}, Status(ClioError::RpcMalformedRequest)
                  },
                  meta::WithCustomError{
                      validation::Type<uint32_t>{}, Status(ClioError::RpcMalformedRequest)
                  }},
                 {
                     JS(loan_broker_id),
                     meta::WithCustomError{
                         validation::Required{}, Status(ClioError::RpcMalformedRequest)
                     },
                     meta::WithCustomError{
                         validation::CustomValidators::uint256HexStringValidator,
                         Status(ClioError::RpcMalformedRequest)
                     },
                 },
             }}},
            {JS(delegate),
             meta::WithCustomError{
                 validation::Type<std::string, boost::json::object>{},
                 Status(ClioError::RpcMalformedRequest)
             },
             meta::IfType<std::string>{kMalformedRequestHexStringValidator},
             meta::IfType<boost::json::object>{meta::Section{
                 {JS(account),
                  meta::WithCustomError{
                      validation::Required{}, Status(ClioError::RpcMalformedRequest)
                  },
                  meta::WithCustomError{
                      validation::CustomValidators::accountBase58Validator,
                      Status(ClioError::RpcMalformedAddress)
                  }},
                 {JS(authorize),
                  meta::WithCustomError{
                      validation::Required{}, Status(ClioError::RpcMalformedRequest)
                  },
                  meta::WithCustomError{
                      validation::CustomValidators::accountBase58Validator,
                      Status(ClioError::RpcMalformedAddress)
                  }}
             }}},
            {JS(amendments), kMalformedRequestHexStringValidator},
            {JS(fee), kMalformedRequestHexStringValidator},
            {JS(hashes), kMalformedRequestHexStringValidator},
            {JS(nft_offer), kMalformedRequestHexStringValidator},
            {JS(nunl), kMalformedRequestHexStringValidator},
            {JS(signer_list), kMalformedRequestHexStringValidator},
            {JS(ledger), check::Deprecated{}},
            {"include_deleted", validation::Type<bool>{}},
        };

        return kRpcSpec;
    }

    /**
     * @brief Process the LedgerEntry command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const;

private:
    // dir_root and owner can not be both empty or filled at the same time
    // This function will return an error if this is the case
    static std::expected<xrpl::uint256, Status>
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
