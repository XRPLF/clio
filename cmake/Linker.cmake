if(DEFINED CMAKE_LINKER_TYPE)
    message(STATUS "Custom linker is already set: ${CMAKE_LINKER_TYPE}")
    return()
endif()

find_program(MOLD_PATH mold)

if(MOLD_PATH AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Using Mold linker: ${MOLD_PATH}")
    set(CMAKE_LINKER_TYPE MOLD)
endif()
