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

#include "rpc/common/Validators.h"

#include "rpc/Errors.h"
#include "rpc/RPCHelpers.h"
#include "rpc/common/Types.h"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/UintTypes.h>
#include <ripple/protocol/tokens.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace rpc::validation {

[[nodiscard]] MaybeError
Required::verify(boost::json::value const& value, std::string_view key)
{
    if (not value.is_object() or not value.as_object().contains(key.data()))
        return Error{Status{RippledError::rpcINVALID_PARAMS, "Required field '" + std::string{key} + "' missing"}};

    return {};
}

[[nodiscard]] MaybeError
CustomValidator::verify(boost::json::value const& value, std::string_view key) const
{
    if (not value.is_object() or not value.as_object().contains(key.data()))
        return {};  // ignore. field does not exist, let 'required' fail instead

    return validator_(value.as_object().at(key.data()), key);
}

[[nodiscard]] bool
checkIsU32Numeric(std::string_view sv)
{
    uint32_t unused = 0;
    auto [_, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), unused);

    return ec == std::errc();
}

CustomValidator Uint192HexStringValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotString"}};

        ripple::uint192 ledgerHash;
        if (!ledgerHash.parseHex(value.as_string().c_str()))
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "Malformed"}};

        return MaybeError{};
    }};

CustomValidator Uint256HexStringValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotString"}};

        ripple::uint256 ledgerHash;
        if (!ledgerHash.parseHex(value.as_string().c_str()))
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "Malformed"}};

        return MaybeError{};
    }};

CustomValidator LedgerIndexValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view /* key */) -> MaybeError {
        auto err = Error{Status{RippledError::rpcINVALID_PARAMS, "ledgerIndexMalformed"}};

        if (!value.is_string() && !(value.is_uint64() || value.is_int64()))
            return err;

        if (value.is_string() && value.as_string() != "validated" && !checkIsU32Numeric(value.as_string().c_str()))
            return err;

        return MaybeError{};
    }};

CustomValidator AccountValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotString"}};

        // TODO: we are using accountFromStringStrict from RPCHelpers, after we
        // remove all old handler, this function can be moved to here
        if (!accountFromStringStrict(value.as_string().c_str()))
            return Error{Status{RippledError::rpcACT_MALFORMED, std::string(key) + "Malformed"}};

        return MaybeError{};
    }};

CustomValidator AccountBase58Validator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotString"}};

        auto const account = ripple::parseBase58<ripple::AccountID>(value.as_string().c_str());
        if (!account || account->isZero())
            return Error{Status{ClioError::rpcMALFORMED_ADDRESS}};

        return MaybeError{};
    }};

CustomValidator AccountMarkerValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotString"}};

        // TODO: we are using parseAccountCursor from RPCHelpers, after we
        // remove all old handler, this function can be moved to here
        if (!parseAccountCursor(value.as_string().c_str())) {
            // align with the current error message
            return Error{Status{RippledError::rpcINVALID_PARAMS, "Malformed cursor."}};
        }

        return MaybeError{};
    }};

CustomValidator CurrencyValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotString"}};

        ripple::Currency currency;
        if (!ripple::to_currency(currency, value.as_string().c_str()))
            return Error{Status{ClioError::rpcMALFORMED_CURRENCY, "malformedCurrency"}};

        return MaybeError{};
    }};

CustomValidator IssuerValidator =
    CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
            return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotString"}};

        ripple::AccountID issuer;

        // TODO: need to align with the error
        if (!ripple::to_issuer(issuer, value.as_string().c_str()))
            return Error{Status{RippledError::rpcINVALID_PARAMS, fmt::format("Invalid field '{}', bad issuer.", key)}};

        if (issuer == ripple::noAccount()) {
            return Error{
                Status{RippledError::rpcINVALID_PARAMS, fmt::format("Invalid field '{}', bad issuer account one.", key)}
            };
        }

        return MaybeError{};
    }};

CustomValidator SubscribeStreamValidator =
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

            if (reportingNotSupportStreams.contains(v.as_string().c_str()))
                return Error{Status{RippledError::rpcREPORTING_UNSUPPORTED}};

            if (not validStreams.contains(v.as_string().c_str()))
                return Error{Status{RippledError::rpcSTREAM_MALFORMED}};
        }

        return MaybeError{};
    }};

CustomValidator SubscribeAccountsValidator =
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

}  // namespace rpc::validation
