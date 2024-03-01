set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost 1.82 REQUIRED CONFIG COMPONENTS program_options coroutine system log log_setup)
