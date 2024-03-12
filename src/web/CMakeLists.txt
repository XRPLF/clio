add_library(clio_web)

target_sources(clio_web PRIVATE impl/AdminVerificationStrategy.cpp IntervalSweepHandler.cpp Resolver.cpp)

target_link_libraries(clio_web PUBLIC clio_util)
