add_library(clio_web)

target_sources(clio_web PRIVATE impl/AdminVerificationStrategy.cpp IntervalSweepHandler.cpp Resolver.cpp)

target_include_directories(clio_web PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(
  clio_web
  PUBLIC Boost::boost
         Boost::coroutine
         Boost::program_options
         Boost::system
         Boost::log
         Boost::log_setup
         Boost::stacktrace_backtrace
         cassandra-cpp-driver::cassandra-cpp-driver
         fmt::fmt
         OpenSSL::Crypto
         OpenSSL::SSL
         xrpl::libxrpl
         dl
         libbacktrace::libbacktrace
         clio_util
  INTERFACE Threads::Threads
)
