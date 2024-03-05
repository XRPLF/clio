if (lint)

  # Find clang-tidy binary
  if (DEFINED ENV{CLIO_CLANG_TIDY_BIN})
    set(_CLANG_TIDY_BIN $ENV{CLIO_CLANG_TIDY_BIN})
    if ((NOT EXISTS ${_CLANG_TIDY_BIN}) OR IS_DIRECTORY ${_CLANG_TIDY_BIN})
      message(FATAL_ERROR "$ENV{CLIO_CLANG_TIDY_BIN} no such file. Check CLIO_CLANG_TIDY_BIN env variable")
    endif ()
    message(STATUS "Using clang-tidy from CLIO_CLANG_TIDY_BIN")
  else ()
    find_program(_CLANG_TIDY_BIN NAMES "clang-tidy-17" "clang-tidy" REQUIRED)
  endif ()

  if (NOT _CLANG_TIDY_BIN)
    message(
      FATAL_ERROR
        "clang-tidy binary not found. Please set the CLIO_CLANG_TIDY_BIN environment variable or install clang-tidy."
    )
  endif ()

  # Support for https://github.com/matus-chochlik/ctcache
  find_program(CLANG_TIDY_CACHE_PATH NAMES "clang-tidy-cache")
  if (CLANG_TIDY_CACHE_PATH)
    set(_CLANG_TIDY_CMD "${CLANG_TIDY_CACHE_PATH};${_CLANG_TIDY_BIN}"
        CACHE STRING "A combined command to run clang-tidy with caching wrapper"
    )
  else ()
    set(_CLANG_TIDY_CMD "${_CLANG_TIDY_BIN}")
  endif ()

  set(CMAKE_CXX_CLANG_TIDY "${_CLANG_TIDY_CMD};--quiet")
  message(STATUS "Using clang-tidy: ${CMAKE_CXX_CLANG_TIDY}")
endif ()
