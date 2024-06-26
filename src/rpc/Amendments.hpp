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

#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/digest.h>

#include <string_view>

namespace rpc {

#define REGISTER_AMENDMENT(name) inline static const ripple::uint256 name = GetAmendmentId(#name);

/**
 * @brief Represents a list of amendments in the XRPL.
 */
struct Amendments {
    /**
     * @param name The name of the amendment
     * @return The corresponding amendment Id
     */
    static ripple::uint256
    GetAmendmentId(std::string_view const name)
    {
        return ripple::sha512Half(ripple::Slice(name.data(), name.size()));
    }

    REGISTER_AMENDMENT(DisallowIncoming)
    REGISTER_AMENDMENT(Clawback)
};
}  // namespace rpc
