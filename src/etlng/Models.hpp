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

#include "util/Concepts.hpp"

#include <boost/json/object.hpp>
#include <fmt/core.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/proto/org/xrpl/rpc/v1/get_ledger.pb.h>
#include <xrpl/proto/org/xrpl/rpc/v1/ledger.pb.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace etlng::model {

/**
 * @brief A specification for the Registry.
 *
 * This specification simply defines the transaction types that are to be filtered out from the incoming transactions by
 * the Registry for its `onTransaction` and `onInitialTransaction` hooks.
 * It's a compilation error to list the same transaction type more than once.
 */
template <ripple::TxType... Types>
    requires(util::hasNoDuplicates(Types...))
struct Spec {
    static constexpr bool kSPEC_TAG = true;

    /**
     * @brief Checks if the transaction type was requested.
     *
     * @param type The transaction type
     * @return true if the transaction was requested; false otherwise
     */
    [[nodiscard]] static constexpr bool
    wants(ripple::TxType type) noexcept
    {
        return ((Types == type) || ...);
    }
};

/**
 * @brief Represents a single transaction on the ledger.
 */
struct Transaction {
    std::string raw;  // raw binary blob
    std::string metaRaw;

    // unpacked blob and meta
    ripple::STTx sttx;
    ripple::TxMeta meta;

    // commonly used stuff
    ripple::uint256 id;
    std::string key;  // key is the above id as a string of 32 characters
    ripple::TxType type;
};

/**
 * @brief Represents a single object on the ledger.
 */
struct Object {
    /**
     * @brief Modification type for the object.
     */
    enum class ModType : int {
        Unspecified = 0,
        Created = 1,
        Modified = 2,
        Deleted = 3,
    };

    ripple::uint256 key;
    std::string keyRaw;
    ripple::Blob data;
    std::string dataRaw;
    std::string successor;
    std::string predecessor;

    ModType type;
};

/**
 * @brief Represents a book successor.
 */
struct BookSuccessor {
    std::string firstBook;
    std::string bookBase;
};

/**
 * @brief Represents an entire ledger diff worth of transactions and objects.
 */
struct LedgerData {
    std::vector<Transaction> transactions;
    std::vector<Object> objects;
    std::optional<std::vector<BookSuccessor>> successors;
    std::optional<std::vector<std::string>> edgeKeys;

    ripple::LedgerHeader header;
    std::string rawHeader;
    uint32_t seq;
};

}  // namespace etlng::model
