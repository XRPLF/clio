add_library(clio_web)

target_sources(
  clio_web
  PRIVATE Resolver.cpp
          dosguard/DOSGuard.cpp
          dosguard/IntervalSweepHandler.cpp
          dosguard/WhitelistHandler.cpp
          impl/AdminVerificationStrategy.cpp
          ng/Connection.cpp
          ng/impl/ConnectionHandler.cpp
          ng/impl/ServerSslContext.cpp
          ng/impl/WsConnection.cpp
          ng/Server.cpp
          ng/Request.cpp
          ng/Response.cpp
)

target_link_libraries(clio_web PUBLIC clio_util)
