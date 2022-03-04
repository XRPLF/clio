#include <ripple/basics/StringUtilities.h>
#include <backend/DBHelpers.h>
#include <etl/ReportingETL.h>

#include <ripple/beast/core/CurrentThreadName.h>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <subscriptions/SubscriptionManager.h>
#include <thread>
#include <variant>

namespace detail {
/// Convenience function for printing out basic ledger info
std::string
toString(ripple::LedgerInfo const& info)
{
    std::stringstream ss;
    ss << "LedgerInfo { Sequence : " << info.seq
       << " Hash : " << strHex(info.hash) << " TxHash : " << strHex(info.txHash)
       << " AccountHash : " << strHex(info.accountHash)
       << " ParentHash : " << strHex(info.parentHash) << " }";
    return ss.str();
}
}  // namespace detail

std::vector<AccountTransactionsData>
ReportingETL::insertTransactions(
    ripple::LedgerInfo const& ledger,
    org::xrpl::rpc::v1::GetLedgerResponse& data)
{
    std::vector<AccountTransactionsData> accountTxData;
    for (auto& txn :
         *(data.mutable_transactions_list()->mutable_transactions()))
    {
        std::string* raw = txn.mutable_transaction_blob();

        ripple::SerialIter it{raw->data(), raw->size()};
        ripple::STTx sttx{it};

        auto txSerializer =
            std::make_shared<ripple::Serializer>(sttx.getSerializer());

        ripple::TxMeta txMeta{
            sttx.getTransactionID(), ledger.seq, txn.metadata_blob()};

        auto metaSerializer = std::make_shared<ripple::Serializer>(
            txMeta.getAsObject().getSerializer());

        BOOST_LOG_TRIVIAL(trace)
            << __func__ << " : "
            << "Inserting transaction = " << sttx.getTransactionID();

        auto journal = ripple::debugLog();
        accountTxData.emplace_back(txMeta, sttx.getTransactionID(), journal);
        std::string keyStr{(const char*)sttx.getTransactionID().data(), 32};
        backend_->writeTransaction(
            std::move(keyStr),
            ledger.seq,
            ledger.closeTime.time_since_epoch().count(),
            std::move(*raw),
            std::move(*txn.mutable_metadata_blob()));
    }
    return accountTxData;
}

std::optional<ripple::LedgerInfo>
ReportingETL::loadInitialLedger(uint32_t startingSequence)
{
    // check that database is actually empty
    auto rng = backend_->hardFetchLedgerRangeNoThrow();
    if (rng)
    {
        BOOST_LOG_TRIVIAL(fatal) << __func__ << " : "
                                 << "Database is not empty";
        assert(false);
        return {};
    }

    // fetch the ledger from the network. This function will not return until
    // either the fetch is successful, or the server is being shutdown. This
    // only fetches the ledger header and the transactions+metadata
    std::optional<org::xrpl::rpc::v1::GetLedgerResponse> ledgerData{
        fetchLedgerData(startingSequence)};
    if (!ledgerData)
        return {};

    ripple::LedgerInfo lgrInfo =
        deserializeHeader(ripple::makeSlice(ledgerData->ledger_header()));

    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " : "
        << "Deserialized ledger header. " << detail::toString(lgrInfo);

    auto start = std::chrono::system_clock::now();

    backend_->startWrites();

    BOOST_LOG_TRIVIAL(debug) << __func__ << " started writes";

    backend_->writeLedger(
        lgrInfo, std::move(*ledgerData->mutable_ledger_header()));

    BOOST_LOG_TRIVIAL(debug) << __func__ << " wrote ledger";
    std::vector<AccountTransactionsData> accountTxData =
        insertTransactions(lgrInfo, *ledgerData);
    BOOST_LOG_TRIVIAL(debug) << __func__ << " inserted txns";

    // download the full account state map. This function downloads full ledger
    // data and pushes the downloaded data into the writeQueue. asyncWriter
    // consumes from the queue and inserts the data into the Ledger object.
    // Once the below call returns, all data has been pushed into the queue
    loadBalancer_->loadInitialLedger(startingSequence);

    BOOST_LOG_TRIVIAL(debug) << __func__ << " loaded initial ledger";

    if (!stopping_)
        backend_->writeAccountTransactions(std::move(accountTxData));

    backend_->finishWrites(startingSequence);

    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(debug) << "Time to download and store ledger = "
                             << ((end - start).count()) / 1000000000.0;
    return lgrInfo;
}

