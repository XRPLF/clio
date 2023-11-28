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

#include "rpc/handlers/LedgerRange.h"

#include "rpc/JS.h"
#include "rpc/common/Types.h"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <ripple/protocol/jss.h>

#include <optional>

namespace rpc {

LedgerRangeHandler::Result
LedgerRangeHandler::process([[maybe_unused]] Context const& ctx) const
{
    // note: we can't get here if range is not available so it's safe
    return Output{sharedPtrBackend_->fetchLedgerRange().value()};
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, LedgerRangeHandler::Output const& output)
{
    jv = boost::json::object{
        {JS(ledger_index_min), output.range.minSequence},
        {JS(ledger_index_max), output.range.maxSequence},
    };
}

}  // namespace rpc
