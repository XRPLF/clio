#pragma once

#include "util/Mutex.hpp"

#include <boost/signals2.hpp>
#include <boost/signals2/connection.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace feed::impl {

/**
 * @brief A thread-safe class to manage a signal and its tracking connections.
 *
 * @param Session The type of the object that will be tracked, when the object is destroyed, the
 * connection will be removed lazily. The pointer of the session object will also be the key to
 * disconnect.
 * @param Args The types of the arguments that will be passed to the slot.
 */
template <typename Session, typename... Args>
class TrackableSignal {
    using ConnectionPtr = Session*;
    using ConnectionSharedPtr = std::shared_ptr<Session>;

    // map of connection and signal connection, key is the pointer of the connection object
    // allow disconnect to be called in the destructor of the connection
    using ConnectionsMap = std::unordered_map<ConnectionPtr, boost::signals2::connection>;
    util::Mutex<ConnectionsMap> connections_;

    using SignalType = boost::signals2::signal<void(Args...)>;
    SignalType signal_;

public:
    /**
     * @brief Connect a slot to the signal, the slot will be called when the signal is emitted and
     * trackable is still alive.
     *
     * @param trackable Track this object's lifttime, if the object is destroyed, the connection
     * will be removed lazily. When the slot is being called, the object is guaranteed to be alive.
     * @param slot The slot connecting to the signal, the slot will be called when the signal is
     * emitted.
     * @return true if the connection is successfully added, false if the connection already exists.
     */
    bool
    connectTrackableSlot(ConnectionSharedPtr const& trackable, std::function<void(Args...)> slot)
    {
        auto connections = connections_.template lock<std::scoped_lock>();
        if (connections->contains(trackable.get())) {
            return false;
        }

        // This class can't hold the trackable's shared_ptr, because disconnect should be able to be
        // called in the the trackable's destructor. However, the trackable can not be destroyed
        // when the slot is being called either. `track_foreign` is racey when one shared_ptr is
        // tracked by multiple signals. Therefore we are storing a weak_ptr of the trackable and
        // using weak_ptr::lock() to atomically check existence and acquire a shared_ptr during slot
        // invocation. This guarantees to keep the trackable alive for the duration of the slot call
        // and avoids potential race conditions.
        connections->emplace(
            trackable.get(),
            signal_.connect([slot, weakTrackable = std::weak_ptr(trackable)](Args&&... args) {
                if (auto lifeExtender = weakTrackable.lock(); lifeExtender)
                    std::invoke(slot, std::forward<Args...>(args)...);
            })
        );
        return true;
    }

    /**
     * @brief Disconnect a slot to the signal.
     *
     * @param trackablePtr Disconnect the slot whose trackable is this pointer. Be aware that the
     * pointer is a raw pointer, allowing disconnect to be called in the destructor of the
     * trackable.
     * @return true if the connection is successfully disconnected, false if the connection does not
     * exist.
     */
    bool
    disconnect(ConnectionPtr trackablePtr)
    {
        if (auto connections = connections_.template lock<std::scoped_lock>();
            connections->contains(trackablePtr)) {
            connections->operator[](trackablePtr).disconnect();
            connections->erase(trackablePtr);
            return true;
        }
        return false;
    }

    /**
     * @brief Calling all slots.
     *
     * @param args The arguments to pass to the slots.
     */
    void
    emit(Args const&... args) const
    {
        signal_(args...);
    }

    /**
     * @brief Get the number of connections.
     */
    [[nodiscard]] std::size_t
    count() const
    {
        return connections_.template lock<std::scoped_lock>()->size();
    }
};
}  // namespace feed::impl
