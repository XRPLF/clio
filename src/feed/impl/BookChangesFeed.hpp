//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "data/Types.hpp"
#include "feed/impl/SingleFeedBase.hpp"
#include "rpc/BookChangesHelper.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/json/serialize.hpp>
#include <ripple/protocol/LedgerHeader.h>

#include <vector>

namespace feed::impl {

/**
 * @brief Feed that publishes book changes. This feed will be published every ledger, even if there are no changes.
 *  Example : {'type': 'bookChanges', 'ledger_index': 2647936, 'ledger_hash':
 * '0A5010342D8AAFABDCA58A68F6F588E1C6E58C21B63ED6CA8DB2478F58F3ECD5', 'ledger_time': 756395682, 'changes': []}
 */
struct BookChangesFeed : public SingleFeedBase {
    BookChangesFeed(boost::asio::io_context& ioContext) : SingleFeedBase(ioContext, "book_changes")
    {
    }

    /**
     * @brief Publishes the book changes.
     * @param lgrInfo The ledger header.
     * @param transactions The transactions that were included in the ledger.
     */
    void
    pub(ripple::LedgerHeader const& lgrInfo, std::vector<data::TransactionAndMetadata> const& transactions) const
    {
        SingleFeedBase::pub(boost::json::serialize(rpc::computeBookChanges(lgrInfo, transactions)));
    }
};
}  // namespace feed::impl
