#pragma once

#include "data/BackendInterface.hpp"
#include "etl/AmendmentBlockHandlerInterface.hpp"
#include "etl/InitialLoadObserverInterface.hpp"
#include "etl/LoaderInterface.hpp"
#include "etl/Models.hpp"
#include "etl/RegistryInterface.hpp"
#include "etl/SystemState.hpp"
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

namespace etl::impl {

class Loader : public LoaderInterface, public InitialLoadObserverInterface {
    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<RegistryInterface> registry_;
    std::shared_ptr<AmendmentBlockHandlerInterface> amendmentBlockHandler_;
    std::shared_ptr<SystemState> state_;

    std::size_t initialLoadWrittenObjects_{0u};
    std::size_t initialLoadWrites_{0u};
    util::Logger log_{"ETL"};

public:
    using RawLedgerObjectType = org::xrpl::rpc::v1::RawLedgerObject;
    using GetLedgerResponseType = org::xrpl::rpc::v1::GetLedgerResponse;
    using OptionalGetLedgerResponseType = std::optional<GetLedgerResponseType>;

    Loader(
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<RegistryInterface> registry,
        std::shared_ptr<AmendmentBlockHandlerInterface> amendmentBlockHandler,
        std::shared_ptr<SystemState> state
    );

    Loader(Loader const&) = delete;
    Loader(Loader&&) = delete;
    Loader&
    operator=(Loader const&) = delete;
    Loader&
    operator=(Loader&&) = delete;

    std::expected<void, LoaderError>
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

}  // namespace etl::impl
