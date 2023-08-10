//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <data/cassandra/Error.h>
#include <data/cassandra/impl/Future.h>
#include <data/cassandra/impl/Result.h>

#include <exception>
#include <vector>

namespace {
static constexpr auto futureDeleter = [](CassFuture* ptr) { cass_future_free(ptr); };
}  // namespace

namespace data::cassandra::detail {

/* implicit */ Future::Future(CassFuture* ptr) : ManagedObject{ptr, futureDeleter}
{
}

MaybeError
Future::await() const
{
    if (auto const rc = cass_future_error_code(*this); rc)
    {
        auto errMsg = [this](std::string const& label) {
            char const* message;
            std::size_t len;
            cass_future_error_message(*this, &message, &len);
            return label + ": " + std::string{message, len};
        }(cass_error_desc(rc));
        return Error{CassandraError{errMsg, rc}};
    }
    return {};
}

ResultOrError
Future::get() const
{
    if (auto const rc = cass_future_error_code(*this); rc)
    {
        auto const errMsg = [this](std::string const& label) {
            char const* message;
            std::size_t len;
            cass_future_error_message(*this, &message, &len);
            return label + ": " + std::string{message, len};
        }("future::get()");
        return Error{CassandraError{errMsg, rc}};
    }
    else
    {
        return Result{cass_future_get_result(*this)};
    }
}

void
invokeHelper(CassFuture* ptr, void* cbPtr)
{
    // Note: can't use Future{ptr}.get() because double free will occur :/
    auto* cb = static_cast<FutureWithCallback::FnType*>(cbPtr);
    if (auto const rc = cass_future_error_code(ptr); rc)
    {
        auto const errMsg = [&ptr](std::string const& label) {
            char const* message;
            std::size_t len;
            cass_future_error_message(ptr, &message, &len);
            return label + ": " + std::string{message, len};
        }("invokeHelper");
        (*cb)(Error{CassandraError{errMsg, rc}});
    }
    else
    {
        (*cb)(Result{cass_future_get_result(ptr)});
    }
}

// TODO: cb_ can be deleted before cassandra-driver calls it if the user fails to hold onto the future object
/* implicit */ FutureWithCallback::FutureWithCallback(CassFuture* ptr, FnType&& cb)
    : Future{ptr}, cb_{std::make_unique<FnType>(std::move(cb))}
{
    // Instead of passing `this` as the userdata void*, we pass the address of
    // the callback itself which will survive std::move of the
    // FutureWithCallback parent. Not ideal but I have no better solution atm.
    cass_future_set_callback(*this, &invokeHelper, cb_.get());
}

}  // namespace data::cassandra::detail
