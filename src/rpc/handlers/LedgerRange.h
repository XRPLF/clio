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

#include <backend/BackendInterface.h>
#include <rpc/common/Types.h>

#include <boost/json.hpp>

#include <memory>

namespace RPC {

/**
 * @brief The ledger_range command returns the index number of the earliest and latest ledgers that the server has.
 *
 * Not documented in the official rippled API docs.
 */
class LedgerRangeHandler
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    struct Output
    {
        Backend::LedgerRange range;
    };

    using Result = HandlerReturnType<Output>;

    LedgerRangeHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend) : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    Result
    process(Context const& ctx) const;

private:
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);
};
}  // namespace RPC
