#pragma once

#include <utility>

namespace util {

/**
 * @brief A base-class that can be used to check whether the current instance was moved from
 */
class MoveTracker {
    bool wasMoved_ = false;

protected:
    /**
     * @brief The function to be used by clients in order to check whether the instance was moved
     * from
     * @return true if moved from; false otherwise
     */
    [[nodiscard]] bool
    wasMoved() const noexcept
    {
        return wasMoved_;
    }

    MoveTracker() = default;  // should only be used via inheritance

public:
    virtual ~MoveTracker() = default;

    /**
     * @brief Move constructor sets the moved-from state on `other` and resets the state on `this`
     * @param other The moved-from object
     */
    MoveTracker(MoveTracker&& other)
    {
        *this = std::move(other);
    }

    /**
     * @brief Move operator sets the moved-from state on `other` and resets the state on `this`
     * @param other The moved-from object
     * @return Reference to self
     */
    MoveTracker&
    operator=(MoveTracker&& other)
    {
        if (this != &other) {
            other.wasMoved_ = true;
            wasMoved_ = false;
        }

        return *this;
    }

    MoveTracker(MoveTracker const&) = default;
    MoveTracker&
    operator=(MoveTracker const&) = default;
};

}  // namespace util
