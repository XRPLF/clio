add_library(clio_etlng)

target_sources(
  clio_etlng PRIVATE impl/AmendmentBlockHandler.cpp impl/AsyncGrpcCall.cpp impl/Extraction.cpp impl/GrpcSource.cpp
                     impl/Loading.cpp
)

target_link_libraries(clio_etlng PUBLIC clio_data)
