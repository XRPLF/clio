#pragma once

#include "feed/impl/TrackableSignal.hpp"
#include "util/Mutex.hpp"

#include <boost/signals2.hpp>

#include <concepts>
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
 * @param Session The type of the object that will be tracked, when the object is destroyed, the
 * connection will be removed lazily.
 * @param Args The types of the arguments that will be passed to the slot
 */
template <Hashable Key, typename Session, typename... Args>
class TrackableSignalMap {
    using ConnectionPtr = Session*;
    using ConnectionSharedPtr = std::shared_ptr<Session>;

    using SignalsMap = std::unordered_map<Key, TrackableSignal<Session, Args...>>;
    util::Mutex<SignalsMap> signalsMap_;

public:
    /**
     * @brief Connect a slot to the signal, the slot will be called when the signal is emitted and
     * trackable is still alive.
     *
     * @param trackable Track this object's lifttime, if the object is destroyed, the connection
     * will be removed lazily. When the slot is being called, the object is guaranteed to be alive.
     * @param key The key to the signal.
     * @param slot The slot connecting to the signal, the slot will be called when the assocaiative
     * signal is emitted.
     * @return true if the connection is successfully added, false if the connection already exists
     * for the key.
     */
    bool
    connectTrackableSlot(
        ConnectionSharedPtr const& trackable,
        Key const& key,
        std::function<void(Args...)> slot
    )
    {
        auto map = signalsMap_.template lock<std::scoped_lock>();
        return map->operator[](key).connectTrackableSlot(trackable, slot);
    }

    /**
     * @brief Disconnect a slot from the key's associative signal.
     *
     * @param trackablePtr The pointer to the object that is being tracked.
     * @param key The key to the signal.
     * @return true if the connection is successfully removed, false if the connection does not
     * exist.
     */
    bool
    disconnect(ConnectionPtr trackablePtr, Key const& key)
    {
        auto map = signalsMap_.template lock<std::scoped_lock>();
        if (!map->contains(key))
            return false;

        auto const disconnected = map->operator[](key).disconnect(trackablePtr);
        // clean the map if there is no connection left.
        if (disconnected && map->operator[](key).count() == 0)
            map->erase(key);

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
        auto map = signalsMap_.template lock<std::scoped_lock>();
        if (map->contains(key))
            map->operator[](key).emit(args...);
    }
};
}  // namespace feed::impl
