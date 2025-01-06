//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022-2024, the clio developers.

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

#include "data/Types.hpp"
#include "migration/cassandra/CassandraMigrationBackend.hpp"
#include "migration/cassandra/impl/FullTableScannerAdapterBase.hpp"

#include <boost/asio/spawn.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

namespace migration::cassandra::impl {

/**
 * @brief The description of the objects table. It has to be a TableSpec.
 */
struct TableObjectsDesc {
    using Row = std::tuple<ripple::uint256, std::uint32_t, data::Blob>;
    static constexpr char const* kPARTITION_KEY = "key";
    static constexpr char const* kTABLE_NAME = "objects";
};

/**
 * @brief The adapter for the objects table. This class is responsible for reading the objects from the
 * FullTableScanner and converting the blobs to the STObject.
 */
class ObjectsAdapter : public impl::FullTableScannerAdapterBase<TableObjectsDesc> {
public:
    using OnStateRead = std::function<void(std::uint32_t, std::optional<ripple::SLE>)>;

    /**
     * @brief Construct a new Objects Adapter object
     *
     * @param backend The backend to use
     * @param onStateRead The callback to call when a state is read
     */
    explicit ObjectsAdapter(std::shared_ptr<CassandraMigrationBackend> backend, OnStateRead onStateRead)
        : FullTableScannerAdapterBase<TableObjectsDesc>(backend), onStateRead_{std::move(onStateRead)}
    {
    }

    /**
     * @brief Called when a row is read
     *
     * @param row The row to read
     */
    void
    onRowRead(TableObjectsDesc::Row const& row) override;

private:
    OnStateRead onStateRead_;
};

}  // namespace migration::cassandra::impl
