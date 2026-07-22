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
        xrpl::uint256 const& bookBase
    ) const;

    void
    writeSuccessors(uint32_t seq) const;

    void
    writeEdgeKeys(std::uint32_t seq, auto const& edgeKeys) const;
};

}  // namespace etl::impl
