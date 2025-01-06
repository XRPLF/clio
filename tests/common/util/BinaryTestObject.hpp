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

#include "etlng/Models.hpp"
#include "etlng/impl/Extraction.hpp"

#include <gtest/gtest.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <string>
#include <utility>

namespace util {

[[maybe_unused, nodiscard]] std::pair<std::string, std::string>
createNftTxAndMetaBlobs();

[[maybe_unused, nodiscard]] std::pair<ripple::STTx, ripple::TxMeta>
createNftTxAndMeta();

[[maybe_unused, nodiscard]] etlng::model::Transaction
createTransaction(ripple::TxType type);

[[maybe_unused, nodiscard]] etlng::model::Object
createObject();

[[maybe_unused, nodiscard]] etlng::model::BookSuccessor
createSuccessor();

[[maybe_unused, nodiscard]] etlng::impl::PBLedgerResponseType
createDataAndDiff();

[[maybe_unused, nodiscard]] etlng::impl::PBLedgerResponseType
createData();

}  // namespace util
