add_library(clio_etlng)

target_sources(
  clio_etlng
  PRIVATE impl/AmendmentBlockHandler.cpp
          impl/AsyncGrpcCall.cpp
          impl/Extraction.cpp
          impl/GrpcSource.cpp
          impl/Loading.cpp
          impl/Monitor.cpp
          impl/TaskManager.cpp
          impl/ext/Cache.cpp
          impl/ext/Core.cpp
          impl/ext/NFT.cpp
          impl/ext/Successor.cpp
)

target_link_libraries(clio_etlng PUBLIC clio_data)