void
ReportingETL::publishLedger(ripple::LedgerInfo const& lgrInfo)
{
    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " - Publishing ledger " << std::to_string(lgrInfo.seq);

    if (!writing_)
    {
        BOOST_LOG_TRIVIAL(debug) << __func__ << " - Updating cache";

        std::vector<Backend::LedgerObject> diff =
            Backend::synchronousAndRetryOnTimeout([&](auto yield) {
                return backend_->fetchLedgerDiff(lgrInfo.seq, yield);
            });

        backend_->cache().update(diff, lgrInfo.seq);
        backend_->updateRange(lgrInfo.seq);
    }

    std::optional<ripple::Fees> fees = Backend::synchronousAndRetryOnTimeout(
        [&](auto yield) { return backend_->fetchFees(lgrInfo.seq, yield); });

    std::vector<Backend::TransactionAndMetadata> transactions =
        Backend::synchronousAndRetryOnTimeout([&](auto yield) {
            return backend_->fetchAllTransactionsInLedger(lgrInfo.seq, yield);
        });

    auto ledgerRange = backend_->fetchLedgerRange();
    assert(ledgerRange);
    assert(fees);

    std::string range = std::to_string(ledgerRange->minSequence) + "-" +
        std::to_string(ledgerRange->maxSequence);

    subscriptions_->pubLedger(lgrInfo, *fees, range, transactions.size());

    for (auto& txAndMeta : transactions)
        subscriptions_->pubTransaction(txAndMeta, lgrInfo);

    setLastPublish();
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " - Published ledger " << std::to_string(lgrInfo.seq);
}

