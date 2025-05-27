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
#include "etlng/Models.hpp"
#include "util/log/Logger.hpp"

#include <xrpl/basics/base_uint.h>

#include <cstdint>
#include <memory>

namespace etlng::impl {

class CoreExt {
    std::shared_ptr<BackendInterface> backend_;

    util::Logger log_{"ETL"};

public:
    CoreExt(std::shared_ptr<BackendInterface> backend);

    void
    onLedgerData(model::LedgerData const& data);

    void
    onInitialData(model::LedgerData const& data);

    void
    onInitialObject(uint32_t seq, model::Object const& obj);

    void
    onObject(uint32_t seq, model::Object const& obj);

private:
    void
    insertTransactions(model::LedgerData const& data);
};

}  // namespace etlng::impl
