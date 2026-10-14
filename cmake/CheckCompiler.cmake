include(CompilationEnv)

set(is_appleclang FALSE)

if(is_clang) # both Clang and AppleClang
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
        set(is_appleclang TRUE)
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
            message(FATAL_ERROR "AppleClang 15+ required for building clio")
        endif()
    elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
        message(FATAL_ERROR "Clang 16+ required for building clio")
    endif()
elseif(is_gcc)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12)
        message(FATAL_ERROR "GCC 12+ required for building clio")
    endif()
else()
    message(
        FATAL_ERROR
        "Supported compilers: AppleClang 15+, Clang 16+, GCC 12+"
    )
endif()
