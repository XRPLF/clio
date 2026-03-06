add_executable(clio_benchmark)

target_sources(
    clio_benchmark
    PRIVATE
        # Common
        Main.cpp
        Playground.cpp
        # ExecutionContext
        util/async/ExecutionContextBenchmarks.cpp
        # Logger
        util/log/LoggerBenchmark.cpp
        # WorkQueue
        rpc/WorkQueueBenchmarks.cpp
)

include(deps/gbench)

target_include_directories(clio_benchmark PRIVATE .)
target_link_libraries(
    clio_benchmark
    PRIVATE clio_rpc clio_util benchmark::benchmark_main spdlog::spdlog
)
set_target_properties(
    clio_benchmark
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
