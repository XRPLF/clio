add_library(clio_web)

target_sources(
  clio_web PRIVATE impl/AdminVerificationStrategy.cpp impl/ServerSslContext.cpp IntervalSweepHandler.cpp Resolver.cpp
                   Server.cpp
)

target_link_libraries(clio_web PUBLIC clio_util)
