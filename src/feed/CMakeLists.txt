add_library(clio_feed)
target_sources(
  clio_feed PRIVATE SubscriptionManager.cpp impl/TransactionFeed.cpp impl/LedgerFeed.cpp
                    impl/ProposedTransactionFeed.cpp impl/SingleFeedBase.cpp
)

target_include_directories(clio_feed PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(
  clio_feed
  PUBLIC Boost::boost
         Boost::coroutine
         Boost::program_options
         Boost::system
         Boost::log
         Boost::log_setup
         Boost::stacktrace_backtrace
         cassandra-cpp-driver::cassandra-cpp-driver
         fmt::fmt
         OpenSSL::Crypto
         OpenSSL::SSL
         xrpl::libxrpl
         dl
         libbacktrace::libbacktrace
         clio_util
  INTERFACE Threads::Threads
)
