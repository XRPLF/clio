//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "etlng/impl/ext/MPT.hpp"

#include "data/BackendInterface.hpp"
#include "data/DBHelpers.hpp"
#include "etl/MPTHelpers.hpp"
#include "etlng/Models.hpp"
#include "util/log/Logger.hpp"

#include <xrpl/basics/strHex.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace etlng::impl {

MPTExt::MPTExt(std::shared_ptr<BackendInterface> backend) : backend_(std::move(backend))
{
}

void
MPTExt::onLedgerData(model::LedgerData const& data)
{
    LOG(log_.trace()) << "got TXS cnt = " << data.transactions.size() << "; OBJS size = " << data.objects.size();
    writeMPTHoldersFromTransactions(data);
}

void
MPTExt::onInitialObject(uint32_t, model::Object const& obj)
{
    LOG(log_.trace()) << "got initial object with key: " << ripple::strHex(obj.key);
    if (auto const mptHolder = etl::getMPTHolderFromObj(obj.keyRaw, obj.dataRaw); mptHolder.has_value())
        backend_->writeMPTHolders({*mptHolder});
}

void
MPTExt::onInitialData(model::LedgerData const& data)
{
    LOG(log_.trace()) << "got initial TXS cnt = " << data.transactions.size();
    writeMPTHoldersFromTransactions(data);
}

void
MPTExt::writeMPTHoldersFromTransactions(model::LedgerData const& data)
{
    std::vector<MPTHolderData> holders;

    for (auto const& tx : data.transactions) {
        if (auto const mptHolder = etl::getMPTHolderFromTx(tx.meta, tx.sttx); mptHolder.has_value())
            holders.push_back(*mptHolder);
    }

    if (not holders.empty())
        backend_->writeMPTHolders(holders);
}

}  // namespace etlng::impl
