add_library(clio)
target_sources(clio PRIVATE impl/Build.cpp)

target_link_libraries(clio PUBLIC clio_etl clio_feed clio_web clio_rpc)

target_compile_features(clio PUBLIC cxx_std_23)

# Clio server
add_executable(clio_server)
target_sources(clio_server PRIVATE Main.cpp)
target_link_libraries(clio_server PRIVATE clio)

if (static)
  target_link_options(clio_server PRIVATE -static)

  if (is_gcc AND NOT san)
    target_link_options(
      # For now let's assume that we only using libstdc++ under gcc.
      clio_server PRIVATE -static-libstdc++ -static-libgcc
    )
  endif ()

  if (is_appleclang)
    message(FATAL_ERROR "Static linkage not supported on AppleClang")
  endif ()
endif ()

set_target_properties(clio_server PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
