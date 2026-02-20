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

#pragma once

#include "data/BackendInterface.hpp"
#include "data/LedgerCacheInterface.hpp"
#include "data/Types.hpp"
#include "etl/Models.hpp"
#include "util/log/Logger.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace etl::impl {

class SuccessorExt {
    std::shared_ptr<BackendInterface> backend_;
    std::reference_wrapper<data::LedgerCacheInterface> cache_;

    util::Logger log_{"ETL"};

public:
    SuccessorExt(std::shared_ptr<BackendInterface> backend, data::LedgerCacheInterface& cache);

    void
    onInitialData(model::LedgerData const& data) const;

    void
    onInitialObjects(
        uint32_t seq,
        [[maybe_unused]] std::vector<model::Object> const& objs,
        std::string lastKey
    ) const;

    void
    onLedgerData(model::LedgerData const& data) const;

private:
    void
    writeIncludedSuccessor(uint32_t seq, model::BookSuccessor const& succ) const;

    void
    writeIncludedSuccessor(uint32_t seq, model::Object const& obj) const;

    void
    updateSuccessorFromCache(uint32_t seq, model::Object const& obj) const;

    void
    updateBookSuccessor(
        std::optional<data::LedgerObject> const& maybeSuccessor,
        auto seq,
        ripple::uint256 const& bookBase
    ) const;

    void
    writeSuccessors(uint32_t seq) const;

    void
    writeEdgeKeys(std::uint32_t seq, auto const& edgeKeys) const;
};

}  // namespace etl::impl
