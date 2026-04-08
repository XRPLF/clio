#pragma once

#include "etl/Models.hpp"

#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace etl {

/**
 * @brief The interface for observing the initial ledger load
 */
struct InitialLoadObserverInterface {
    virtual ~InitialLoadObserverInterface() = default;

    /**
     * @brief Callback for each incoming batch of objects during initial ledger load
     *
     * @param seq The sequence for this batch of objects
     * @param data The batch of objects
     * @param lastKey The last key of the previous batch if there was one
     */
    virtual void
    onInitialLoadGotMoreObjects(
        uint32_t seq,
        std::vector<model::Object> const& data,
        std::optional<std::string> lastKey = std::nullopt
    ) = 0;
};

}  // namespace etl
