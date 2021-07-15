#include <backend/BackendIndexer.h>
#include <backend/BackendInterface.h>

namespace Backend {
BackendIndexer::BackendIndexer(boost::json::object const& config)
    : strand_(ioc_)
{
    if (config.contains("indexer_key_shift"))
        keyShift_ = config.at("indexer_key_shift").as_int64();
    work_.emplace(ioc_);
    ioThread_ = std::thread{[this]() { ioc_.run(); }};
};
BackendIndexer::~BackendIndexer()
{
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
    auto rng = backend.fetchLedgerRange();

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
            if (backend.isLedgerIndexed(*sequence))
            {
                BOOST_LOG_TRIVIAL(info)
                    << __func__ << " - " << std::to_string(*sequence)
                    << " flag ledger already written. returning";
                return;
            }
            else
            {
                BOOST_LOG_TRIVIAL(info)
                    << __func__ << " - " << std::to_string(*sequence)
                    << " flag ledger not written. recursing..";
                uint32_t lower = (*sequence - 1) >> keyShift_ << keyShift_;
                doKeysRepair(backend, lower);
                BOOST_LOG_TRIVIAL(info)
                    << __func__ << " - "
                    << " sequence = " << std::to_string(*sequence)
                    << " lower = " << std::to_string(lower)
                    << " finished recursing. submitting repair ";
                writeKeyFlagLedger(lower, backend);
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
    boost::asio::post(strand_, [this, sequence, &backend]() {
        doKeysRepair(backend, sequence);
    });
}
void
BackendIndexer::writeKeyFlagLedger(
    uint32_t ledgerSequence,
    BackendInterface const& backend)
{
    auto nextFlag = getKeyIndexOfSeq(ledgerSequence + 1);
    uint32_t lower = ledgerSequence >> keyShift_ << keyShift_;
    BOOST_LOG_TRIVIAL(info)
        << "writeKeyFlagLedger - "
        << "next flag  = " << std::to_string(nextFlag.keyIndex)
        << "lower = " << std::to_string(lower)
        << "ledgerSequence = " << std::to_string(ledgerSequence) << " starting";
    ripple::uint256 zero = {};
    std::optional<ripple::uint256> cursor;
    size_t numKeys = 0;
    auto begin = std::chrono::system_clock::now();
    while (true)
    {
        try
        {
            {
                BOOST_LOG_TRIVIAL(info)
                    << "writeKeyFlagLedger - checking for complete...";
                if (backend.isLedgerIndexed(nextFlag.keyIndex))
                {
                    BOOST_LOG_TRIVIAL(warning)
                        << "writeKeyFlagLedger - "
                        << "flag ledger already written. flag = "
                        << std::to_string(nextFlag.keyIndex)
                        << " , ledger sequence = "
                        << std::to_string(ledgerSequence);
                    return;
                }
                BOOST_LOG_TRIVIAL(info)
                    << "writeKeyFlagLedger - is not complete";
            }
            indexing_ = nextFlag.keyIndex;
            auto start = std::chrono::system_clock::now();
            auto [objects, curCursor, warning] =
                backend.fetchLedgerPage(cursor, lower, 2048);
            auto mid = std::chrono::system_clock::now();
            // no cursor means this is the first page
            if (!cursor)
            {
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
            std::unordered_set<ripple::uint256> keys;
            for (auto& obj : objects)
            {
                keys.insert(obj.key);
            }
            backend.writeKeys(keys, nextFlag, true);
            auto end = std::chrono::system_clock::now();
            BOOST_LOG_TRIVIAL(debug)
                << "writeKeyFlagLedger - " << std::to_string(nextFlag.keyIndex)
                << " fetched a page "
                << " cursor = "
                << (cursor.has_value() ? ripple::strHex(*cursor)
                                       : std::string{})
                << " num keys = " << std::to_string(numKeys) << " fetch time = "
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
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin)
               .count();
    indexing_ = 0;
}
void
BackendIndexer::writeKeyFlagLedgerAsync(
    uint32_t ledgerSequence,
    BackendInterface const& backend)
{
    BOOST_LOG_TRIVIAL(info)
        << __func__
        << " starting. sequence = " << std::to_string(ledgerSequence);

    boost::asio::post(strand_, [this, ledgerSequence, &backend]() {
        writeKeyFlagLedger(ledgerSequence, backend);
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
    auto keyIndex = getKeyIndexOfSeq(ledgerSequence);
    if (isFirst_)
    {
        auto rng = backend.fetchLedgerRange();
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
        // write next flag sychronously
        keyIndex = getKeyIndexOfSeq(ledgerSequence + 1);
        backend.writeKeys(keys, keyIndex);
        backend.writeKeys({zero}, keyIndex);
    }
    isFirst_ = false;
    keys = {};
    BOOST_LOG_TRIVIAL(debug)
        << __func__
        << " finished. sequence = " << std::to_string(ledgerSequence);
}
}  // namespace Backend
