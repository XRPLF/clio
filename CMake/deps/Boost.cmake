set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost 1.75 REQUIRED
  COMPONENTS
    program_options
    coroutine
    system
    log
    log_setup
)

target_include_directories(clio PUBLIC ${Boost_INCLUDE_DIRS})

target_compile_definitions(clio 
  PUBLIC 
    BOOST_ASIO_DISABLE_HANDLER_TYPE_REQUIREMENTS 
    BOOST_ASIO_HAS_STD_INVOKE_RESULT 
    BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT
    BOOST_BEAST_ALLOW_DEPRECATED
    BOOST_CONTAINER_FWD_BAD_DEQUE
    BOOST_COROUTINES_NO_DEPRECATION_WARNING)

target_link_libraries(clio
  PUBLIC
    Boost::boost
    Boost::coroutine
    Boost::program_options
    Boost::system
    Boost::log
    Boost::log_setup)
