#pragma once

#include "data/BackendInterface.hpp"
#include "etl/Models.hpp"
#include "util/log/Logger.hpp"

#include <cstdint>
#include <memory>

namespace etl::impl {

class NFTExt {
    std::shared_ptr<BackendInterface> backend_;
    util::Logger log_{"ETL"};

public:
    NFTExt(std::shared_ptr<BackendInterface> backend);

    void
    onLedgerData(model::LedgerData const& data);

    void
    onInitialObject(uint32_t seq, model::Object const& obj);

    void
    onInitialData(model::LedgerData const& data);

private:
    void
    writeNFTs(model::LedgerData const& data);
};

}  // namespace etl::impl
