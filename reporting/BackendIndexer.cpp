#include <reporting/BackendInterface.h>

namespace Backend {
BackendIndexer::BackendIndexer(boost::json::object const& config)
    : keyShift_(config.at("keyshift").as_int64())
    , bookShift_(config.at("bookshift").as_int64())
{
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
BackendIndexer::addKey(ripple::uint256 const& key)
{
    keys.insert(key);
}
void
BackendIndexer::deleteKey(ripple::uint256 const& key)
{
    keys.erase(key);
}

void
BackendIndexer::addBookOffer(
    ripple::uint256 const& book,
    ripple::uint256 const& offerKey)
{
    booksToOffers[book].insert(offerKey);
}
void
BackendIndexer::deleteBookOffer(
    ripple::uint256 const& book,
    ripple::uint256 const& offerKey)
{
    booksToOffers[book].erase(offerKey);
    booksToDeletedOffers[book].insert(offerKey);
}

void
BackendIndexer::finish(uint32_t ledgerSequence, BackendInterface const& backend)
{
    if (ledgerSequence >> keyShift_ << keyShift_ == ledgerSequence)
    {
        std::unordered_set<ripple::uint256> keysCopy = keys;
        boost::asio::post(ioc_, [=, &backend]() {
            BOOST_LOG_TRIVIAL(info) << "Indexer - writing keys. Ledger = "
                                    << std::to_string(ledgerSequence);
            backend.writeKeys(keysCopy, ledgerSequence);
            BOOST_LOG_TRIVIAL(info) << "Indexer - wrote keys. Ledger = "
                                    << std::to_string(ledgerSequence);
        });
    }
    if (ledgerSequence >> bookShift_ << bookShift_ == ledgerSequence)
    {
        std::unordered_map<ripple::uint256, std::unordered_set<ripple::uint256>>
            booksToOffersCopy = booksToOffers;
        std::unordered_map<ripple::uint256, std::unordered_set<ripple::uint256>>
            booksToDeletedOffersCopy = booksToDeletedOffers;
        boost::asio::post(ioc_, [=, &backend]() {
            BOOST_LOG_TRIVIAL(info) << "Indexer - writing books. Ledger = "
                                    << std::to_string(ledgerSequence);
            backend.writeBooks(booksToOffersCopy, ledgerSequence);
            backend.writeBooks(booksToDeletedOffersCopy, ledgerSequence);
            BOOST_LOG_TRIVIAL(info) << "Indexer - wrote books. Ledger = "
                                    << std::to_string(ledgerSequence);
        });
        booksToDeletedOffers = {};
    }
}
}  // namespace Backend
