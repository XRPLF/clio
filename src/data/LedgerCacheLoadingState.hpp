#pragma once

#include "data/LedgerCacheInterface.hpp"

#include <atomic>
#include <functional>
#include <memory>

namespace data {

/**
 * @brief Interface for coordinating cache loading permissions across a cluster.
 *
 * Controls whether this node is allowed to load the ledger cache from the backend.
 * In a cluster, at most one node should load the cache at a time; this state is used
 * to gate loading until permission is granted.
 */
class LedgerCacheLoadingStateInterface {
public:
    virtual ~LedgerCacheLoadingStateInterface() = default;

    /**
     * @brief Allow this node to begin loading the cache from the backend.
     */
    virtual void
    allowLoading() = 0;

    /**
     * @brief Check whether loading has been permitted.
     * @return true if allowLoading() has been called
     */
    [[nodiscard]] virtual bool
    isLoadingAllowed() const = 0;

    /**
     * @brief Block until loading is permitted.
     * @note Returns immediately if allowLoading() was already called.
     */
    virtual void
    waitForLoadingAllowed() const = 0;

    /**
     * @brief Check whether the cache is currently being loaded from the backend.
     * @return true if the underlying cache has been marked as loading and is not yet full
     */
    [[nodiscard]] virtual bool
    isCurrentlyLoading() const = 0;

    /**
     * @brief Create a clone that shares the same loading-allowed flag.
     * @note Clones share the @c isLoadingAllowed_ atomic, so allowLoading() on any
     *       copy is visible to all clones.
     * @return A new instance sharing the same loading permission state
     */
    [[nodiscard]] virtual std::unique_ptr<LedgerCacheLoadingStateInterface>
    clone() const = 0;
};

/**
 * @brief Concrete implementation of @ref LedgerCacheLoadingStateInterface.
 *
 * Stores a reference to the ledger cache to delegate isCurrentlyLoading(), and a
 * shared atomic flag for the loading-allowed coordination.
 */
class LedgerCacheLoadingState : public LedgerCacheLoadingStateInterface {
    std::reference_wrapper<LedgerCacheInterface const> cache_;
    std::shared_ptr<std::atomic_bool> isLoadingAllowed_ = std::make_shared<std::atomic_bool>(false);

public:
    /**
     * @brief Construct a new LedgerCacheLoadingState.
     * @param cache The cache whose loading status will be monitored
     */
    explicit LedgerCacheLoadingState(LedgerCacheInterface const& cache);

    /** @copydoc LedgerCacheLoadingStateInterface::allowLoading() */
    void
    allowLoading() override;

    /** @copydoc LedgerCacheLoadingStateInterface::isLoadingAllowed() */
    [[nodiscard]] bool
    isLoadingAllowed() const override;

    /** @copydoc LedgerCacheLoadingStateInterface::waitForLoadingAllowed() */
    void
    waitForLoadingAllowed() const override;

    /** @copydoc LedgerCacheLoadingStateInterface::isCurrentlyLoading() */
    [[nodiscard]] bool
    isCurrentlyLoading() const override;

    /** @copydoc LedgerCacheLoadingStateInterface::clone() */
    [[nodiscard]] std::unique_ptr<LedgerCacheLoadingStateInterface>
    clone() const override;
};

}  // namespace data
