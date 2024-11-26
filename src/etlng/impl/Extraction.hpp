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

#include "etl/LedgerFetcherInterface.hpp"
#include "etl/impl/LedgerFetcher.hpp"
#include "etlng/ExtractorInterface.hpp"
#include "etlng/Models.hpp"
#include "util/log/Logger.hpp"

#include <google/protobuf/repeated_ptr_field.h>
#include <sys/types.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/proto/org/xrpl/rpc/v1/get_ledger.pb.h>
#include <xrpl/proto/org/xrpl/rpc/v1/ledger.pb.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace etlng::impl {

using PBObjType = org::xrpl::rpc::v1::RawLedgerObject;
using PBModType = PBObjType::ModificationType;
using PBTxType = org::xrpl::rpc::v1::TransactionAndMetadata;
using PBTxListType = google::protobuf::RepeatedPtrField<PBTxType>;
using PBObjListType = google::protobuf::RepeatedPtrField<PBObjType>;
using PBBookSuccessorType = org::xrpl::rpc::v1::BookSuccessor;
using PBLedgerResponseType = org::xrpl::rpc::v1::GetLedgerResponse;

[[nodiscard]] model::Object::ModType
extractModType(PBModType type);

[[nodiscard]] model::Transaction
extractTx(PBTxType tx, uint32_t seq);

[[nodiscard]] std::vector<model::Transaction>
extractTxs(PBTxListType transactions, uint32_t seq);

[[nodiscard]] model::Object
extractObj(PBObjType obj);

[[nodiscard]] std::vector<model::Object>
extractObjs(PBObjListType objects);

[[nodiscard]] model::BookSuccessor
extractSuccessor(PBBookSuccessorType successor);

[[nodiscard]] std::optional<std::vector<model::BookSuccessor>>
maybeExtractSuccessors(PBLedgerResponseType const& data);

// fetches the data in gRPC and transforms to local representation
class Extractor : public ExtractorInterface {
    std::shared_ptr<etl::LedgerFetcherInterface> fetcher_;

    util::Logger log_{"ETL"};

private:
    [[nodiscard]] static auto
    unpack();

public:
    Extractor(std::shared_ptr<etl::LedgerFetcherInterface> fetcher) : fetcher_(std::move(fetcher))
    {
    }

    [[nodiscard]] std::optional<model::LedgerData>
    extractLedgerWithDiff(uint32_t seq) override;

    [[nodiscard]] std::optional<model::LedgerData>
    extractLedgerOnly(uint32_t seq) override;
};

}  // namespace etlng::impl
