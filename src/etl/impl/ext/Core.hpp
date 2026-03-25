#pragma once

#include "data/BackendInterface.hpp"
#include "etl/Models.hpp"
#include "util/log/Logger.hpp"

#include <xrpl/basics/base_uint.h>

#include <cstdint>
#include <memory>

namespace etl::impl {

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

}  // namespace etl::impl
