add_library(clio_etl)

target_sources(
  clio_etl
  PRIVATE CacheLoaderSettings.cpp
          ETLHelpers.cpp
          ETLService.cpp
          ETLState.cpp
          LoadBalancer.cpp
          NetworkValidatedLedgers.cpp
          NFTHelpers.cpp
          Source.cpp
          impl/AmendmentBlockHandler.cpp
          impl/ForwardingCache.cpp
          impl/ForwardingSource.cpp
          impl/GrpcSource.cpp
          impl/SubscriptionSource.cpp
)

target_link_libraries(clio_etl PUBLIC clio_data)
