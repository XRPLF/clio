#include "etl/NetworkValidatedLedgers.hpp"

#include <boost/signals2/connection.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

namespace etl {
std::shared_ptr<NetworkValidatedLedgers>
NetworkValidatedLedgers::makeValidatedLedgers()
{
    return std::make_shared<NetworkValidatedLedgers>();
}

void
NetworkValidatedLedgers::push(uint32_t idx)
{
    std::scoped_lock const lck(mtx_);
    if (!latest_ || idx > *latest_)
        latest_ = idx;

    notificationChannel_(idx);
    cv_.notify_all();
}

std::optional<uint32_t>
NetworkValidatedLedgers::getMostRecent()
{
    std::unique_lock lck(mtx_);
    cv_.wait(lck, [this]() { return latest_; });
    return latest_;
}

bool
NetworkValidatedLedgers::waitUntilValidatedByNetwork(
    uint32_t sequence,
    std::optional<uint32_t> maxWaitMs
)
{
    std::unique_lock lck(mtx_);
    auto pred = [sequence, this]() -> bool { return (latest_ && sequence <= *latest_); };
    if (maxWaitMs) {
        cv_.wait_for(lck, std::chrono::milliseconds(*maxWaitMs));
    } else {
        cv_.wait(lck, pred);
    }
    return pred();
}

boost::signals2::scoped_connection
NetworkValidatedLedgers::subscribe(SignalType::slot_type const& subscriber)
{
    return notificationChannel_.connect(subscriber);
}

}  // namespace etl
