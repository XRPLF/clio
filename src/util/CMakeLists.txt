add_subdirectory(build)

add_library(clio_util)

target_sources(
  clio_util
  PRIVATE Assert.cpp
          Coroutine.cpp
          CoroutineGroup.cpp
          log/Logger.cpp
          log/PrettyPath.cpp
          prometheus/Http.cpp
          prometheus/Label.cpp
          prometheus/MetricBase.cpp
          prometheus/MetricBuilder.cpp
          prometheus/MetricsFamily.cpp
          prometheus/OStream.cpp
          prometheus/Prometheus.cpp
          Random.cpp
          Retry.cpp
          Repeat.cpp
          requests/RequestBuilder.cpp
          requests/Types.cpp
          requests/WsConnection.cpp
          requests/impl/SslContext.cpp
          ResponseExpirationCache.cpp
          Shasum.cpp
          SignalsHandler.cpp
          StopHelper.cpp
          StringHash.cpp
          Taggable.cpp
          TerminationHandler.cpp
          TimeUtils.cpp
          TxUtils.cpp
          LedgerUtils.cpp
          config/Array.cpp
          config/ArrayView.cpp
          config/ConfigConstraints.cpp
          config/ConfigDefinition.cpp
          config/ConfigFileJson.cpp
          config/ObjectView.cpp
          config/Types.cpp
          config/ValueView.cpp
)

# This must be above the target_link_libraries call otherwise backtrace doesn't work
if ("${san}" STREQUAL "")
  target_link_libraries(clio_util PUBLIC Boost::stacktrace_backtrace)
endif ()

target_link_libraries(
  clio_util
  PUBLIC Boost::headers
         Boost::iostreams
         Boost::coroutine
         Boost::context
         fmt::fmt
         openssl::openssl
         xrpl::libxrpl
         Threads::Threads
         clio_options
         clio_rpc_center
         clio_build_version
  PRIVATE spdlog::spdlog
)

# FIXME: needed on gcc-12, clang-16 and AppleClang for now (known boost 1.82 issue for some compilers)
#
# For some reason cmake doesn't propagate the compile definitions from clio_options so we need to add them here
target_compile_definitions(clio_util PUBLIC BOOST_ASIO_DISABLE_CONCEPTS)
