#pragma once

#include "util/Concepts.hpp"

#include <boost/json/object.hpp>
#include <fmt/format.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/proto/org/xrpl/rpc/v1/get_ledger.pb.h>
#include <xrpl/proto/org/xrpl/rpc/v1/ledger.pb.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief This namespace contains the data models used by the ETL pipeline and its extensions.
 */
namespace etl::model {

/**
 * @brief A specification for the Registry.
 *
 * This specification simply defines the transaction types that are to be filtered out from the
 * incoming transactions by the Registry for its `onTransaction` and `onInitialTransaction` hooks.
 * It's a compilation error to list the same transaction type more than once.
 *
 * @tparam Types The transaction types the Registry should deliver to the extension
 */
template <xrpl::TxType... Types>
    requires(util::hasNoDuplicates(Types...))  // NOLINT(readability-redundant-parentheses)
struct Spec {
    static constexpr bool kSpecTag = true;  ///< Marker used by the Registry to detect a Spec

    /**
     * @brief Checks if the transaction type was requested.
     *
     * @param type The transaction type
     * @return true if the transaction was requested; false otherwise
     */
    [[nodiscard]] static constexpr bool
    wants(xrpl::TxType type) noexcept
    {
        return ((Types == type) || ...);
    }
};

/**
 * @brief Represents a single transaction on the ledger.
 */
struct Transaction {
    std::string raw;      ///< Raw serialized transaction blob
    std::string metaRaw;  ///< Raw serialized transaction metadata blob

    // unpacked blob and meta
    xrpl::STTx sttx;    ///< Deserialized transaction
    xrpl::TxMeta meta;  ///< Deserialized transaction metadata

    // commonly used stuff
    xrpl::uint256 id;   ///< Transaction hash
    std::string key;    ///< The above id as a string of 32 raw bytes
    xrpl::TxType type;  ///< Transaction type

    /**
     * @brief Compares Transaction objects to each other without considering sttx and meta fields
     * @param other The Transaction to compare to
     * @return true if transaction is equivalent; false otherwise
     */
    bool
    operator==(Transaction const& other) const
    {
        return raw == other.raw                                           //
            and metaRaw == other.metaRaw                                  //
            and sttx.getTransactionID() == other.sttx.getTransactionID()  //
            and meta.getTxID() == other.meta.getTxID()                    //
            and id == other.id                                            //
            and key == other.key                                          //
            and type == other.type;
    }
};

/**
 * @brief Represents a single object on the ledger.
 */
struct Object {
    /**
     * @brief Modification type for the object.
     */
    enum class ModType : int {
        Unspecified = 0,  ///< Modification type was not provided
        Created = 1,      ///< Object was created in this ledger
        Modified = 2,     ///< Object was modified in this ledger
        Deleted = 3,      ///< Object was deleted in this ledger
    };

    xrpl::uint256 key;        ///< Key of the ledger object
    std::string keyRaw;       ///< Key of the ledger object as raw bytes
    xrpl::Blob data;          ///< Serialized object data; empty if the object was deleted
    std::string dataRaw;      ///< Serialized object data as a string
    std::string successor;    ///< Raw key of the next object in the ledger (first key if unset)
    std::string predecessor;  ///< Raw key of the previous object in the ledger (last key if unset)

    ModType type;  ///< How the object was modified in this ledger

    bool
    operator==(Object const&) const = default;
};

/**
 * @brief Represents a book successor.
 */
struct BookSuccessor {
    std::string firstBook;  ///< Raw key of the first offer directory page of the book
    std::string bookBase;   ///< Raw key of the book base (book directory without the quality)

    bool
    operator==(BookSuccessor const&) const = default;
};

/**
 * @brief Represents an entire ledger diff worth of transactions and objects.
 */
struct LedgerData {
    std::vector<Transaction> transactions;                 ///< Transactions in the ledger
    std::vector<Object> objects;                           ///< Objects changed by the ledger
    std::optional<std::vector<BookSuccessor>> successors;  ///< Book successors, if provided
    std::optional<std::vector<std::string>> edgeKeys;      ///< Last key of each initial load range

    xrpl::LedgerHeader header;  ///< Deserialized ledger header
    std::string rawHeader;      ///< Serialized ledger header
    uint32_t seq;               ///< Sequence of the ledger

    /**
     * @brief Compares LedgerData objects to each other without considering the header field
     * @param other The LedgerData to compare to
     * @return true if data is equivalent; false otherwise
     */
    bool
    operator==(LedgerData const& other) const
    {
        auto const serialized = [](auto const& hdr) {
            xrpl::Serializer ser;
            xrpl::addRaw(hdr, ser);
            return ser.getString();
        };

        return transactions == other.transactions               //
            and objects == other.objects                        //
            and successors == other.successors                  //
            and edgeKeys == other.edgeKeys                      //
            and serialized(header) == serialized(other.header)  //
            and rawHeader == other.rawHeader                    //
            and seq == other.seq;
    }
};

/**
 * @brief Represents a task for the extractors
 */
struct Task {
    /**
     * @brief Represents the priority of the task
     */
    enum class Priority : uint8_t {
        Lower = 0u,   ///< Low priority, used for backfilling older ledgers
        Higher = 1u,  ///< High priority, used for extracting new ledgers going forward
    };

    Priority priority;  ///< Priority of the task
    uint32_t seq;       ///< Sequence of the ledger to extract
};

}  // namespace etl::model
