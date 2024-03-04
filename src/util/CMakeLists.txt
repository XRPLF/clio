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

target_link_libraries(clio_util PUBLIC Boost::headers fmt::fmt xrpl::libxrpl clio_options INTERFACE Threads::Threads)
