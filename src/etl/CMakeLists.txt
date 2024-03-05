add_library(clio_etl)

target_sources(
  clio_etl
  PRIVATE NFTHelpers.cpp
          ETLService.cpp
          ETLState.cpp
          LoadBalancer.cpp
          CacheLoaderSettings.cpp
          Source.cpp
          impl/ForwardingSource.cpp
          impl/GrpcSource.cpp
          impl/SubscriptionSource.cpp
)

target_link_libraries(clio_etl PUBLIC clio_data)
