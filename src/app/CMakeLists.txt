add_library(clio_app)
target_sources(
    clio_app
    PRIVATE CliArgs.cpp ClioApplication.cpp Stopper.cpp WebHandlers.cpp
)

target_link_libraries(
    clio_app
    PUBLIC clio_cluster clio_etl clio_feed clio_migration clio_rpc clio_web
    PRIVATE Boost::program_options
)
