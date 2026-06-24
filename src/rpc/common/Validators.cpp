#include "rpc/common/Validators.hpp"

#include "rpc/Errors.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/AccountUtils.hpp"
#include "util/LedgerUtils.hpp"
#include "util/TimeUtils.hpp"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/format.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/UintTypes.h>

#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace rpc::validation {

[[nodiscard]] MaybeError
Required::verify(boost::json::value const& value, std::string_view key)
{
    if (not value.is_object() or not value.as_object().contains(key)) {
        return Error{Status{
            RippledError::RpcInvalidParams, "Required field '" + std::string{key} + "' missing"
        }};
    }

    return {};
}

[[nodiscard]] MaybeError
TimeFormatValidator::verify(boost::json::value const& value, std::string_view key) const
{
    using boost::json::value_to;

    if (not value.is_object() or not value.as_object().contains(key))
        return {};  // ignore. field does not exist, let 'required' fail instead

    if (not value.as_object().at(key).is_string())
        return Error{Status{RippledError::RpcInvalidParams}};

    auto const ret =
        util::systemTpFromUtcStr(value_to<std::string>(value.as_object().at(key)), format_);
    if (!ret)
        return Error{Status{RippledError::RpcInvalidParams}};

    return {};
}

[[nodiscard]] MaybeError
CustomValidator::verify(boost::json::value const& value, std::string_view key) const
{
    if (not value.is_object() or not value.as_object().contains(key))
        return {};  // ignore. field does not exist, let 'required' fail instead

    return validator_(value.as_object().at(key), key);
}

[[nodiscard]] bool
checkIsU32Numeric(std::string_view sv)
{
    uint32_t unused = 0;
    auto [_, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), unused);

    return ec == std::errc();
}

CustomValidator CustomValidators::uint160HexStringValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        return makeHexStringValidator<xrpl::uint160>(value, key);
    }};

CustomValidator CustomValidators::uint192HexStringValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        return makeHexStringValidator<xrpl::uint192>(value, key);
    }};

CustomValidator CustomValidators::uint256HexStringValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        return makeHexStringValidator<xrpl::uint256>(value, key);
    }};

CustomValidator CustomValidators::ledgerIndexValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view /* key */) -> MaybeError {
        auto err = Error{Status{RippledError::RpcInvalidParams, "ledgerIndexMalformed"}};

        if (!value.is_string() && !(value.is_uint64() || value.is_int64()))
            return err;

        if (value.is_string() && value.as_string() != "validated" &&
            !checkIsU32Numeric(boost::json::value_to<std::string>(value)))
            return err;

        return MaybeError{};
    }};

CustomValidator CustomValidators::ledgerTypeValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string()) {
            return Error{Status{
                RippledError::RpcInvalidParams, fmt::format("Invalid field '{}', not string.", key)
            }};
        }

        auto const type =
            util::LedgerTypes::getLedgerEntryTypeFromStr(boost::json::value_to<std::string>(value));
        if (type == xrpl::ltANY) {
            return Error{
                Status{RippledError::RpcInvalidParams, fmt::format("Invalid field '{}'.", key)}
            };
        }

        return MaybeError{};
    }};

CustomValidator CustomValidators::accountValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::RpcInvalidParams, std::string(key) + "NotString"}};

        // TODO: we are using accountFromStringStrict from RPCHelpers, after we
        // remove all old handler, this function can be moved to here
        if (!accountFromStringStrict(boost::json::value_to<std::string>(value)))
            return Error{Status{RippledError::RpcActMalformed, std::string(key) + "Malformed"}};

        return MaybeError{};
    }};

CustomValidator CustomValidators::accountBase58Validator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::RpcInvalidParams, std::string(key) + "NotString"}};

        auto const account =
            util::parseBase58Wrapper<xrpl::AccountID>(boost::json::value_to<std::string>(value));
        if (!account || account->isZero())
            return Error{Status{ClioError::RpcMalformedAddress}};

        return MaybeError{};
    }};

CustomValidator CustomValidators::accountMarkerValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::RpcInvalidParams, std::string(key) + "NotString"}};

        // TODO: we are using parseAccountCursor from RPCHelpers, after we
        // remove all old handler, this function can be moved to here
        if (!parseAccountCursor(boost::json::value_to<std::string>(value))) {
            // align with the current error message
            return Error{Status{RippledError::RpcInvalidParams, "Malformed cursor."}};
        }

        return MaybeError{};
    }};

CustomValidator CustomValidators::accountTypeValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string()) {
            return Error{Status{
                RippledError::RpcInvalidParams, fmt::format("Invalid field '{}', not string.", key)
            }};
        }

        auto const type = util::LedgerTypes::getAccountOwnedLedgerTypeFromStr(
            boost::json::value_to<std::string>(value)
        );
        if (type == xrpl::ltANY) {
            return Error{
                Status{RippledError::RpcInvalidParams, fmt::format("Invalid field '{}'.", key)}
            };
        }

        return MaybeError{};
    }};

CustomValidator CustomValidators::currencyValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::RpcInvalidParams, std::string(key) + "NotString"}};

        auto const currencyStr = boost::json::value_to<std::string>(value);
        if (currencyStr.empty())
            return Error{Status{RippledError::RpcInvalidParams, std::string(key) + "IsEmpty"}};

        xrpl::Currency currency;
        if (!xrpl::toCurrency(currency, currencyStr))
            return Error{Status{ClioError::RpcMalformedCurrency, "malformedCurrency"}};

        return MaybeError{};
    }};

