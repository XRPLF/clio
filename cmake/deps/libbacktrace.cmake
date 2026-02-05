if ("${san}" STREQUAL "")
    target_compile_definitions(clio_options INTERFACE BOOST_STACKTRACE_LINK)
    target_compile_definitions(clio_options INTERFACE BOOST_STACKTRACE_USE_BACKTRACE)
    find_package(libbacktrace REQUIRED CONFIG)
else ()
    # Some sanitizers (TSAN and ASAN for sure) can't be used with libbacktrace because they have their own backtracing
    # capabilities and there are conflicts. In any case, this makes sure Clio code knows that backtrace is not
    # available. See relevant conan profiles for sanitizers where we disable stacktrace in Boost explicitly.
    target_compile_definitions(clio_options INTERFACE CLIO_WITHOUT_STACKTRACE)
    message(STATUS "Sanitizer enabled, disabling stacktrace")
endif ()
