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

#include "etlng/impl/ext/NFT.hpp"

#include "data/BackendInterface.hpp"
#include "data/DBHelpers.hpp"
#include "etl/NFTHelpers.hpp"
#include "etlng/Models.hpp"
#include "util/log/Logger.hpp"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace etlng::impl {

NFTExt::NFTExt(std::shared_ptr<BackendInterface> backend) : backend_(std::move(backend))
{
}

void
NFTExt::onLedgerData(model::LedgerData const& data) const
{
    writeNFTs(data);
}

void
NFTExt::onInitialObject(uint32_t seq, model::Object const& obj) const
{
    LOG(log_.trace()) << "got initial object with key = " << obj.key;
    backend_->writeNFTs(etl::getNFTDataFromObj(seq, obj.keyRaw, obj.dataRaw));
}

void
NFTExt::onInitialData(model::LedgerData const& data) const
{
    LOG(log_.trace()) << "got initial TXS cnt = " << data.transactions.size();
    writeNFTs(data);
}

void
NFTExt::writeNFTs(model::LedgerData const& data) const
{
    std::vector<NFTsData> nfts;
    std::vector<NFTTransactionsData> nftTxs;

    for (auto const& tx : data.transactions) {
        auto const [txs, maybeNFT] = etl::getNFTDataFromTx(tx.meta, tx.sttx);
        nftTxs.insert(nftTxs.end(), txs.begin(), txs.end());
        if (maybeNFT)
            nfts.push_back(*maybeNFT);
    }

    // This is uniqued so that we only write latest modification (as in previous implementation)
    backend_->writeNFTs(etl::getUniqueNFTsDatas(nfts));
    backend_->writeNFTTransactions(nftTxs);
}

}  // namespace etlng::impl
