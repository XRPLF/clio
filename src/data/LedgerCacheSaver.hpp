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

#include "data/LedgerCacheInterface.hpp"
#include "util/config/ConfigDefinition.hpp"

#include <concepts>
#include <functional>
#include <optional>
#include <string>
#include <thread>

namespace data {

/**
 * @brief A concept for a class that can save ledger cache asynchronously.
 *
 * This concept defines the interface requirements for any type that manages
 * asynchronous saving of ledger cache to persistent storage.
 */
template <typename T>
concept SomeLedgerCacheSaver = requires(T a) {
    { a.save() } -> std::same_as<void>;
    { a.waitToFinish() } -> std::same_as<void>;
};

/**
 * @brief Manages asynchronous saving of ledger cache to a file.
 *
 * This class provides functionality to save the ledger cache to a file in a separate thread,
 * allowing the main application to continue without blocking. The file path is configured
 * through the application's configuration system.
 */
class LedgerCacheSaver {
    std::optional<std::string> cacheFilePath_;
    std::reference_wrapper<LedgerCacheInterface const> cache_;
    std::optional<std::thread> savingThread_;

public:
    /**
     * @brief Constructs a LedgerCacheSaver instance.
     *
     * @param config The configuration object containing the cache file path setting
     * @param cache Reference to the ledger cache interface to be saved
     */
    LedgerCacheSaver(util::config::ClioConfigDefinition const& config, LedgerCacheInterface const& cache);

    /**
     * @brief Destructor that ensures the saving thread is properly joined.
     *
     * Waits for any ongoing save operation to complete before destruction.
     */
    ~LedgerCacheSaver();

    /**
     * @brief Initiates an asynchronous save operation of the ledger cache.
     *
     * Spawns a new thread that saves the ledger cache to the configured file path.
     * If no file path is configured, the operation is skipped. Logs the progress
     * and result of the save operation.
     */
    void
    save();

    /**
     * @brief Waits for the saving thread to complete.
     *
     * Blocks until the saving operation finishes if a thread is currently active.
     * Safe to call multiple times or when no save operation is in progress.
     */
    void
    waitToFinish();
};

}  // namespace data
