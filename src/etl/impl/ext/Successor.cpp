#include "etl/impl/ext/Successor.hpp"

#include "data/BackendInterface.hpp"
#include "data/DBHelpers.hpp"
#include "data/LedgerCacheInterface.hpp"
#include "data/Types.hpp"
#include "etl/Models.hpp"
#include "util/Assert.hpp"
#include "util/log/Logger.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace etl::impl {

SuccessorExt::SuccessorExt(
    std::shared_ptr<BackendInterface> backend,
    data::LedgerCacheInterface& cache
)
    : backend_(std::move(backend)), cache_(cache)
{
}

void
SuccessorExt::onInitialData(model::LedgerData const& data) const
{
    ASSERT(cache_.get().isFull(), "Cache must be full at this point");
    ASSERT(data.edgeKeys.has_value(), "Expecting to have edge keys on initial data load");
    ASSERT(data.objects.empty(), "Should not have objects from initial data");
    writeSuccessors(data.seq);
    writeEdgeKeys(data.seq, *data.edgeKeys);  // NOLINT(bugprone-unchecked-optional-access)
}

void
SuccessorExt::onInitialObjects(
    uint32_t seq,
    [[maybe_unused]] std::vector<model::Object> const& objs,
    std::string lastKey
) const
{
    for (auto const& obj : objs) {
        if (!lastKey.empty())
            backend_->writeSuccessor(std::move(lastKey), seq, auto{obj.keyRaw});
        lastKey = obj.keyRaw;
    }
}

void
SuccessorExt::onLedgerData(model::LedgerData const& data) const
{
    namespace vs = std::views;

    LOG(log_.info()) << "Received ledger data for successor ext; obj cnt = " << data.objects.size()
                     << "; got successors = " << data.successors.has_value() << "; cache is "
                     << (cache_.get().isFull() ? "FULL" : "Not full");

    auto filteredObjects = data.objects  //
        | vs::filter([](auto const& obj) { return obj.type != model::Object::ModType::Modified; });

    if (data.successors.has_value()) {
        for (auto const& successor : *data.successors)
            writeIncludedSuccessor(data.seq, successor);

        for (auto const& obj : filteredObjects)
            writeIncludedSuccessor(data.seq, obj);
    } else {
        if (not cache_.get().isFull() or cache_.get().latestLedgerSequence() != data.seq)
            throw std::logic_error("Cache is not full, but object neighbors were not included");

        for (auto const& obj : filteredObjects)
            updateSuccessorFromCache(data.seq, obj);
    }
}

void
SuccessorExt::writeIncludedSuccessor(uint32_t seq, model::BookSuccessor const& succ) const
{
    auto firstBook = succ.firstBook;
    if (firstBook.empty())
        firstBook = uint256ToString(data::kLAST_KEY);

    backend_->writeSuccessor(auto{succ.bookBase}, seq, std::move(firstBook));
}

void
SuccessorExt::writeIncludedSuccessor(uint32_t seq, model::Object const& obj) const
{
    ASSERT(
        obj.type != model::Object::ModType::Modified,
        "Attempt to write successor for a modified object"
    );

    // TODO: perhaps make these optionals inside of obj and move value_or here
    auto pred = obj.predecessor;
    auto succ = obj.successor;

    if (obj.type == model::Object::ModType::Deleted) {
        backend_->writeSuccessor(std::move(pred), seq, std::move(succ));
    } else if (obj.type == model::Object::ModType::Created) {
        backend_->writeSuccessor(std::move(pred), seq, auto{obj.keyRaw});
        backend_->writeSuccessor(auto{obj.keyRaw}, seq, std::move(succ));
    }
}

void
SuccessorExt::updateSuccessorFromCache(uint32_t seq, model::Object const& obj) const
{
    auto const lb = cache_.get()
                        .getPredecessor(obj.key, seq)
                        .value_or(data::LedgerObject{.key = data::kFIRST_KEY, .blob = {}});
    auto const ub = cache_.get()
                        .getSuccessor(obj.key, seq)
                        .value_or(data::LedgerObject{.key = data::kLAST_KEY, .blob = {}});

    auto checkBookBase = false;
    auto const isDeleted = obj.data.empty();

    if (isDeleted) {
        backend_->writeSuccessor(uint256ToString(lb.key), seq, uint256ToString(ub.key));
    } else {
        backend_->writeSuccessor(uint256ToString(lb.key), seq, uint256ToString(obj.key));
        backend_->writeSuccessor(uint256ToString(obj.key), seq, uint256ToString(ub.key));
    }

    if (isDeleted) {
        auto const old = cache_.get().getDeleted(obj.key, seq - 1);
        ASSERT(old.has_value(), "Deleted object {} must be in cache", ripple::strHex(obj.key));

        checkBookBase = isBookDir(obj.key, *old);  // NOLINT(bugprone-unchecked-optional-access)
    } else {
        checkBookBase = isBookDir(obj.key, obj.data);
    }

    if (checkBookBase) {
        auto const current = cache_.get().get(obj.key, seq);
        auto const bookBase = getBookBase(obj.key);

        if (isDeleted and not current.has_value()) {
            updateBookSuccessor(cache_.get().getSuccessor(bookBase, seq), seq, bookBase);
        } else if (current.has_value()) {
            auto const successor = cache_.get().getSuccessor(bookBase, seq);
            ASSERT(successor.has_value(), "Book base must have a successor for seq = {}", seq);

            if (successor->key == obj.key) {
                updateBookSuccessor(successor, seq, bookBase);
            }
        }
    }
}

void
SuccessorExt::updateBookSuccessor(
    std::optional<data::LedgerObject> const& maybeSuccessor,
    auto seq,
    ripple::uint256 const& bookBase
) const
{
    if (maybeSuccessor.has_value()) {
        backend_->writeSuccessor(
            uint256ToString(bookBase), seq, uint256ToString(maybeSuccessor->key)
        );
    } else {
        backend_->writeSuccessor(uint256ToString(bookBase), seq, uint256ToString(data::kLAST_KEY));
    }
}

void
SuccessorExt::writeSuccessors(uint32_t seq) const
{
    ripple::uint256 prev = data::kFIRST_KEY;
    while (auto cur = cache_.get().getSuccessor(prev, seq)) {
        if (prev == data::kFIRST_KEY)
            backend_->writeSuccessor(uint256ToString(prev), seq, uint256ToString(cur->key));

        if (isBookDir(cur->key, cur->blob)) {
            auto base = getBookBase(cur->key);

            // make sure the base is not an actual object
            if (not cache_.get().get(base, seq)) {
                auto succ = cache_.get().getSuccessor(base, seq);
                ASSERT(
                    succ.has_value(), "Book base {} must have a successor", ripple::strHex(base)
                );

                if (succ->key == cur->key)  // NOLINT(bugprone-unchecked-optional-access)
                    backend_->writeSuccessor(uint256ToString(base), seq, uint256ToString(cur->key));
            }
        }

        prev = cur->key;
    }

    backend_->writeSuccessor(uint256ToString(prev), seq, uint256ToString(data::kLAST_KEY));
}

void
SuccessorExt::writeEdgeKeys(std::uint32_t seq, auto const& edgeKeys) const
{
    for (auto const& key : edgeKeys) {
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        auto succ = cache_.get().getSuccessor(*ripple::uint256::fromVoidChecked(key), seq);
        if (succ)
            backend_->writeSuccessor(auto{key}, seq, uint256ToString(succ->key));
    }
}

}  // namespace etl::impl
