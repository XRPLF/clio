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

#include "etlng/impl/Extraction.hpp"

#include "data/DBHelpers.hpp"
#include "data/Types.hpp"
#include "etl/LedgerFetcherInterface.hpp"
#include "etl/impl/LedgerFetcher.hpp"
#include "etlng/Models.hpp"
#include "util/Assert.hpp"
#include "util/LedgerUtils.hpp"
#include "util/Profiler.hpp"
#include "util/log/Logger.hpp"

#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxMeta.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace etlng::impl {

model::Object::ModType
extractModType(PBModType type)
{
    switch (type) {
        case PBObjType::UNSPECIFIED:
            return model::Object::ModType::Unspecified;
        case PBObjType::CREATED:
            return model::Object::ModType::Created;
        case PBObjType::MODIFIED:
            return model::Object::ModType::Modified;
        case PBObjType::DELETED:
            return model::Object::ModType::Deleted;
        default:  // some gRPC system values that we don't care about
            ASSERT(false, "Tried to extract bogus mod type '{}'", PBObjType::ModificationType_Name(type));
    }

    std::unreachable();
}

model::Transaction
extractTx(PBTxType tx, uint32_t seq)
{
    auto raw = std::move(*tx.mutable_transaction_blob());
    ripple::SerialIter it{raw.data(), raw.size()};
    ripple::STTx const sttx{it};
    ripple::TxMeta meta{sttx.getTransactionID(), seq, tx.metadata_blob()};

    return {
        .raw = std::move(raw),
        .metaRaw = std::move(*tx.mutable_metadata_blob()),
        .sttx = sttx,  // trivially copyable
        .meta = std::move(meta),
        .id = sttx.getTransactionID(),
        .key = uint256ToString(sttx.getTransactionID()),
        .type = sttx.getTxnType()
    };
}

std::vector<model::Transaction>
extractTxs(PBTxListType transactions, uint32_t seq)
{
    namespace rg = std::ranges;
    namespace vs = std::views;

    // TODO: should be simplified with ranges::to<> when available
    std::vector<model::Transaction> output;
    output.reserve(transactions.size());

    rg::move(transactions | vs::transform([seq](auto&& tx) { return extractTx(tx, seq); }), std::back_inserter(output));
    return output;
}

model::Object
extractObj(PBObjType obj)
{
    auto const key = ripple::uint256::fromVoidChecked(obj.key());
    ASSERT(key.has_value(), "Failed to deserialize key from void");

    auto const valueOr = [](std::string const& maybe, std::string fallback) -> std::string {
        if (maybe.empty())
            return fallback;
        return maybe;
    };

    return {
        .key = *key,  // trivially copyable
        .keyRaw = std::move(*obj.mutable_key()),
        .data = {obj.mutable_data()->begin(), obj.mutable_data()->end()},
        .dataRaw = std::move(*obj.mutable_data()),
        .successor = valueOr(obj.successor(), uint256ToString(data::firstKey)),
        .predecessor = valueOr(obj.predecessor(), uint256ToString(data::lastKey)),
        .type = extractModType(obj.mod_type()),
    };
}

std::vector<model::Object>
extractObjs(PBObjListType objects)
{
    namespace rg = std::ranges;
    namespace vs = std::views;

    // TODO: should be simplified with ranges::to<> when available
    std::vector<model::Object> output;
    output.reserve(objects.size());

    rg::move(objects | vs::transform([](auto&& obj) { return extractObj(obj); }), std::back_inserter(output));
    return output;
}

model::BookSuccessor
extractSuccessor(PBBookSuccessorType successor)
{
    return {
        .firstBook = successor.first_book(),
        .bookBase = successor.book_base(),
    };
}

std::optional<std::vector<model::BookSuccessor>>
maybeExtractSuccessors(PBLedgerResponseType const& data)
{
    namespace rg = std::ranges;
    namespace vs = std::views;

    if (not data.object_neighbors_included())
        return std::nullopt;

    // TODO: should be simplified with ranges::to<> when available
    std::vector<model::BookSuccessor> output;
    output.reserve(data.book_successors_size());

    rg::copy(
        data.book_successors() | vs::transform([](auto&& obj) { return extractSuccessor(obj); }),
        std::back_inserter(output)
    );
    return output;
}

auto
Extractor::unpack()
{
    return [](auto&& data) {
        auto header = ::util::deserializeHeader(ripple::makeSlice(data.ledger_header()));

        return std::make_optional<model::LedgerData>({
            .transactions =
                extractTxs(std::move(*data.mutable_transactions_list()->mutable_transactions()), header.seq),
            .objects = extractObjs(std::move(*data.mutable_ledger_objects()->mutable_objects())),
            .successors = maybeExtractSuccessors(data),
            .edgeKeys = std::nullopt,
            .header = header,
            .rawHeader = std::move(*data.mutable_ledger_header()),
            .seq = header.seq,
        });
    };
}

std::optional<model::LedgerData>
Extractor::extractLedgerWithDiff(uint32_t seq)
{
    LOG(log_.debug()) << "Extracting DIFF " << seq;

    auto const [batch, time] = ::util::timed<std::chrono::duration<double>>([this, seq] {
        return fetcher_->fetchDataAndDiff(seq).and_then(unpack());
    });

    LOG(log_.debug()) << "Extracted and Transformed diff for " << seq << " in " << time << "ms";

    // can be nullopt. this means that either the server is stopping or another node took over ETL writing.
    return batch;
}

std::optional<model::LedgerData>
Extractor::extractLedgerOnly(uint32_t seq)
{
    LOG(log_.debug()) << "Extracting FULL " << seq;

    auto const [batch, time] = ::util::timed<std::chrono::duration<double>>([this, seq] {
        return fetcher_->fetchData(seq).and_then(unpack());
    });

    LOG(log_.debug()) << "Extracted and Transformed full ledger for " << seq << " in " << time << "ms";

    // can be nullopt. this means that either the server is stopping or another node took over ETL writing.
    return batch;
}

}  // namespace etlng::impl
