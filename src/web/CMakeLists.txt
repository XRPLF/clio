add_library(clio_web)

target_sources(
  clio_web
  PRIVATE AdminVerificationStrategy.cpp
          dosguard/DOSGuard.cpp
          dosguard/IntervalSweepHandler.cpp
          dosguard/Weights.cpp
          dosguard/WhitelistHandler.cpp
          Connection.cpp
          impl/ErrorHandling.cpp
          impl/ConnectionHandler.cpp
          impl/ServerSslContext.cpp
          Request.cpp
          Response.cpp
          Server.cpp
          SubscriptionContext.cpp
          Resolver.cpp
          SubscriptionContext.cpp
)

target_link_libraries(clio_web PUBLIC clio_util)
