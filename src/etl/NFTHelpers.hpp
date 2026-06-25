/** @file */
#pragma once

#include "data/DBHelpers.hpp"

#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace etl {

/**
 * @brief Get the NFT URI change data from a NFToken Modify transaction
 *
 * @param txMeta Transaction metadata
 * @param sttx The transaction
 * @return NFT URI change data as a pair of transactions and optional NFTsData
 */
std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNftokenModifyData(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx);

/**
 * @brief Get the NFT Token mint data from a transaction
 *
 * @param txMeta Transaction metadata
 * @param sttx The transaction
 * @return NFT Token mint data as a pair of transactions and optional NFTsData
 */
std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenMintData(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx);

/**
 * @brief Get the NFT Token burn data from a transaction
 *
 * @param txMeta Transaction metadata
 * @param sttx The transaction
 * @return NFT Token burn data as a pair of transactions and optional NFTsData
 */
std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenBurnData(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx);

/**
 * @brief Get the NFT Token accept offer data from a transaction
 *
 * @param txMeta Transaction metadata
 * @param sttx The transaction
 * @return NFT Token accept offer data as a pair of transactions and optional NFTsData
 */
std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenAcceptOfferData(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx);

/**
 * @brief Get the NFT Token cancel offer data from a transaction
 *
 * @param txMeta Transaction metadata
 * @param sttx The transaction
 * @return NFT Token cancel offer data as a pair of transactions and optional NFTsData
 */
std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenCancelOfferData(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx);

/**
 * @brief Get the NFT Token create offer data from a transaction
 *
 * @param txMeta Transaction metadata
 * @param sttx The transaction
 * @return NFT Token create offer data as a pair of transactions and optional NFTsData
 */
std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTokenCreateOfferData(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx);

/**
 * @brief Pull NFT data from TX via ETLService.
 *
 * @param txMeta Transaction metadata
 * @param sttx The transaction
 * @return NFT transactions data as a pair of transactions and optional NFTsData
 */
std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTDataFromTx(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx);

/**
 * @brief Pull NFT data from ledger object via loadInitialLedger.
 *
 * @param seq The ledger sequence to pull for
 * @param key The owner key
 * @param blob Object data as blob
 * @return The NFT data as a vector
 */
std::vector<NFTsData>
getNFTDataFromObj(std::uint32_t seq, std::string const& key, std::string const& blob);

/**
 * @brief Get the unique NFTs data from a vector of NFTsData happening in the same ledger. For
 example, if a NFT has
 * both accept offer and burn happening in the same ledger,we only keep the final state of the NFT.

 * @param nfts The NFTs data to filter, happening in the same ledger
 * @return The unique NFTs data
 */
std::vector<NFTsData>
getUniqueNFTsDatas(std::vector<NFTsData> const& nfts);

}  // namespace etl
