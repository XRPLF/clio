# Clio server
add_executable(clio_server)
target_sources(clio_server PRIVATE Main.cpp)
target_link_libraries(clio_server PRIVATE clio_app)

set_target_properties(
    clio_server
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

patch_nix_binary(clio_server)
