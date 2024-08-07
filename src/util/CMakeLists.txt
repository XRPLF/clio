add_library(clio_util)

target_sources(
  clio_util
  PRIVATE build/Build.cpp
          config/Config.cpp
          log/Logger.cpp
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
          SignalsHandler.cpp
          Taggable.cpp
          TerminationHandler.cpp
          TimeUtils.cpp
          TxUtils.cpp
          LedgerUtils.cpp
          newconfig/ConfigDefinition.cpp
          newconfig/ObjectView.cpp
          newconfig/ArrayView.cpp
          newconfig/ValueView.cpp
)

target_link_libraries(
  clio_util
  PUBLIC Boost::headers
         Boost::stacktrace_backtrace
         dl
         libbacktrace::libbacktrace
         fmt::fmt
         openssl::openssl
         xrpl::libxrpl
         Threads::Threads
         clio_options
)

# FIXME: needed on gcc-12, clang-16 and AppleClang for now (known boost 1.82 issue for some compilers)
#
# For some reason cmake doesn't propagate the compile definitions from clio_options so we need to add them here
target_compile_definitions(clio_util PUBLIC BOOST_ASIO_DISABLE_CONCEPTS)
