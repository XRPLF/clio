add_library(clio)
target_sources(clio PRIVATE impl/Build.cpp)

target_link_libraries(clio PUBLIC clio_etl clio_feed clio_web clio_rpc)

target_compile_features(clio PUBLIC cxx_std_20)

if (is_gcc)
  # FIXME: needed on gcc for now
  target_compile_definitions(clio PUBLIC BOOST_ASIO_DISABLE_CONCEPTS)
endif ()

# Clio server
add_executable(clio_server)
target_sources(clio_server PRIVATE Main.cpp)
target_link_libraries(clio_server PRIVATE clio)
target_link_options(
  clio_server PRIVATE $<$<AND:$<NOT:$<BOOL:${APPLE}>>,$<NOT:$<BOOL:${san}>>>:-static-libstdc++ -static-libgcc>
)
set_target_properties(clio_server PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
