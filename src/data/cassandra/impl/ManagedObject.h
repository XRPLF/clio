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

#include <memory>

namespace data::cassandra::detail {

template <typename Managed>
class ManagedObject
{
protected:
    std::unique_ptr<Managed, void (*)(Managed*)> ptr_;

public:
    template <typename deleterCallable>
    ManagedObject(Managed* rawPtr, deleterCallable deleter) : ptr_{rawPtr, deleter}
    {
        if (rawPtr == nullptr)
            throw std::runtime_error("Could not create DB object - got nullptr");
    }
    ManagedObject(ManagedObject&&) = default;

    operator Managed*() const
    {
        return ptr_.get();
    }
};

}  // namespace data::cassandra::detail