bool
ReportingETL::publishLedger(
    uint32_t ledgerSequence,
    std::optional<uint32_t> maxAttempts)
{
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " : "
        << "Attempting to publish ledger = " << ledgerSequence;
    size_t numAttempts = 0;
    while (!stopping_)
    {
        auto range = backend_->hardFetchLedgerRangeNoThrow();

        if (!range || range->maxSequence < ledgerSequence)
        {
            BOOST_LOG_TRIVIAL(debug) << __func__ << " : "
                                     << "Trying to publish. Could not find "
                                        "ledger with sequence = "
                                     << ledgerSequence;
            // We try maxAttempts times to publish the ledger, waiting one
            // second in between each attempt.
            if (maxAttempts && numAttempts >= maxAttempts)
            {
                BOOST_LOG_TRIVIAL(debug) << __func__ << " : "
                                         << "Failed to publish ledger after "
                                         << numAttempts << " attempts.";
                return false;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ++numAttempts;
            continue;
        }
        else
        {
            auto lgr = Backend::synchronousAndRetryOnTimeout([&](auto yield) {
                return backend_->fetchLedgerBySequence(ledgerSequence, yield);
            });

            assert(lgr);
            publishLedger(*lgr);

            return true;
        }
    }
    return false;
}

std::optional<org::xrpl::rpc::v1::GetLedgerResponse>
ReportingETL::fetchLedgerData(uint32_t idx)
{
    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " : "
        << "Attempting to fetch ledger with sequence = " << idx;

    std::optional<org::xrpl::rpc::v1::GetLedgerResponse> response =
        loadBalancer_->fetchLedger(idx, false, false);
    if (response)
        BOOST_LOG_TRIVIAL(trace)
            << __func__ << " : "
            << "GetLedger reply = " << response->DebugString();
    return response;
}

std::optional<org::xrpl::rpc::v1::GetLedgerResponse>
ReportingETL::fetchLedgerDataAndDiff(uint32_t idx)
{
    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " : "
        << "Attempting to fetch ledger with sequence = " << idx;

    std::optional<org::xrpl::rpc::v1::GetLedgerResponse> response =
        loadBalancer_->fetchLedger(idx, true, !backend_->cache().isFull());
    if (response)
        BOOST_LOG_TRIVIAL(trace)
            << __func__ << " : "
            << "GetLedger reply = " << response->DebugString();
    return response;
}

std::pair<ripple::LedgerInfo, bool>
ReportingETL::buildNextLedger(org::xrpl::rpc::v1::GetLedgerResponse& rawData)
{
    BOOST_LOG_TRIVIAL(debug) << __func__ << " : "
                             << "Beginning ledger update";

    ripple::LedgerInfo lgrInfo =
        deserializeHeader(ripple::makeSlice(rawData.ledger_header()));

    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " : "
        << "Deserialized ledger header. " << detail::toString(lgrInfo);

    backend_->startWrites();

    BOOST_LOG_TRIVIAL(debug) << __func__ << " : "
                             << "started writes";

    backend_->writeLedger(lgrInfo, std::move(*rawData.mutable_ledger_header()));

    BOOST_LOG_TRIVIAL(debug) << __func__ << " : "
                             << "wrote ledger header";

    // Write successor info, if included from rippled
    if (rawData.object_neighbors_included())
    {
        BOOST_LOG_TRIVIAL(debug) << __func__ << " object neighbors included";
        for (auto& obj : *(rawData.mutable_book_successors()))
        {
            auto firstBook = std::move(*obj.mutable_first_book());
            if (!firstBook.size())
                firstBook = uint256ToString(Backend::lastKey);

            backend_->writeSuccessor(
                std::move(*obj.mutable_book_base()),
                lgrInfo.seq,
                std::move(firstBook));

            BOOST_LOG_TRIVIAL(debug) << __func__ << " writing book successor "
                                     << ripple::strHex(obj.book_base()) << " - "
                                     << ripple::strHex(firstBook);
        }
        for (auto& obj : *(rawData.mutable_ledger_objects()->mutable_objects()))
        {
            if (obj.mod_type() != org::xrpl::rpc::v1::RawLedgerObject::MODIFIED)
            {
                std::string* predPtr = obj.mutable_predecessor();
                if (!predPtr->size())
                    *predPtr = uint256ToString(Backend::firstKey);
                std::string* succPtr = obj.mutable_successor();
                if (!succPtr->size())
                    *succPtr = uint256ToString(Backend::lastKey);

                if (obj.mod_type() ==
                    org::xrpl::rpc::v1::RawLedgerObject::DELETED)
                {
                    BOOST_LOG_TRIVIAL(debug)
                        << __func__
                        << " modifying successors for deleted object "
                        << ripple::strHex(obj.key()) << " - "
                        << ripple::strHex(*predPtr) << " - "
                        << ripple::strHex(*succPtr);

                    backend_->writeSuccessor(
                        std::move(*predPtr), lgrInfo.seq, std::move(*succPtr));
                }
                else
                {
                    BOOST_LOG_TRIVIAL(debug)
                        << __func__ << " adding successor for new object "
                        << ripple::strHex(obj.key()) << " - "
                        << ripple::strHex(*predPtr) << " - "
                        << ripple::strHex(*succPtr);

                    backend_->writeSuccessor(
                        std::move(*predPtr),
                        lgrInfo.seq,
                        std::string{obj.key()});
                    backend_->writeSuccessor(
                        std::string{obj.key()},
                        lgrInfo.seq,
                        std::move(*succPtr));
                }
            }
            else
                BOOST_LOG_TRIVIAL(debug) << __func__ << " object modified "
                                         << ripple::strHex(obj.key());
        }
    }
    std::vector<Backend::LedgerObject> cacheUpdates;
    cacheUpdates.reserve(rawData.ledger_objects().objects_size());
    // TODO change these to unordered_set
    std::set<ripple::uint256> bookSuccessorsToCalculate;
    std::set<ripple::uint256> modified;
    for (auto& obj : *(rawData.mutable_ledger_objects()->mutable_objects()))
    {
        auto key = ripple::uint256::fromVoidChecked(obj.key());
        assert(key);
        cacheUpdates.push_back(
            {*key, {obj.mutable_data()->begin(), obj.mutable_data()->end()}});
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " key = " << ripple::strHex(*key)
            << " - mod type = " << obj.mod_type();

        if (obj.mod_type() != org::xrpl::rpc::v1::RawLedgerObject::MODIFIED &&
            !rawData.object_neighbors_included())
        {
            BOOST_LOG_TRIVIAL(debug)
                << __func__ << " object neighbors not included. using cache";
            assert(backend_->cache().isFull());
            if (!backend_->cache().isFull())
                throw std::runtime_error(
                    "Cache is not full, but object neighbors were not "
                    "included");
            auto blob = obj.mutable_data();
            bool checkBookBase = false;
            bool isDeleted = (blob->size() == 0);
            if (isDeleted)
            {
                auto old = backend_->cache().get(*key, lgrInfo.seq - 1);
                assert(old);
                checkBookBase = isBookDir(*key, *old);
            }
            else
                checkBookBase = isBookDir(*key, *blob);
            if (checkBookBase)
            {
                BOOST_LOG_TRIVIAL(debug)
                    << __func__
                    << " is book dir. key = " << ripple::strHex(*key);
                auto bookBase = getBookBase(*key);
                auto oldFirstDir =
                    backend_->cache().getSuccessor(bookBase, lgrInfo.seq - 1);
                assert(oldFirstDir);
                // We deleted the first directory, or we added a directory prior
                // to the old first directory
                if ((isDeleted && key == oldFirstDir->key) ||
                    (!isDeleted && key < oldFirstDir->key))
                {
                    BOOST_LOG_TRIVIAL(debug)
                        << __func__
                        << " Need to recalculate book base successor. base = "
                        << ripple::strHex(bookBase)
                        << " - key = " << ripple::strHex(*key)
                        << " - isDeleted = " << isDeleted
                        << " - seq = " << lgrInfo.seq;
                    bookSuccessorsToCalculate.insert(bookBase);
                }
            }
        }
        if (obj.mod_type() == org::xrpl::rpc::v1::RawLedgerObject::MODIFIED)
            modified.insert(*key);

        backend_->writeLedgerObject(
            std::move(*obj.mutable_key()),
            lgrInfo.seq,
            std::move(*obj.mutable_data()));
    }
    backend_->cache().update(cacheUpdates, lgrInfo.seq);
    // rippled didn't send successor information, so use our cache
    if (!rawData.object_neighbors_included() || backend_->cache().isFull())
    {
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " object neighbors not included. using cache";
        assert(backend_->cache().isFull());
        for (auto const& obj : cacheUpdates)
        {
            if (modified.count(obj.key))
                continue;
            auto lb = backend_->cache().getPredecessor(obj.key, lgrInfo.seq);
            if (!lb)
                lb = {Backend::firstKey, {}};
            auto ub = backend_->cache().getSuccessor(obj.key, lgrInfo.seq);
            if (!ub)
                ub = {Backend::lastKey, {}};
            if (obj.blob.size() == 0)
            {
                BOOST_LOG_TRIVIAL(debug)
                    << __func__ << " writing successor for deleted object "
                    << ripple::strHex(obj.key) << " - "
                    << ripple::strHex(lb->key) << " - "
                    << ripple::strHex(ub->key);

                backend_->writeSuccessor(
                    uint256ToString(lb->key),
                    lgrInfo.seq,
                    uint256ToString(ub->key));
            }
            else
            {
                backend_->writeSuccessor(
                    uint256ToString(lb->key),
                    lgrInfo.seq,
                    uint256ToString(obj.key));
                backend_->writeSuccessor(
                    uint256ToString(obj.key),
                    lgrInfo.seq,
                    uint256ToString(ub->key));

                BOOST_LOG_TRIVIAL(debug)
                    << __func__ << " writing successor for new object "
                    << ripple::strHex(lb->key) << " - "
                    << ripple::strHex(obj.key) << " - "
                    << ripple::strHex(ub->key);
            }
        }
        for (auto const& base : bookSuccessorsToCalculate)
        {
            auto succ = backend_->cache().getSuccessor(base, lgrInfo.seq);
            if (succ)
            {
                backend_->writeSuccessor(
                    uint256ToString(base),
                    lgrInfo.seq,
                    uint256ToString(succ->key));

                BOOST_LOG_TRIVIAL(debug)
                    << __func__ << " Updating book successor "
                    << ripple::strHex(base) << " - "
                    << ripple::strHex(succ->key);
            }
            else
            {
                backend_->writeSuccessor(
                    uint256ToString(base),
                    lgrInfo.seq,
                    uint256ToString(Backend::lastKey));

                BOOST_LOG_TRIVIAL(debug)
                    << __func__ << " Updating book successor "
                    << ripple::strHex(base) << " - "
                    << ripple::strHex(Backend::lastKey);
            }
        }
    }

    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " : "
        << "Inserted/modified/deleted all objects. Number of objects = "
        << rawData.ledger_objects().objects_size();
    std::vector<AccountTransactionsData> accountTxData{
        insertTransactions(lgrInfo, rawData)};
    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " : "
        << "Inserted all transactions. Number of transactions  = "
        << rawData.transactions_list().transactions_size();

    backend_->writeAccountTransactions(std::move(accountTxData));

    BOOST_LOG_TRIVIAL(debug) << __func__ << " : "
                             << "wrote account_tx";
    auto start = std::chrono::system_clock::now();

    bool success = backend_->finishWrites(lgrInfo.seq);

    auto end = std::chrono::system_clock::now();

    auto duration = ((end - start).count()) / 1000000000.0;
    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " finished writes. took " << std::to_string(duration);

    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " : "
        << "Finished ledger update. " << detail::toString(lgrInfo);
    return {lgrInfo, success};
}

// Database must be populated when this starts
std::optional<uint32_t>
ReportingETL::runETLPipeline(uint32_t startSequence, int numExtractors)
{
    if (finishSequence_ && startSequence > *finishSequence_)
        return {};

    /*
     * Behold, mortals! This function spawns three separate threads, which talk
     * to each other via 2 different thread safe queues and 1 atomic variable.
     * All threads and queues are function local. This function returns when all
     *
     * of the threads exit. There are two termination conditions: the first is
     * if the load thread encounters a write conflict. In this case, the load
     * thread sets writeConflict, an atomic bool, to true, which signals the
     * other threads to stop. The second termination condition is when the
     * entire server is shutting down, which is detected in one of three ways:
     * 1. isStopping() returns true if the server is shutting down
     * 2. networkValidatedLedgers_.waitUntilValidatedByNetwork returns
     * false, signaling the wait was aborted.
     * 3. fetchLedgerDataAndDiff returns an empty optional, signaling the fetch
     * was aborted.
     * In all cases, the extract thread detects this condition,
     * and pushes an empty optional onto the transform queue. The transform
     * thread, upon popping an empty optional, pushes an empty optional onto the
     * load queue, and then returns. The load thread, upon popping an empty
     * optional, returns.
     */

    BOOST_LOG_TRIVIAL(debug) << __func__ << " : "
                             << "Starting etl pipeline";
    writing_ = true;

    auto rng = backend_->hardFetchLedgerRangeNoThrow();
    if (!rng || rng->maxSequence < startSequence - 1)
    {
        assert(false);
        throw std::runtime_error("runETLPipeline: parent ledger is null");
    }
    std::atomic<uint32_t> minSequence = rng->minSequence;

    std::atomic_bool writeConflict = false;
    std::optional<uint32_t> lastPublishedSequence;
    uint32_t maxQueueSize = 1000 / numExtractors;
    auto begin = std::chrono::system_clock::now();
    using QueueType =
        ThreadSafeQueue<std::optional<org::xrpl::rpc::v1::GetLedgerResponse>>;
    std::vector<std::shared_ptr<QueueType>> queues;

    auto getNext = [&queues, &startSequence, &numExtractors](
                       uint32_t sequence) -> std::shared_ptr<QueueType> {
        return queues[(sequence - startSequence) % numExtractors];
    };
    std::vector<std::thread> extractors;
    for (size_t i = 0; i < numExtractors; ++i)
    {
        auto transformQueue = std::make_shared<QueueType>(maxQueueSize);
        queues.push_back(transformQueue);

        extractors.emplace_back([this,
                                 &startSequence,
                                 &writeConflict,
                                 transformQueue,
                                 i,
                                 numExtractors]() {
            beast::setCurrentThreadName("rippled: ReportingETL extract");
            uint32_t currentSequence = startSequence + i;

            double totalTime = 0;

            // there are two stopping conditions here.
            // First, if there is a write conflict in the load thread, the
            // ETL mechanism should stop. The other stopping condition is if
            // the entire server is shutting down. This can be detected in a
            // variety of ways. See the comment at the top of the function
            while ((!finishSequence_ || currentSequence <= *finishSequence_) &&
                   networkValidatedLedgers_->waitUntilValidatedByNetwork(
                       currentSequence) &&
                   !writeConflict && !isStopping())
            {
                auto start = std::chrono::system_clock::now();
                std::optional<org::xrpl::rpc::v1::GetLedgerResponse>
                    fetchResponse{fetchLedgerDataAndDiff(currentSequence)};
                auto end = std::chrono::system_clock::now();

                auto time = ((end - start).count()) / 1000000000.0;
                totalTime += time;

                // if the fetch is unsuccessful, stop. fetchLedger only
                // returns false if the server is shutting down, or if the
                // ledger was found in the database (which means another
                // process already wrote the ledger that this process was
                // trying to extract; this is a form of a write conflict).
                // Otherwise, fetchLedgerDataAndDiff will keep trying to
                // fetch the specified ledger until successful
                if (!fetchResponse)
                {
                    break;
                }
                auto tps =
                    fetchResponse->transactions_list().transactions_size() /
                    time;

                BOOST_LOG_TRIVIAL(info)
                    << "Extract phase time = " << time
                    << " . Extract phase tps = " << tps
                    << " . Avg extract time = "
                    << totalTime / (currentSequence - startSequence + 1)
                    << " . thread num = " << i
                    << " . seq = " << currentSequence;

                transformQueue->push(std::move(fetchResponse));
                currentSequence += numExtractors;
                if (finishSequence_ && currentSequence > *finishSequence_)
                    break;
            }
            // empty optional tells the transformer to shut down
            transformQueue->push({});
        });
    }

    std::thread transformer{[this,
                             &minSequence,
                             &writeConflict,
                             &startSequence,
                             &getNext,
                             &lastPublishedSequence]() {
        beast::setCurrentThreadName("rippled: ReportingETL transform");
        uint32_t currentSequence = startSequence;

        auto begin = std::chrono::system_clock::now();

        while (!writeConflict)
        {
            std::optional<org::xrpl::rpc::v1::GetLedgerResponse> fetchResponse{
                getNext(currentSequence)->pop()};
            ++currentSequence;
            // if fetchResponse is an empty optional, the extracter thread
            // has stopped and the transformer should stop as well
            if (!fetchResponse)
            {
                break;
            }
            if (isStopping())
                continue;

            auto numTxns =
                fetchResponse->transactions_list().transactions_size();
            auto numObjects = fetchResponse->ledger_objects().objects_size();
            auto start = std::chrono::system_clock::now();
            auto [lgrInfo, success] = buildNextLedger(*fetchResponse);
            auto end = std::chrono::system_clock::now();

            auto duration = ((end - start).count()) / 1000000000.0;
            if (success)
                BOOST_LOG_TRIVIAL(info)
                    << "Load phase of etl : "
                    << "Successfully wrote ledger! Ledger info: "
                    << detail::toString(lgrInfo) << ". txn count = " << numTxns
                    << ". object count = " << numObjects
                    << ". load time = " << duration
                    << ". load txns per second = " << numTxns / duration
                    << ". load objs per second = " << numObjects / duration;
            else
                BOOST_LOG_TRIVIAL(error)
                    << "Error writing ledger. " << detail::toString(lgrInfo);
            // success is false if the ledger was already written
            if (success)
            {
                auto now =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
                auto closeTime = lgrInfo.closeTime.time_since_epoch().count();
                auto age = now - (rippleEpochStart + closeTime);
                // if the ledger closed over 10 seconds ago, assume we are still
                // catching up and don't publish
                if (age < 10)
                {
                    boost::asio::post(
                        publishStrand_, [this, lgrInfo = lgrInfo]() {
                            publishLedger(lgrInfo);
                        });
                }

                lastPublishedSequence = lgrInfo.seq;
            }
            writeConflict = !success;
            // TODO move online delete logic to an admin RPC call
            if (onlineDeleteInterval_ && !deleting_ &&
                lgrInfo.seq - minSequence > *onlineDeleteInterval_)
            {
                deleting_ = true;
                ioContext_.post([this, &minSequence]() {
                    BOOST_LOG_TRIVIAL(info) << "Running online delete";

                    Backend::synchronous(
                        [&](boost::asio::yield_context& yield) {
                            backend_->doOnlineDelete(
                                *onlineDeleteInterval_, yield);
                        });

                    BOOST_LOG_TRIVIAL(info) << "Finished online delete";
                    auto rng = backend_->fetchLedgerRange();
                    minSequence = rng->minSequence;
                    deleting_ = false;
                });
            }
        }
    }};

    transformer.join();
    for (size_t i = 0; i < numExtractors; ++i)
    {
        // pop from each queue that might be blocked on a push
        getNext(i)->tryPop();
    }
    // wait for all of the extractors to stop
    for (auto& t : extractors)
        t.join();
    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(debug)
        << "Extracted and wrote " << *lastPublishedSequence - startSequence
        << " in " << ((end - begin).count()) / 1000000000.0;
    writing_ = false;

    BOOST_LOG_TRIVIAL(debug) << __func__ << " : "
                             << "Stopping etl pipeline";

    return lastPublishedSequence;
}

// main loop. The software begins monitoring the ledgers that are validated
// by the nework. The member networkValidatedLedgers_ keeps track of the
// sequences of ledgers validated by the network. Whenever a ledger is validated
// by the network, the software looks for that ledger in the database. Once the
// ledger is found in the database, the software publishes that ledger to the
// ledgers stream. If a network validated ledger is not found in the database
// after a certain amount of time, then the software attempts to take over
// responsibility of the ETL process, where it writes new ledgers to the
// database. The software will relinquish control of the ETL process if it
// detects that another process has taken over ETL.
void
ReportingETL::monitor()
{
    std::optional<uint32_t> latestSequence =
        Backend::synchronousAndRetryOnTimeout([&](auto yield) {
            return backend_->fetchLatestLedgerSequence(yield);
        });
    if (!latestSequence)
    {
        BOOST_LOG_TRIVIAL(info) << __func__ << " : "
                                << "Database is empty. Will download a ledger "
                                   "from the network.";
        std::optional<ripple::LedgerInfo> ledger;
        if (startSequence_)
        {
            BOOST_LOG_TRIVIAL(info)
                << __func__ << " : "
                << "ledger sequence specified in config. "
                << "Will begin ETL process starting with ledger "
                << *startSequence_;
            ledger = loadInitialLedger(*startSequence_);
        }
        else
        {
            BOOST_LOG_TRIVIAL(info)
                << __func__ << " : "
                << "Waiting for next ledger to be validated by network...";
            std::optional<uint32_t> mostRecentValidated =
                networkValidatedLedgers_->getMostRecent();
            if (mostRecentValidated)
            {
                BOOST_LOG_TRIVIAL(info) << __func__ << " : "
                                        << "Ledger " << *mostRecentValidated
                                        << " has been validated. "
                                        << "Downloading...";
                ledger = loadInitialLedger(*mostRecentValidated);
            }
            else
            {
                BOOST_LOG_TRIVIAL(info) << __func__ << " : "
                                        << "The wait for the next validated "
                                        << "ledger has been aborted. "
                                        << "Exiting monitor loop";
                return;
            }
        }
        if (ledger)
            latestSequence = ledger->seq;
    }
    else
    {
        if (startSequence_)
        {
            BOOST_LOG_TRIVIAL(warning)
                << "start sequence specified but db is already populated";
        }
        BOOST_LOG_TRIVIAL(info)
            << __func__ << " : "
            << "Database already populated. Picking up from the tip of history";
    }
    if (!latestSequence)
    {
        BOOST_LOG_TRIVIAL(error)
            << __func__ << " : "
            << "Failed to load initial ledger. Exiting monitor loop";
        return;
    }
    else
    {
    }
    uint32_t nextSequence = latestSequence.value() + 1;
    if (!backend_->cache().isFull())
    {
        std::thread t{[this, latestSequence]() {
            BOOST_LOG_TRIVIAL(info) << "Loading cache";
            loadBalancer_->loadInitialLedger(*latestSequence, true);
            backend_->cache().setFull();
        }};
        t.detach();
    }

    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " : "
        << "Database is populated. "
        << "Starting monitor loop. sequence = " << nextSequence;
    while (!stopping_ &&
           networkValidatedLedgers_->waitUntilValidatedByNetwork(nextSequence))
    {
        BOOST_LOG_TRIVIAL(info) << __func__ << " : "
                                << "Ledger with sequence = " << nextSequence
                                << " has been validated by the network. "
                                << "Attempting to find in database and publish";
        // Attempt to take over responsibility of ETL writer after 2 failed
        // attempts to publish the ledger. publishLedger() fails if the
        // ledger that has been validated by the network is not found in the
        // database after the specified number of attempts. publishLedger()
        // waits one second between each attempt to read the ledger from the
        // database
        //
        // In strict read-only mode, when the software fails to find a
        // ledger in the database that has been validated by the network,
        // the software will only try to publish subsequent ledgers once,
        // until one of those ledgers is found in the database. Once the
        // software successfully publishes a ledger, the software will fall
        // back to the normal behavior of trying several times to publish
        // the ledger that has been validated by the network. In this
        // manner, a reporting processing running in read-only mode does not
        // need to restart if the database is wiped.
        constexpr size_t timeoutSeconds = 2;
        bool success = publishLedger(nextSequence, timeoutSeconds);
        if (!success)
        {
            BOOST_LOG_TRIVIAL(warning)
                << __func__ << " : "
                << "Failed to publish ledger with sequence = " << nextSequence
                << " . Beginning ETL";
            // doContinousETLPipelined returns the most recent sequence
            // published empty optional if no sequence was published
            std::optional<uint32_t> lastPublished =
                runETLPipeline(nextSequence, extractorThreads_);
            BOOST_LOG_TRIVIAL(info)
                << __func__ << " : "
                << "Aborting ETL. Falling back to publishing";
            // if no ledger was published, don't increment nextSequence
            if (lastPublished)
                nextSequence = *lastPublished + 1;
        }
        else
            ++nextSequence;
    }
}

void
ReportingETL::monitorReadOnly()
{
    BOOST_LOG_TRIVIAL(debug) << "Starting reporting in strict read only mode";
    std::optional<uint32_t> latestSequence =
        Backend::synchronousAndRetryOnTimeout([&](auto yield) {
            return backend_->fetchLatestLedgerSequence(yield);
        });
    if (!latestSequence)
        latestSequence = networkValidatedLedgers_->getMostRecent();
    if (!latestSequence)
        return;
    std::thread t{[this, latestSequence]() {
        BOOST_LOG_TRIVIAL(info) << "Loading cache";
        loadBalancer_->loadInitialLedger(*latestSequence, true);
    }};
    t.detach();
    latestSequence = *latestSequence + 1;
    while (true)
    {
        // try to grab the next ledger
        if (Backend::synchronousAndRetryOnTimeout([&](auto yield) {
                return backend_->fetchLedgerBySequence(*latestSequence, yield);
            }))
        {
            publishLedger(*latestSequence, {});
            latestSequence = *latestSequence + 1;
        }
        else  // if we can't, wait until it's validated by the network, or 1
              // second passes, whichever occurs first. Even if we don't hear
              // from rippled, if ledgers are being written to the db, we
              // publish them
            networkValidatedLedgers_->waitUntilValidatedByNetwork(
                *latestSequence, 1000);
    }
}

void
ReportingETL::doWork()
{
    worker_ = std::thread([this]() {
        beast::setCurrentThreadName("rippled: ReportingETL worker");
        if (readOnly_)
            monitorReadOnly();
        else
            monitor();
    });
}

ReportingETL::ReportingETL(
    boost::json::object const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<ETLLoadBalancer> balancer,
    std::shared_ptr<NetworkValidatedLedgers> ledgers)
    : backend_(backend)
    , subscriptions_(subscriptions)
    , loadBalancer_(balancer)
    , ioContext_(ioc)
    , publishStrand_(ioc)
    , networkValidatedLedgers_(ledgers)
{
    if (config.contains("start_sequence"))
        startSequence_ = config.at("start_sequence").as_int64();
    if (config.contains("finish_sequence"))
        finishSequence_ = config.at("finish_sequence").as_int64();
    if (config.contains("read_only"))
        readOnly_ = config.at("read_only").as_bool();
    if (config.contains("online_delete"))
    {
        int64_t interval = config.at("online_delete").as_int64();
        uint32_t max = std::numeric_limits<uint32_t>::max();
        if (interval > max)
        {
            std::stringstream msg;
            msg << "online_delete cannot be greater than "
                << std::to_string(max);
            throw std::runtime_error(msg.str());
        }
        if (interval > 0)
            onlineDeleteInterval_ = static_cast<uint32_t>(interval);
    }
    if (config.contains("extractor_threads"))
        extractorThreads_ = config.at("extractor_threads").as_int64();
    if (config.contains("txn_threshold"))
        txnThreshold_ = config.at("txn_threshold").as_int64();
}
