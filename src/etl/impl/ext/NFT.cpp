#include "etl/impl/ext/NFT.hpp"

#include "data/BackendInterface.hpp"
#include "data/DBHelpers.hpp"
#include "etl/Models.hpp"
#include "etl/NFTHelpers.hpp"
#include "util/log/Logger.hpp"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace etl::impl {

NFTExt::NFTExt(std::shared_ptr<BackendInterface> backend) : backend_(std::move(backend))
{
}

void
NFTExt::onLedgerData(model::LedgerData const& data)
{
    LOG(log_.trace()) << "got TXS cnt = " << data.transactions.size()
                      << "; OBJS size = " << data.objects.size();
    writeNFTs(data);
}

void
NFTExt::onInitialObject(uint32_t seq, model::Object const& obj)
{
    LOG(log_.trace()) << "got initial object with key = " << obj.key;
    backend_->writeNFTs(getNFTDataFromObj(seq, obj.keyRaw, obj.dataRaw));
}

void
NFTExt::onInitialData(model::LedgerData const& data)
{
    LOG(log_.trace()) << "got initial TXS cnt = " << data.transactions.size();
    writeNFTs(data);
}

void
NFTExt::writeNFTs(model::LedgerData const& data)
{
    std::vector<NFTsData> nfts;
    std::vector<NFTTransactionsData> nftTxs;

    for (auto const& tx : data.transactions) {
        auto const [txs, maybeNFT] = getNFTDataFromTx(tx.meta, tx.sttx);
        nftTxs.insert(nftTxs.end(), txs.begin(), txs.end());
        if (maybeNFT)
            nfts.push_back(*maybeNFT);
    }

    // This is uniqued so that we only write latest modification (as in previous implementation)
    backend_->writeNFTs(getUniqueNFTsDatas(nfts));
    backend_->writeNFTTransactions(nftTxs);
}

}  // namespace etl::impl
