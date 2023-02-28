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

#include <ripple/basics/base_uint.h>
#include <rpc/RPCHelpers.h>
#include <rpc/common/Validators.h>

#include <boost/json/value.hpp>

#include <charconv>
#include <string_view>

namespace RPCng::validation {

[[nodiscard]] MaybeError
Section::verify(boost::json::value const& value, std::string_view key) const
{
    if (not value.is_object() or not value.as_object().contains(key.data()))
        return {};  // ignore. field does not exist, let 'required' fail
                    // instead

    auto const& res = value.at(key.data());
    for (auto const& spec : specs)
    {
        if (auto const ret = spec.validate(res); not ret)
            return Error{ret.error()};
    }
    return {};
}

[[nodiscard]] MaybeError
Required::verify(boost::json::value const& value, std::string_view key) const
{
    if (not value.is_object() or not value.as_object().contains(key.data()))
        return Error{RPC::Status{
            RPC::RippledError::rpcINVALID_PARAMS,
            "Required field '" + std::string{key} + "' missing"}};

    return {};
}

[[nodiscard]] MaybeError
ValidateArrayAt::verify(boost::json::value const& value, std::string_view key)
    const
{
    if (not value.is_object() or not value.as_object().contains(key.data()))
        return {};  // ignore. field does not exist, let 'required' fail
                    // instead

    if (not value.as_object().at(key.data()).is_array())
        return Error{RPC::Status{RPC::RippledError::rpcINVALID_PARAMS}};

    auto const& arr = value.as_object().at(key.data()).as_array();
    if (idx_ >= arr.size())
        return Error{RPC::Status{RPC::RippledError::rpcINVALID_PARAMS}};

    auto const& res = arr.at(idx_);
    for (auto const& spec : specs_)
        if (auto const ret = spec.validate(res); not ret)
            return Error{ret.error()};

    return {};
}

[[nodiscard]] MaybeError
CustomValidator::verify(boost::json::value const& value, std::string_view key)
    const
{
    if (not value.is_object() or not value.as_object().contains(key.data()))
        return {};  // ignore. field does not exist, let 'required' fail
                    // instead

    return validator_(value.as_object().at(key.data()), key);
}

[[nodiscard]] bool
checkIsU32Numeric(std::string_view sv)
{
    uint32_t unused;
    auto [_, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), unused);
    return ec == std::errc();
}

CustomValidator LedgerHashValidator = CustomValidator{
    [](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
        {
            return Error{RPC::Status{
                RPC::RippledError::rpcINVALID_PARAMS, "ledgerHashNotString"}};
        }
        ripple::uint256 ledgerHash;
        if (!ledgerHash.parseHex(value.as_string().c_str()))
            return Error{RPC::Status{
                RPC::RippledError::rpcINVALID_PARAMS, "ledgerHashMalformed"}};
        return MaybeError{};
    }};

CustomValidator LedgerIndexValidator = CustomValidator{
    [](boost::json::value const& value, std::string_view key) -> MaybeError {
        auto err = Error{RPC::Status{
            RPC::RippledError::rpcINVALID_PARAMS, "ledgerIndexMalformed"}};
        if (!value.is_string() && !(value.is_uint64() || value.is_int64()))
        {
            return err;
        }
        if (value.is_string() && value.as_string() != "validated" &&
            !checkIsU32Numeric(value.as_string().c_str()))
        {
            return err;
        }
        return MaybeError{};
    }};

CustomValidator AccountValidator = CustomValidator{
    [](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
        {
            return Error{RPC::Status{
                RPC::RippledError::rpcINVALID_PARAMS,
                std::string(key) + "NotString"}};
        }
        // TODO: we are using accountFromStringStrict from RPCHelpers, after we
        // remove all old handler, this function can be moved to here
        if (!RPC::accountFromStringStrict(value.as_string().c_str()))
        {
            return Error{RPC::Status{
                RPC::RippledError::rpcINVALID_PARAMS,
                std::string(key) + "Malformed"}};
        }
        return MaybeError{};
    }};

CustomValidator MarkerValidator = CustomValidator{
    [](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
        {
            return Error{RPC::Status{
                RPC::RippledError::rpcINVALID_PARAMS,
                std::string(key) + "NotString"}};
        }
        // TODO: we are using parseAccountCursor from RPCHelpers, after we
        // remove all old handler, this function can be moved to here
        if (!RPC::parseAccountCursor(value.as_string().c_str()))
        {
            // align with the current error message
            return Error{RPC::Status{
                RPC::RippledError::rpcINVALID_PARAMS, "Malformed cursor"}};
        }
        return MaybeError{};
    }};

CustomValidator TxHashValidator = CustomValidator{
    [](boost::json::value const& value, std::string_view key) -> MaybeError {
        if (!value.is_string())
        {
            return Error{RPC::Status{
                RPC::RippledError::rpcINVALID_PARAMS,
                std::string(key) + "NotString"}};
        }
        ripple::uint256 txHash;
        if (!txHash.parseHex(value.as_string().c_str()))
        {
            return Error{RPC::Status{
                RPC::RippledError::rpcINVALID_PARAMS, "malformedTransaction"}};
        }
        return MaybeError{};
    }};

}  // namespace RPCng::validation
