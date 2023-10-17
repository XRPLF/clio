include (CheckIncludeFileCXX)

check_include_file_cxx ("source_location" SOURCE_LOCATION_AVAILABLE)
if (SOURCE_LOCATION_AVAILABLE) 
    target_compile_definitions (clio PUBLIC "HAS_SOURCE_LOCATION")
endif ()

check_include_file_cxx ("experimental/source_location" EXPERIMENTAL_SOURCE_LOCATION_AVAILABLE)
if (EXPERIMENTAL_SOURCE_LOCATION_AVAILABLE)     
    target_compile_definitions (clio PUBLIC "HAS_EXPERIMENTAL_SOURCE_LOCATION")
endif ()
