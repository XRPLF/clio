#pragma once

#include "data/Types.hpp"
#include "feed/impl/SingleFeedBase.hpp"
#include "rpc/BookChangesHelper.hpp"
#include "util/async/AnyExecutionContext.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/json/serialize.hpp>
#include <xrpl/protocol/LedgerHeader.h>

#include <vector>

namespace feed::impl {

/**
 * @brief Feed that publishes book changes. This feed will be published every ledger, even if there
 * are no changes. Example : {'type': 'bookChanges', 'ledger_index': 2647936, 'ledger_hash':
 * '0A5010342D8AAFABDCA58A68F6F588E1C6E58C21B63ED6CA8DB2478F58F3ECD5', 'ledger_time': 756395682,
 * 'changes': []}
 */
struct BookChangesFeed : public SingleFeedBase {
    BookChangesFeed(util::async::AnyExecutionContext& executionCtx)
        : SingleFeedBase(executionCtx, "book_changes")
    {
    }

    /**
     * @brief Publishes the book changes.
     * @param lgrInfo The ledger header.
     * @param transactions The transactions that were included in the ledger.
     */
    void
    pub(ripple::LedgerHeader const& lgrInfo,
        std::vector<data::TransactionAndMetadata> const& transactions)
    {
        SingleFeedBase::pub(boost::json::serialize(rpc::computeBookChanges(lgrInfo, transactions)));
    }
};
}  // namespace feed::impl
