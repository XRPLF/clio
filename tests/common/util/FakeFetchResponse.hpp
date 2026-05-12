#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Note: we don't control gRPC models so let's keep the fakes compatible
// NOLINTBEGIN(readability-identifier-naming)

class FakeBook {
    std::string base_;
    std::string first_;

public:
    std::string*
    mutable_first_book()
    {
        return &first_;
    }

    [[nodiscard]] std::string
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

class FakeBookSuccessors {
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

class FakeLedgerObject {
public:
    enum class ModType : int { MODIFIED, DELETED };

private:
    std::string key_;
    std::string data_;
    std::string predecessor_;
    std::string successor_;
    ModType mod_ = ModType::MODIFIED;

public:
    [[nodiscard]] ModType
    mod_type() const
    {
        return mod_;
    }

    [[nodiscard]] std::string
    key() const
    {
        return key_;
    }

    std::string*
    mutable_key()
    {
        return &key_;
    }

    [[nodiscard]] std::string
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

class FakeLedgerObjects {
    std::vector<FakeLedgerObject> objects_;

public:
    std::vector<FakeLedgerObject>*
    mutable_objects()
    {
        return &objects_;
    }
};

class FakeTransactionsList {
    std::size_t size_ = 0;

public:
    [[nodiscard]] std::size_t
    transactions_size() const
    {
        return size_;
    }
};

class FakeObjectsList {
    std::size_t size_ = 0;

public:
    [[nodiscard]] std::size_t
    objects_size() const
    {
        return size_;
    }
};

struct FakeFetchResponse {
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
        : id{id}, objectNeighborsIncluded{objectNeighborsIncluded}, ledgerHeader{std::move(blob)}
    {
    }

    bool
    operator==(FakeFetchResponse const& other) const
    {
        return other.id == id;
    }

    static FakeTransactionsList
    transactions_list()
    {
        return {};
    }

    static FakeObjectsList
    ledger_objects()
    {
        return {};
    }

    [[nodiscard]] bool
    object_neighbors_included() const
    {
        return objectNeighborsIncluded;
    }

    FakeLedgerObjects*
    mutable_ledger_objects()
    {
        return &ledgerObjects;
    }

    [[nodiscard]] std::string
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

// NOLINTEND(readability-identifier-naming)