CustomValidator CustomValidators::issuerValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::RpcInvalidParams, std::string(key) + "NotString"}};

        xrpl::AccountID issuer;

        // TODO: need to align with the error
        if (!xrpl::toIssuer(issuer, boost::json::value_to<std::string>(value))) {
            return Error{Status{
                RippledError::RpcInvalidParams, fmt::format("Invalid field '{}', bad issuer.", key)
            }};
        }

        if (issuer == xrpl::noAccount()) {
            return Error{Status{
                RippledError::RpcInvalidParams,
                fmt::format("Invalid field '{}', bad issuer account one.", key)
            }};
        }

        return MaybeError{};
    }};

CustomValidator CustomValidators::subscribeStreamValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_array())
            return Error{Status{RippledError::RpcInvalidParams, std::string(key) + "NotArray"}};

        static std::unordered_set<std::string> const kValidStreams = {
            "ledger",
            "transactions",
            "transactions_proposed",
            "book_changes",
            "manifests",
            "validations"
        };

        static std::unordered_set<std::string> const kNotSupportStreams = {
            "peer_status", "consensus", "server"
        };
        for (auto const& v : value.as_array()) {
            if (!v.is_string())
                return Error{Status{RippledError::RpcInvalidParams, "streamNotString"}};

            if (kNotSupportStreams.contains(boost::json::value_to<std::string>(v)))
                return Error{Status{RippledError::RpcNotSupported}};

            if (not kValidStreams.contains(boost::json::value_to<std::string>(v)))
                return Error{Status{RippledError::RpcStreamMalformed}};
        }

        return MaybeError{};
    }};

CustomValidator CustomValidators::subscribeAccountsValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_array())
            return Error{Status{RippledError::RpcInvalidParams, std::string(key) + "NotArray"}};

        if (value.as_array().empty())
            return Error{Status{RippledError::RpcActMalformed, std::string(key) + " malformed."}};

        for (auto const& v : value.as_array()) {
            auto obj = boost::json::object();
            auto const keyItem = std::string(key) + "'sItem";

            obj[keyItem] = v;

            if (auto err = accountValidator.verify(obj, keyItem); !err)
                return err;
        }

        return MaybeError{};
    }};

CustomValidator CustomValidators::currencyIssueValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (not value.is_object())
            return Error{Status{RippledError::RpcInvalidParams, std::string(key) + "NotObject"}};

        try {
            parseIssue(value.as_object());
        } catch (std::runtime_error const&) {
            return Error{Status{ClioError::RpcMalformedRequest}};
        }

        return MaybeError{};
    }};

CustomValidator CustomValidators::credentialTypeValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (not value.is_string()) {
            return Error{Status{
                ClioError::RpcMalformedAuthorizedCredentials, std::string(key) + " NotString"
            }};
        }

        auto const& credTypeHex = xrpl::strViewUnHex(value.as_string());
        if (!credTypeHex.has_value()) {
            return Error{Status{
                ClioError::RpcMalformedAuthorizedCredentials, std::string(key) + " NotHexString"
            }};
        }

        if (credTypeHex->empty()) {
            return Error{
                Status{ClioError::RpcMalformedAuthorizedCredentials, std::string(key) + " is empty"}
            };
        }

        if (credTypeHex->size() > xrpl::kMaxCredentialTypeLength) {
            return Error{Status{
                ClioError::RpcMalformedAuthorizedCredentials,
                std::string(key) + " greater than max length"
            }};
        }

        return MaybeError{};
    }};

CustomValidator CustomValidators::authorizeCredentialValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (not value.is_array())
            return Error{Status{ClioError::RpcMalformedRequest, std::string(key) + " not array"}};

        auto const& authCred = value.as_array();
        if (authCred.empty()) {
            return Error{Status{
                ClioError::RpcMalformedAuthorizedCredentials,
                fmt::format("Requires at least one element in authorized_credentials array.")
            }};
        }

        if (authCred.size() > xrpl::kMaxCredentialsArraySize) {
            return Error{Status{
                ClioError::RpcMalformedAuthorizedCredentials,
                fmt::format(
                    "Max {} number of credentials in authorized_credentials array",
                    xrpl::kMaxCredentialsArraySize
                )
            }};
        }

        for (auto const& credObj : value.as_array()) {
            if (!credObj.is_object()) {
                return Error{Status{
                    ClioError::RpcMalformedAuthorizedCredentials,
                    "authorized_credentials elements in array are not objects."
                }};
            }
            auto const& obj = credObj.as_object();

            if (!obj.contains("issuer")) {
                return Error{Status{
                    ClioError::RpcMalformedAuthorizedCredentials,
                    "Field 'Issuer' is required but missing."
                }};
            }

            // don't want to change issuer error message to be about credentials
            if (!issuerValidator.verify(credObj, "issuer")) {
                return Error{
                    Status{ClioError::RpcMalformedAuthorizedCredentials, "issuer NotString"}
                };
            }

            if (!obj.contains("credential_type")) {
                return Error{Status{
                    ClioError::RpcMalformedAuthorizedCredentials,
                    "Field 'CredentialType' is required but missing."
                }};
            }

            if (auto const err = credentialTypeValidator.verify(credObj, "credential_type"); !err)
                return err;
        }

        return MaybeError{};
    }};

}  // namespace rpc::validation
