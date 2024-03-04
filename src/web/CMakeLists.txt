add_library(clio_web)

target_sources(clio_web PRIVATE impl/AdminVerificationStrategy.cpp IntervalSweepHandler.cpp Resolver.cpp)

target_include_directories(clio_web PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(clio_web PUBLIC clio_util)
