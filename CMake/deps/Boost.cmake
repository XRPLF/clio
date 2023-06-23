target_link_directories(clio PUBLIC ${CONAN_LIB_DIRS_BOOST})
target_link_libraries(clio PUBLIC ${CONAN_LIBS_BOOST})
target_include_directories(clio PUBLIC ${CONAN_INCLUDE_DIRS_BOOST})
# set(Boost_USE_STATIC_LIBS ON)
# set(Boost_USE_STATIC_RUNTIME ON)

# find_package(Boost 1.75 REQUIRED
#   COMPONENTS
#     filesystem
#     program_options
#     system
#     thread
#     log
#     log_setup
# )

# add_library(clio_boost INTERFACE)
# add_library(Clio::boost ALIAS clio_boost)
# if(XCODE)
#   target_include_directories(clio_boost BEFORE INTERFACE ${Boost_INCLUDE_DIRS})
#   target_compile_options(clio_boost INTERFACE --system-header-prefix="boost/")
# else()
#   target_include_directories(clio_boost SYSTEM BEFORE INTERFACE ${Boost_INCLUDE_DIRS})
# endif()

# target_link_libraries(clio_boost
#   INTERFACE
#     Boost::boost
#     Boost::filesystem
#     Boost::program_options
#     Boost::system
#     Boost::thread
#     Boost::log
#     Boost::log_setup)
# if(Boost_COMPILER)
#   target_link_libraries(clio_boost INTERFACE Boost::disable_autolinking)
# endif()

# target_link_libraries(clio PUBLIC Clio::boost)
