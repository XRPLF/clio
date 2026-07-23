#pragma once

#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/Collection.hpp"
#include "data/cassandra/impl/ManagedObject.hpp"
#include "data/cassandra/impl/Tuple.hpp"
#include "util/UnsupportedType.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cassandra.h>
#include <fmt/format.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/STAccount.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace data::cassandra::impl {

class Statement : public ManagedObject<CassStatement> {
    static constexpr auto kDeleter = [](CassStatement* ptr) { cass_statement_free(ptr); };

public:
    /**
     * @brief Construct a new statement with optionally provided arguments.
     *
     * Note: it's up to the user to make sure the bound parameters match
     * the format of the query (e.g. amount of '?' matches count of args).
     */
    template <typename... Args>
    explicit Statement(std::string_view query, Args&&... args)
        : ManagedObject{cass_statement_new_n(query.data(), query.size(), sizeof...(args)), kDeleter}
    {
        // TODO: figure out how to set consistency level in config
        // NOTE: Keyspace doesn't support QUORUM at write level
        // cass_statement_set_consistency(*this, CASS_CONSISTENCY_LOCAL_QUORUM);
        cass_statement_set_is_idempotent(*this, cass_true);
        bind<Args...>(std::forward<Args>(args)...);
    }

    /* implicit */ Statement(CassStatement* ptr) : ManagedObject{ptr, kDeleter}
    {
        // cass_statement_set_consistency(*this, CASS_CONSISTENCY_LOCAL_QUORUM);
        cass_statement_set_is_idempotent(*this, cass_true);
    }

    /**
     * @brief Binds the given arguments to the statement.
     *
     * @param args Arguments to bind
     */
    template <typename... Args>
    void
    bind(Args&&... args) const
    {
        std::size_t idx = 0;  // NOLINT(misc-const-correctness)
        (this->bindAt<Args>(idx++, std::forward<Args>(args)), ...);
    }

    /**
     * @brief Binds an argument to a specific index.
     *
     * @param idx The index of the argument
     * @param value The value to bind it to
     */
    template <typename Type>
    void
    bindAt(std::size_t const idx, Type&& value) const
    {
        using std::to_string;
        auto throwErrorIfNeeded = [idx](CassError rc, std::string_view label) {
            if (rc != CASS_OK) {
                throw std::logic_error(
                    fmt::format("[{}] at idx {}: {}", label, idx, cass_error_desc(rc))
                );
            }
        };

        auto bindBytes = [this, idx](auto const* data, size_t size) {
            return cass_statement_bind_bytes(
                *this, idx, static_cast<cass_byte_t const*>(data), size
            );
        };

        using DecayedType = std::decay_t<Type>;
        using UCharVectorType = std::vector<unsigned char>;
        using UintTupleType = std::tuple<uint32_t, uint32_t>;
        using UintByteTupleType = std::tuple<uint32_t, xrpl::uint256>;
        using ByteVectorType = std::vector<xrpl::uint256>;

        if constexpr (
            std::is_same_v<DecayedType, xrpl::uint256> || std::is_same_v<DecayedType, xrpl::uint192>
        ) {
            auto const rc = bindBytes(value.data(), value.size());
            throwErrorIfNeeded(rc, "Bind xrpl::base_uint");
        } else if constexpr (std::is_same_v<DecayedType, xrpl::AccountID>) {
            auto const rc = bindBytes(value.data(), value.size());
            throwErrorIfNeeded(rc, "Bind xrpl::AccountID");
        } else if constexpr (std::is_same_v<DecayedType, UCharVectorType>) {
            auto const rc = bindBytes(value.data(), value.size());
            throwErrorIfNeeded(rc, "Bind vector<unsigned char>");
        } else if constexpr (std::is_convertible_v<DecayedType, std::string>) {
            // reinterpret_cast is needed here :'(
            auto const rc =
                bindBytes(reinterpret_cast<unsigned char const*>(value.data()), value.size());
            throwErrorIfNeeded(rc, "Bind string (as bytes)");
        } else if constexpr (std::is_convertible_v<DecayedType, Text>) {
            auto const rc =
                cass_statement_bind_string_n(*this, idx, value.text.c_str(), value.text.size());
            throwErrorIfNeeded(rc, "Bind string (as TEXT)");
        } else if constexpr (
            std::is_same_v<DecayedType, UintTupleType> ||
            std::is_same_v<DecayedType, UintByteTupleType>
        ) {
            auto const rc = cass_statement_bind_tuple(*this, idx, Tuple{std::forward<Type>(value)});
            throwErrorIfNeeded(rc, "Bind tuple<uint32, uint32> or <uint32_t, xrpl::uint256>");
        } else if constexpr (std::is_same_v<DecayedType, ByteVectorType>) {
            auto const rc =
                cass_statement_bind_collection(*this, idx, Collection{std::forward<Type>(value)});
            throwErrorIfNeeded(rc, "Bind collection");
        } else if constexpr (std::is_same_v<DecayedType, bool>) {
            auto const rc = cass_statement_bind_bool(*this, idx, value ? cass_true : cass_false);
            throwErrorIfNeeded(rc, "Bind bool");
        } else if constexpr (std::is_same_v<DecayedType, Limit>) {
            auto const rc = cass_statement_bind_int32(*this, idx, value.limit);
            throwErrorIfNeeded(rc, "Bind limit (int32)");
        } else if constexpr (std::is_convertible_v<DecayedType, boost::uuids::uuid>) {
            auto const uuidStr = boost::uuids::to_string(value);
            CassUuid cassUuid;
            auto rc = cass_uuid_from_string(uuidStr.c_str(), &cassUuid);
            throwErrorIfNeeded(rc, "CassUuid from string");
            rc = cass_statement_bind_uuid(*this, idx, cassUuid);
            throwErrorIfNeeded(rc, "Bind boost::uuid");
            // clio only uses bigint (int64_t) so we convert any incoming type
        } else if constexpr (std::is_convertible_v<DecayedType, int64_t>) {
            auto const rc = cass_statement_bind_int64(*this, idx, value);
            throwErrorIfNeeded(rc, "Bind int64");
        } else {
            // type not supported for binding
            static_assert(util::Unsupported<DecayedType>);
        }
    }
};

/**
 * @brief Represents a prepared statement on the DB side.
 *
 * This is used to produce Statement objects that can be executed.
 */
class PreparedStatement : public ManagedObject<CassPrepared const> {
    static constexpr auto kDeleter = [](CassPrepared const* ptr) { cass_prepared_free(ptr); };

public:
    /* implicit */ PreparedStatement(CassPrepared const* ptr) : ManagedObject{ptr, kDeleter}
    {
    }

    /**
     * @brief Bind the given arguments and produce a ready to execute Statement.
     *
     * @param args The arguments to bind
     * @return A bound and ready to execute Statement object
     */
    template <typename... Args>
    [[nodiscard]] Statement
    bind(Args&&... args) const
    {
        Statement statement = cass_prepared_bind(*this);
        statement.bind<Args...>(std::forward<Args>(args)...);
        return statement;
    }
};

}  // namespace data::cassandra::impl
