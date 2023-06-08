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

#include <cstddef>

class FakeLedgerObjects
{
    std::vector<int> objects;

public:
    std::vector<int>*
    mutable_objects()
    {
        return &objects;
    }
};

class FakeTransactionsList
{
    std::size_t size_ = 0;

public:
    std::size_t
    transactions_size()
    {
        return size_;
    }
};

class FakeObjectsList
{
    std::size_t size_ = 0;

public:
    std::size_t
    objects_size()
    {
        return size_;
    }
};

struct FakeFetchResponse
{
    uint32_t id;
    bool objectNeighborsIncluded;
    FakeLedgerObjects ledgerObjects;

    FakeFetchResponse(uint32_t id = 0, bool objectNeighborsIncluded = false)
        : id{id}, objectNeighborsIncluded{objectNeighborsIncluded}
    {
    }

    bool
    operator==(FakeFetchResponse const& other) const
    {
        return other.id == id;
    }

    FakeTransactionsList
    transactions_list() const
    {
        return {};
    }

    FakeObjectsList
    ledger_objects() const
    {
        return {};
    }

    bool
    object_neighbors_included() const
    {
        return objectNeighborsIncluded;
    }

    FakeLedgerObjects&
    mutable_ledger_objects()
    {
        return ledgerObjects;
    }
};
