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
 * @param Session The type of the object that will be tracked, when the object is destroyed, the connection will be
 * removed lazily. The pointer of the session object will also be the key to disconnect.
 * @param Args The types of the arguments that will be passed to the slot.
 */
template <typename Session, typename... Args>
class TrackableSignal {
    using ConnectionPtr = Session*;
    using ConnectionSharedPtr = std::shared_ptr<Session>;

    // map of connection and signal connection, key is the pointer of the connection object
    // allow disconnect to be called in the destructor of the connection
    std::unordered_map<ConnectionPtr, boost::signals2::connection> connections_;
    mutable std::mutex mutex_;

    using SignalType = boost::signals2::signal<void(Args...)>;
    SignalType signal_;

public:
    /**
     * @brief Connect a slot to the signal, the slot will be called when the signal is emitted and trackable is still
     * alive.
     *
     * @param trackable Track this object's lifttime, if the object is destroyed, the connection will be removed lazily.
     * When the slot is being called, the object is guaranteed to be alive.
     * @param slot The slot connecting to the signal, the slot will be called when the signal is emitted.
     * @return true if the connection is successfully added, false if the connection already exists.
     */
    bool
    connectTrackableSlot(ConnectionSharedPtr const& trackable, std::function<void(Args...)> slot)
    {
        std::scoped_lock const lk(mutex_);
        if (connections_.contains(trackable.get())) {
            return false;
        }

        // This class can't hold the trackable's shared_ptr, because disconnect should be able to be called in the
        // the trackable's destructor. However, the trackable can not be destroied when the slot is being called
        // either. track_foreign will hold a weak_ptr to the connection, which makes sure the connection is valid when
        // the slot is called.
        connections_.emplace(
            trackable.get(), signal_.connect(typename SignalType::slot_type(slot).track_foreign(trackable))
        );
        return true;
    }

    /**
     * @brief Disconnect a slot to the signal.
     *
     * @param trackablePtr Disconnect the slot whose trackable is this pointer. Be aware that the pointer is a raw
     * pointer, allowing disconnect to be called in the destructor of the trackable.
     * @return true if the connection is successfully disconnected, false if the connection does not exist.
     */
    bool
    disconnect(ConnectionPtr trackablePtr)
    {
        std::scoped_lock const lk(mutex_);
        if (connections_.contains(trackablePtr)) {
            connections_[trackablePtr].disconnect();
            connections_.erase(trackablePtr);
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
    emit(Args... args) const
    {
        signal_(args...);
    }

    /**
     * @brief Get the number of connections.
     */
    std::size_t
    count() const
    {
        std::scoped_lock const lk(mutex_);
        return connections_.size();
    }
};
}  // namespace feed::impl
