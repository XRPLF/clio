add_library(clio_util)

target_sources(
  clio_util
  PRIVATE config/Config.cpp
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
          requests/RequestBuilder.cpp
          requests/Types.cpp
          requests/WsConnection.cpp
          requests/impl/SslContext.cpp
          Taggable.cpp
          TerminationHandler.cpp
          TxUtils.cpp
          LedgerUtils.cpp
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

if (is_gcc OR is_clang)
  # FIXME: needed on gcc and on clang-16 for now
  #
  # For some reason cmake doesn't propagate the compile definitions from clio_options so we need to add them here
  target_compile_definitions(clio_util PUBLIC BOOST_ASIO_DISABLE_CONCEPTS)
endif ()
