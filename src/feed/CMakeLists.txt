add_library(clio_feed)
target_sources(
  clio_feed PRIVATE SubscriptionManager.cpp impl/TransactionFeed.cpp impl/LedgerFeed.cpp
                    impl/ProposedTransactionFeed.cpp impl/SingleFeedBase.cpp
)

target_link_libraries(clio_feed PRIVATE clio_util)
