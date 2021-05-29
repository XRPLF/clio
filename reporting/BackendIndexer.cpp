#include <reporting/BackendInterface.h>

namespace Backend {
BackendIndexer::BackendIndexer(boost::json::object const& config)
{
    if (config.contains("indexer_key_shift"))
        keyShift_ = config.at("indexer_key_shift").as_int64();
    work_.emplace(ioc_);
    ioThread_ = std::thread{[this]() { ioc_.run(); }};
};
BackendIndexer::~BackendIndexer()
{
    std::unique_lock lck(mutex_);
    work_.reset();
    ioThread_.join();
}

void
BackendIndexer::addKey(ripple::uint256&& key)
{
    keys.insert(std::move(key));
}

void
BackendIndexer::doKeysRepair(
    BackendInterface const& backend,
    std::optional<uint32_t> sequence)
{
    auto rng = backend.fetchLedgerRangeNoThrow();

    if (!rng)
        return;

    if (!sequence)
        sequence = rng->maxSequence;

    if (sequence < rng->minSequence)
        sequence = rng->minSequence;

    BOOST_LOG_TRIVIAL(info)
        << __func__ << " sequence = " << std::to_string(*sequence);

    std::optional<ripple::uint256> cursor;
    while (true)
    {
        try
        {
            auto [objects, curCursor, warning] =
                backend.fetchLedgerPage({}, *sequence, 1);
            // no cursor means this is the first page
            // if there is no warning, we don't need to do a repair
            // warning only shows up on the first page
            if (!warning)
            {
                BOOST_LOG_TRIVIAL(debug)
                    << __func__ << " flag ledger already written. returning";
                return;
            }
            else
            {
                uint32_t lower = (*sequence - 1) >> keyShift_ << keyShift_;
                doKeysRepair(backend, lower);
                writeKeyFlagLedgerAsync(lower, backend);
                return;
            }
        }
        catch (DatabaseTimeout const& e)
        {
            BOOST_LOG_TRIVIAL(warning)
                << __func__ << " Database timeout fetching keys";
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " finished. sequence = " << std::to_string(*sequence);
}
void
BackendIndexer::doKeysRepairAsync(
    BackendInterface const& backend,
    std::optional<uint32_t> sequence)
{
    boost::asio::post(ioc_, [this, sequence, &backend]() {
        doKeysRepair(backend, sequence);
    });
}

void
BackendIndexer::writeKeyFlagLedgerAsync(
    uint32_t ledgerSequence,
    BackendInterface const& backend)
{
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " starting. sequence = " << std::to_string(ledgerSequence);

    boost::asio::post(ioc_, [this, ledgerSequence, &backend]() {
        std::unordered_set<ripple::uint256> keys;
        auto nextFlag = getKeyIndexOfSeq(ledgerSequence + 1);
        BOOST_LOG_TRIVIAL(info)
            << "writeKeyFlagLedger - " << std::to_string(nextFlag.keyIndex)
            << " starting";
        ripple::uint256 zero = {};
        std::optional<ripple::uint256> cursor;
        size_t numKeys = 0;
        auto begin = std::chrono::system_clock::now();
        while (true)
        {
            try
            {
                auto start = std::chrono::system_clock::now();
                auto [objects, curCursor, warning] =
                    backend.fetchLedgerPage(cursor, ledgerSequence, 2048);
                auto mid = std::chrono::system_clock::now();
                // no cursor means this is the first page
                if (!cursor)
                {
                    // if there is no warning, we don't need to do a repair
                    // warning only shows up on the first page
                    if (warning)
                    {
                        BOOST_LOG_TRIVIAL(error)
                            << "writeKeyFlagLedger - "
                            << " prev flag ledger not written "
                            << std::to_string(nextFlag.keyIndex) << " : "
                            << std::to_string(ledgerSequence);
                        assert(false);
                        throw std::runtime_error("Missing prev flag");
                    }
                }

                cursor = curCursor;
                for (auto& obj : objects)
                {
                    keys.insert(obj.key);
                }
                backend.writeKeys(keys, nextFlag, true);
                auto end = std::chrono::system_clock::now();
                BOOST_LOG_TRIVIAL(debug)
                    << "writeKeyFlagLedger - "
                    << std::to_string(nextFlag.keyIndex) << " fetched a page "
                    << " cursor = "
                    << (cursor.has_value() ? ripple::strHex(*cursor)
                                           : std::string{})
                    << " num keys = " << std::to_string(numKeys)
                    << " fetch time = "
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           mid - start)
                           .count()
                    << " write time = "
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           end - mid)
                           .count();
                if (!cursor)
                    break;
            }
            catch (DatabaseTimeout const& e)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << __func__ << " Database timeout fetching keys";
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
        backend.writeKeys({zero}, nextFlag, true);
        auto end = std::chrono::system_clock::now();
        BOOST_LOG_TRIVIAL(info)
            << "writeKeyFlagLedger - " << std::to_string(nextFlag.keyIndex)
            << " finished. "
            << " num keys = " << std::to_string(numKeys) << " total time = "
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   end - begin)
                   .count();
    });
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " finished. sequence = " << std::to_string(ledgerSequence);
}

void
BackendIndexer::finish(uint32_t ledgerSequence, BackendInterface const& backend)
{
    BOOST_LOG_TRIVIAL(debug)
        << __func__
        << " starting. sequence = " << std::to_string(ledgerSequence);
    bool isFirst = false;
    auto keyIndex = getKeyIndexOfSeq(ledgerSequence);
    if (isFirst_)
    {
        auto rng = backend.fetchLedgerRangeNoThrow();
        if (rng && rng->minSequence != ledgerSequence)
            isFirst_ = false;
        else
        {
            keyIndex = KeyIndex{ledgerSequence};
        }
    }

    backend.writeKeys(keys, keyIndex);
    if (isFirst_)
    {
        // write completion record
        ripple::uint256 zero = {};
        backend.writeKeys({zero}, keyIndex);
    }
    isFirst_ = false;
    keys = {};
    BOOST_LOG_TRIVIAL(debug)
        << __func__
        << " finished. sequence = " << std::to_string(ledgerSequence);
}
}  // namespace Backend
