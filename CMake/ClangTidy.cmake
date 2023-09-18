if (clang_tidy)
  find_program (CLANG_TIDY_CACHE_PATH NAMES "clang-tidy-cache")
  if (CLANG_TIDY_CACHE_PATH)
    find_program (_CLANG_TIDY_PATH NAMES "clang-tidy-16" "clang-tidy-15" "clang-tidy-14" "clang-tidy" REQUIRED)
    set (CLANG_TIDY_PATH
        "${CLANG_TIDY_CACHE_PATH};${_CLANG_TIDY_PATH};${CLANG_TIDY_ARGS}"
        CACHE STRING "A combined command to run clang-tidy with caching wrapper")
  else ()
    find_program(_CLANG_TIDY_PATH NAMES "clang-tidy-16" "clang-tidy-15" "clang-tidy-14" "clang-tidy" REQUIRED)
  endif ()
  set (CLANG_TIDY_ARGS "--quiet" CACHE STRING "Additional args for clang-tidy")
  set (CMAKE_CXX_CLANG_TIDY "${_CLANG_TIDY_PATH};${CLANG_TIDY_ARGS}")
  message (STATUS "Using clang-tidy: ${CMAKE_CXX_CLANG_TIDY}")
endif ()
