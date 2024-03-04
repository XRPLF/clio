add_executable(clio_benchmark)

target_sources(
  clio_benchmark
  PRIVATE # Common
          benchmarks/Main.cpp
          benchmarks/Playground.cpp
          # ExecutionContext
          benchmarks/util/async/ExecutionContextBenchmarks.cpp
)

include(deps/gbench)

target_include_directories(${BENCH_TARGET} PRIVATE benchmarks)
target_link_libraries(${BENCH_TARGET} PUBLIC clio benchmark::benchmark_main)
