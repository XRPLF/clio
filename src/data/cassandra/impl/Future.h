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

#pragma once

#include <data/cassandra/Types.h>
#include <data/cassandra/impl/ManagedObject.h>

#include <cassandra.h>

namespace data::cassandra::detail {

struct Future : public ManagedObject<CassFuture>
{
    /* implicit */ Future(CassFuture* ptr);

    MaybeError
    await() const;

    ResultOrError
    get() const;
};

void
invokeHelper(CassFuture* ptr, void* self);

class FutureWithCallback : public Future
{
public:
    using FnType = std::function<void(ResultOrError)>;
    using FnPtrType = std::unique_ptr<FnType>;

    /* implicit */ FutureWithCallback(CassFuture* ptr, FnType&& cb);
    FutureWithCallback(FutureWithCallback const&) = delete;
    FutureWithCallback(FutureWithCallback&&) = default;

private:
    /** Wrapped in a unique_ptr so it can survive std::move :/ */
    FnPtrType cb_;
};

}  // namespace data::cassandra::detail
