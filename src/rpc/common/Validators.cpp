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

#include "rpc/common/Validators.hpp"

#include "rpc/Errors.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/core.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/tokens.h>

#include <charconv>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace rpc::validation {

[[nodiscard]] MaybeError
Required::verify(boost::json::value const& value, std::string_view key)
{
    if (not value.is_object() or not value.as_object().contains(key))
        return Error{Status{RippledError::rpcINVALID_PARAMS, "Required field '" + std::string{key} + "' missing"}};

    return {};
}

[[nodiscard]] MaybeError
TimeFormatValidator::verify(boost::json::value const& value, std::string_view key) const
{
    using boost::json::value_to;

    if (not value.is_object() or not value.as_object().contains(key))
        return {};  // ignore. field does not exist, let 'required' fail instead

    if (not value.as_object().at(key).is_string())
        return Error{Status{RippledError::rpcINVALID_PARAMS}};

    std::tm time = {};
    std::stringstream stream(value_to<std::string>(value.as_object().at(key)));
    stream >> std::get_time(&time, format_.c_str());
    if (stream.fail())
        return Error{Status{RippledError::rpcINVALID_PARAMS}};

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

CustomValidator CustomValidators::Uint160HexStringValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        return makeHexStringValidator<ripple::uint160>(value, key);
    }};

CustomValidator CustomValidators::Uint192HexStringValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        return makeHexStringValidator<ripple::uint192>(value, key);
    }};

CustomValidator CustomValidators::Uint256HexStringValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        return makeHexStringValidator<ripple::uint256>(value, key);
    }};

CustomValidator CustomValidators::LedgerIndexValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view /* key */) -> MaybeError {
        auto err = Error{Status{RippledError::rpcINVALID_PARAMS, "ledgerIndexMalformed"}};

        if (!value.is_string() && !(value.is_uint64() || value.is_int64()))
            return err;

        if (value.is_string() && value.as_string() != "validated" &&
            !checkIsU32Numeric(boost::json::value_to<std::string>(value)))
            return err;

        return MaybeError{};
    }};

CustomValidator CustomValidators::AccountValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotString"}};

        // TODO: we are using accountFromStringStrict from RPCHelpers, after we
        // remove all old handler, this function can be moved to here
        if (!accountFromStringStrict(boost::json::value_to<std::string>(value)))
            return Error{Status{RippledError::rpcACT_MALFORMED, std::string(key) + "Malformed"}};

        return MaybeError{};
    }};

CustomValidator CustomValidators::AccountBase58Validator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotString"}};

        auto const account = ripple::parseBase58<ripple::AccountID>(boost::json::value_to<std::string>(value));
        if (!account || account->isZero())
            return Error{Status{ClioError::rpcMALFORMED_ADDRESS}};

        return MaybeError{};
    }};

CustomValidator CustomValidators::AccountMarkerValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotString"}};

        // TODO: we are using parseAccountCursor from RPCHelpers, after we
        // remove all old handler, this function can be moved to here
        if (!parseAccountCursor(boost::json::value_to<std::string>(value))) {
            // align with the current error message
            return Error{Status{RippledError::rpcINVALID_PARAMS, "Malformed cursor."}};
        }

        return MaybeError{};
    }};

CustomValidator CustomValidators::CurrencyValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotString"}};

        auto const currencyStr = boost::json::value_to<std::string>(value);
        if (currencyStr.empty())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "IsEmpty"}};

        ripple::Currency currency;
        if (!ripple::to_currency(currency, currencyStr))
            return Error{Status{ClioError::rpcMALFORMED_CURRENCY, "malformedCurrency"}};

        return MaybeError{};
    }};

CustomValidator CustomValidators::IssuerValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotString"}};

        ripple::AccountID issuer;

        // TODO: need to align with the error
        if (!ripple::to_issuer(issuer, boost::json::value_to<std::string>(value)))
            return Error{Status{RippledError::rpcINVALID_PARAMS, fmt::format("Invalid field '{}', bad issuer.", key)}};

        if (issuer == ripple::noAccount()) {
            return Error{
                Status{RippledError::rpcINVALID_PARAMS, fmt::format("Invalid field '{}', bad issuer account one.", key)}
            };
        }

        return MaybeError{};
    }};

CustomValidator CustomValidators::SubscribeStreamValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_array())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotArray"}};

        static std::unordered_set<std::string> const validStreams = {
            "ledger", "transactions", "transactions_proposed", "book_changes", "manifests", "validations"
        };

        static std::unordered_set<std::string> const reportingNotSupportStreams = {
            "peer_status", "consensus", "server"
        };
        for (auto const& v : value.as_array()) {
            if (!v.is_string())
                return Error{Status{RippledError::rpcINVALID_PARAMS, "streamNotString"}};

            if (reportingNotSupportStreams.contains(boost::json::value_to<std::string>(v)))
                return Error{Status{RippledError::rpcREPORTING_UNSUPPORTED}};

            if (not validStreams.contains(boost::json::value_to<std::string>(v)))
                return Error{Status{RippledError::rpcSTREAM_MALFORMED}};
        }

        return MaybeError{};
    }};

CustomValidator CustomValidators::SubscribeAccountsValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_array())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotArray"}};

        if (value.as_array().empty())
            return Error{Status{RippledError::rpcACT_MALFORMED, std::string(key) + " malformed."}};

        for (auto const& v : value.as_array()) {
            auto obj = boost::json::object();
            auto const keyItem = std::string(key) + "'sItem";

            obj[keyItem] = v;

            if (auto err = AccountValidator.verify(obj, keyItem); !err)
                return err;
        }

        return MaybeError{};
    }};

CustomValidator CustomValidators::CurrencyIssueValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (not value.is_object())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotObject"}};

        try {
            parseIssue(value.as_object());
        } catch (std::runtime_error const&) {
            return Error{Status{ClioError::rpcMALFORMED_REQUEST}};
        }

        return MaybeError{};
    }};

}  // namespace rpc::validation
