add_library(clio_web)

target_sources(
  clio_web
  PRIVATE AdminVerificationStrategy.cpp
          dosguard/DOSGuard.cpp
          dosguard/IntervalSweepHandler.cpp
          dosguard/WhitelistHandler.cpp
          ng/Connection.cpp
          ng/impl/ErrorHandling.cpp
          ng/impl/ConnectionHandler.cpp
          ng/impl/ServerSslContext.cpp
          ng/impl/WsConnection.cpp
          ng/Request.cpp
          ng/Response.cpp
          ng/Server.cpp
          ng/SubscriptionContext.cpp
          Resolver.cpp
          SubscriptionContext.cpp
)

target_link_libraries(clio_web PUBLIC clio_util)
