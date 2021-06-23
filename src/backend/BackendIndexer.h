#ifndef CLIO_BACKEND_INDEXER_H_INCLUDED
#define CLIO_BACKEND_INDEXER_H_INCLUDED
#include <ripple/basics/base_uint.h>
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <mutex>
#include <optional>
#include <thread>
namespace std {
template <>
struct hash<ripple::uint256>
{
    std::size_t
    operator()(const ripple::uint256& k) const noexcept
    {
        return boost::hash_range(k.begin(), k.end());
    }
};
}  // namespace std
namespace Backend {
// The below two structs exist to prevent developers from accidentally mixing up
// the two indexes.
struct BookIndex
{
    uint32_t bookIndex;
    explicit BookIndex(uint32_t v) : bookIndex(v){};
};
struct KeyIndex
{
    uint32_t keyIndex;
    explicit KeyIndex(uint32_t v) : keyIndex(v){};
};
class BackendInterface;
class BackendIndexer
{
    boost::asio::io_context ioc_;
    boost::asio::io_context::strand strand_;
    std::mutex mutex_;
    std::optional<boost::asio::io_context::work> work_;
    std::thread ioThread_;

    std::atomic_uint32_t indexing_ = 0;

    uint32_t keyShift_ = 20;
    std::unordered_set<ripple::uint256> keys;

    mutable bool isFirst_ = true;
    void
    doKeysRepair(
        BackendInterface const& backend,
        std::optional<uint32_t> sequence);
    void
    writeKeyFlagLedger(
        uint32_t ledgerSequence,
        BackendInterface const& backend);

public:
    BackendIndexer(boost::json::object const& config);
    ~BackendIndexer();

    void
    addKey(ripple::uint256&& key);

    void
    finish(uint32_t ledgerSequence, BackendInterface const& backend);
    void
    writeKeyFlagLedgerAsync(
        uint32_t ledgerSequence,
        BackendInterface const& backend);
    void
    doKeysRepairAsync(
        BackendInterface const& backend,
        std::optional<uint32_t> sequence);
    uint32_t
    getKeyShift()
    {
        return keyShift_;
    }
    std::optional<uint32_t>
    getCurrentlyIndexing()
    {
        uint32_t cur = indexing_.load();
        if (cur != 0)
            return cur;
        return {};
    }
    KeyIndex
    getKeyIndexOfSeq(uint32_t seq) const
    {
        if (isKeyFlagLedger(seq))
            return KeyIndex{seq};
        auto incr = (1 << keyShift_);
        KeyIndex index{(seq >> keyShift_ << keyShift_) + incr};
        assert(isKeyFlagLedger(index.keyIndex));
        return index;
    }
    bool
    isKeyFlagLedger(uint32_t ledgerSequence) const
    {
        return (ledgerSequence % (1 << keyShift_)) == 0;
    }
};
}  // namespace Backend
#endif
