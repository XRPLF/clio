#pragma once

namespace etl {

/**
 * @brief The interface of a handler for amendment blocking
 */
struct AmendmentBlockHandlerInterface {
    virtual ~AmendmentBlockHandlerInterface() = default;

    /**
     * @brief The function to call once an amendment block has been discovered
     */
    virtual void
    notifyAmendmentBlocked() = 0;

    /**
     * @brief Stop the block handler from repeatedly executing
     */
    virtual void
    stop() = 0;
};

}  // namespace etl
