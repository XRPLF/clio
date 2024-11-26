add_library(clio_etlng)

target_sources(clio_etlng PRIVATE impl/AsyncGrpcCall.cpp impl/Extraction.cpp impl/GrpcSource.cpp)

target_link_libraries(clio_etlng PUBLIC clio_data)
