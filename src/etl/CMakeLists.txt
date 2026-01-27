add_library(clio_etl)

target_sources(
  clio_etl
  PRIVATE CacheLoaderSettings.cpp
          ETLHelpers.cpp
          ETLService.cpp
          ETLState.cpp
          LoadBalancer.cpp
          MPTHelpers.cpp
          NetworkValidatedLedgers.cpp
          NFTHelpers.cpp
          Source.cpp
          WriterState.cpp
          impl/AmendmentBlockHandler.cpp
          impl/AsyncGrpcCall.cpp
          impl/Extraction.cpp
          impl/ForwardingSource.cpp
          impl/GrpcSource.cpp
          impl/Loading.cpp
          impl/Monitor.cpp
          impl/SubscriptionSource.cpp
          impl/TaskManager.cpp
          impl/ext/Cache.cpp
          impl/ext/Core.cpp
          impl/ext/MPT.cpp
          impl/ext/NFT.cpp
          impl/ext/Successor.cpp
)

target_link_libraries(clio_etl PUBLIC clio_data)
