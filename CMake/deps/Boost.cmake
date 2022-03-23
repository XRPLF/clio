set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost 1.75 COMPONENTS filesystem log_setup log thread system REQUIRED)

target_link_libraries(clio PUBLIC ${Boost_LIBRARIES})
