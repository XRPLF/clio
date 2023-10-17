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
#include <string>
#include <vector>

class FakeBook
{
    std::string base_;
    std::string first_;

public:
    std::string*
    mutable_first_book()
    {
        return &first_;
    }

    std::string
    book_base() const
    {
        return base_;
    }

    std::string*
    mutable_book_base()
    {
        return &base_;
    }
};

class FakeBookSuccessors
{
    std::vector<FakeBook> books_;

public:
    auto
    begin()
    {
        return books_.begin();
    }

    auto
    end()
    {
        return books_.end();
    }
};

class FakeLedgerObject
{
public:
    enum ModType : int { MODIFIED, DELETED };

private:
    std::string key_;
    std::string data_;
    std::string predecessor_;
    std::string successor_;
    ModType mod_ = MODIFIED;

public:
    ModType
    mod_type() const
    {
        return mod_;
    }

    std::string
    key() const
    {
        return key_;
    }

    std::string*
    mutable_key()
    {
        return &key_;
    }

    std::string
    data() const
    {
        return data_;
    }

    std::string*
    mutable_data()
    {
        return &data_;
    }

    std::string*
    mutable_predecessor()
    {
        return &predecessor_;
    }

    std::string*
    mutable_successor()
    {
        return &successor_;
    }
};

class FakeLedgerObjects
{
    std::vector<FakeLedgerObject> objects;

public:
    std::vector<FakeLedgerObject>*
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
    std::string ledgerHeader;
    FakeBookSuccessors bookSuccessors;

    FakeFetchResponse(uint32_t id = 0, bool objectNeighborsIncluded = false)
        : id{id}, objectNeighborsIncluded{objectNeighborsIncluded}
    {
    }

    FakeFetchResponse(std::string blob, uint32_t id = 0, bool objectNeighborsIncluded = false)
        : id{id}, objectNeighborsIncluded{objectNeighborsIncluded}, ledgerHeader{blob}
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

    FakeLedgerObjects*
    mutable_ledger_objects()
    {
        return &ledgerObjects;
    }

    std::string
    ledger_header() const
    {
        return ledgerHeader;
    }

    std::string*
    mutable_ledger_header()
    {
        return &ledgerHeader;
    }

    FakeBookSuccessors*
    mutable_book_successors()
    {
        return &bookSuccessors;
    }
};
