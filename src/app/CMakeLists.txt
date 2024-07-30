add_library(clio_app)
target_sources(clio_app PRIVATE CliArgs.cpp ClioApplication.cpp)

target_link_libraries(clio_app PUBLIC clio_etl clio_feed clio_web clio_rpc)

