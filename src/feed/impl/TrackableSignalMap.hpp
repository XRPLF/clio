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

#include "feed/impl/TrackableSignal.hpp"

#include <boost/signals2.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace feed::impl {

template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Class to manage a map of key and its associative signal.
 * @param Key The type of the key.
 * @param Session The type of the object that will be tracked, when the object is destroyed, the connection will be
 * removed lazily.
 * @param Args The types of the arguments that will be passed to the slot
 */
template <Hashable Key, typename Session, typename... Args>
class TrackableSignalMap {
    using ConnectionPtr = Session*;
    using ConnectionSharedPtr = std::shared_ptr<Session>;

    mutable std::mutex mutex_;
    std::unordered_map<Key, TrackableSignal<Session, Args...>> signalsMap_;

public:
    /**
     * @brief Connect a slot to the signal, the slot will be called when the signal is emitted and trackable is still
     * alive.
     *
     * @param trackable Track this object's lifttime, if the object is destroyed, the connection will be removed lazily.
     * When the slot is being called, the object is guaranteed to be alive.
     * @param key The key to the signal.
     * @param slot The slot connecting to the signal, the slot will be called when the assocaiative signal is emitted.
     * @return true if the connection is successfully added, false if the connection already exists for the key.
     */
    bool
    connectTrackableSlot(ConnectionSharedPtr const& trackable, Key const& key, std::function<void(Args...)> slot)
    {
        std::scoped_lock const lk(mutex_);
        return signalsMap_[key].connectTrackableSlot(trackable, slot);
    }

    /**
     * @brief Disconnect a slot from the key's associative signal.
     *
     * @param trackablePtr The pointer to the object that is being tracked.
     * @param key The key to the signal.
     * @return true if the connection is successfully removed, false if the connection does not exist.
     */
    bool
    disconnect(ConnectionPtr trackablePtr, Key const& key)
    {
        std::scoped_lock const lk(mutex_);
        if (!signalsMap_.contains(key))
            return false;

        auto const disconnected = signalsMap_[key].disconnect(trackablePtr);
        // clean the map if there is no connection left.
        if (disconnected && signalsMap_[key].count() == 0)
            signalsMap_.erase(key);

        return disconnected;
    }

    /**
     * @brief Emit the signal with the given key and arguments.
     *
     * @param key The key to the signal.
     * @param args The arguments to be passed to the slot.
     */
    void
    emit(Key const& key, Args const&... args)
    {
        std::scoped_lock const lk(mutex_);
        if (signalsMap_.contains(key))
            signalsMap_[key].emit(args...);
    }
};
}  // namespace feed::impl
