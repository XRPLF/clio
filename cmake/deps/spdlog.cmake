find_package(spdlog REQUIRED)

if(NOT TARGET spdlog::spdlog)
    message(FATAL_ERROR "spdlog::spdlog target not found")
endif()
