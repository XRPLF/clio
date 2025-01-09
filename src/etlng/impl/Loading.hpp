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

#include "data/BackendInterface.hpp"
#include "etl/LedgerFetcherInterface.hpp"
#include "etl/impl/LedgerLoader.hpp"
#include "etlng/AmendmentBlockHandlerInterface.hpp"
#include "etlng/InitialLoadObserverInterface.hpp"
#include "etlng/LoaderInterface.hpp"
#include "etlng/Models.hpp"
#include "etlng/RegistryInterface.hpp"
#include "util/log/Logger.hpp"

#include <org/xrpl/rpc/v1/ledger.pb.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/proto/org/xrpl/rpc/v1/get_ledger.pb.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace etlng::impl {

class Loader : public LoaderInterface, public InitialLoadObserverInterface {
    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<etl::LedgerFetcherInterface> fetcher_;
    std::shared_ptr<RegistryInterface> registry_;
    std::shared_ptr<AmendmentBlockHandlerInterface> amendmentBlockHandler_;

    util::Logger log_{"ETL"};

public:
    using RawLedgerObjectType = org::xrpl::rpc::v1::RawLedgerObject;
    using GetLedgerResponseType = org::xrpl::rpc::v1::GetLedgerResponse;
    using OptionalGetLedgerResponseType = std::optional<GetLedgerResponseType>;

    Loader(
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<etl::LedgerFetcherInterface> fetcher,
        std::shared_ptr<RegistryInterface> registry,
        std::shared_ptr<AmendmentBlockHandlerInterface> amendmentBlockHandler
    );

    void
    load(model::LedgerData const& data) override;

    void
    onInitialLoadGotMoreObjects(
        uint32_t seq,
        std::vector<model::Object> const& data,
        std::optional<std::string> lastKey
    ) override;

    std::optional<ripple::LedgerHeader>
    loadInitialLedger(model::LedgerData const& data) override;
};

}  // namespace etlng::impl
