add_library(clio_web)

target_sources(
  clio_web
  PRIVATE Resolver.cpp
          Server.cpp
          dosguard/DOSGuard.cpp
          dosguard/IntervalSweepHandler.cpp
          dosguard/WhitelistHandler.cpp
          impl/AdminVerificationStrategy.cpp
          impl/ServerSslContext.cpp
          ng/Connection.cpp
          ng/Server.cpp
          ng/impl/ServerSslContext.cpp
)

target_link_libraries(clio_web PUBLIC clio_util)
