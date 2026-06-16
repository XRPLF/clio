#include "util/Coroutine.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>

#include <memory>
#include <utility>

namespace util {

Coroutine::Coroutine(
    boost::asio::yield_context&& yield,
    std::shared_ptr<FamilyCancellationSignal> signal
)
    : yield_(std::move(yield))
    , cyield_(boost::asio::bind_cancellation_slot(cancellationSignal_.slot(), yield_[error_]))
    , familySignal_{std::move(signal)}
    , connection_{familySignal_->connect([this](boost::asio::cancellation_type_t cancellationType) {
        cancellationSignal_.emit(cancellationType);
        isCancelled_ = true;
    })}

{
}

Coroutine::~Coroutine()
{
    connection_.disconnect();
}

boost::system::error_code
Coroutine::error() const
{
    return error_;
}

void
Coroutine::cancelAll(boost::asio::cancellation_type_t cancellationType)
{
    if (isCancelled())
        return;
    familySignal_->operator()(cancellationType);
}

bool
Coroutine::isCancelled() const
{
    return error_ == boost::asio::error::operation_aborted || isCancelled_;
}

Coroutine::cancellable_yield_context_type
Coroutine::yieldContext() const
{
    return cyield_;
}

boost::asio::any_io_executor
Coroutine::executor() const
{
    return cyield_.get().get_executor();
}

void
Coroutine::yield() const
{
    boost::asio::post(yield_);
}

}  // namespace util
