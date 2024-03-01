add_library(clio_rpc)

target_sources(
  clio_rpc
  PRIVATE Errors.cpp
          Factories.cpp
          AMMHelpers.cpp
          RPCHelpers.cpp
          Counters.cpp
          WorkQueue.cpp
          common/Specs.cpp
          common/Validators.cpp
          common/MetaProcessors.cpp
          common/impl/APIVersionParser.cpp
          common/impl/HandlerProvider.cpp
          handlers/AccountChannels.cpp
          handlers/AccountCurrencies.cpp
          handlers/AccountInfo.cpp
          handlers/AccountLines.cpp
          handlers/AccountNFTs.cpp
          handlers/AccountObjects.cpp
          handlers/AccountOffers.cpp
          handlers/AccountTx.cpp
          handlers/AMMInfo.cpp
          handlers/BookChanges.cpp
          handlers/BookOffers.cpp
          handlers/DepositAuthorized.cpp
          handlers/GatewayBalances.cpp
          handlers/Ledger.cpp
          handlers/LedgerData.cpp
          handlers/LedgerEntry.cpp
          handlers/LedgerRange.cpp
          handlers/NFTsByIssuer.cpp
          handlers/NFTBuyOffers.cpp
          handlers/NFTHistory.cpp
          handlers/NFTInfo.cpp
          handlers/NFTOffersCommon.cpp
          handlers/NFTSellOffers.cpp
          handlers/NoRippleCheck.cpp
          handlers/Random.cpp
          handlers/TransactionEntry.cpp
)

target_include_directories(clio_rpc PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(
  clio_rpc
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
  INTERFACE Threads::Threads
)
