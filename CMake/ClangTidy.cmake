if (clang_tidy)

  function (find_clang_tidy _CLANG_TIDY_PATH)
    if (DEFINED ENV{CLIO_CLANG_TIDY_BIN})
      set (_CLANG_TIDY_PATH $ENV{CLIO_CLANG_TIDY_BIN})
      if (NOT EXISTS ${_CLANG_TIDY_PATH})
        message (FATAL_ERROR "$ENV{CLIO_CLANG_TIDY_BIN} no such file. Check CLIO_CLANG_TIDY_BIN env variable")
      endif ()
      message (STATUS "Using clang-tidy from CLIO_CLANG_TIDY_BIN")
    else ()
      find_program (_CLANG_TIDY_PATH NAMES "clang-tidy-16" "clang-tidy-15" "clang-tidy-14" "clang-tidy" REQUIRED)
    endif ()
    if (NOT _CLANG_TIDY_PATH)
      message (FATAL_ERROR
        "clang-tidy binary not found. Please set the CLIO_CLANG_TIDY_BIN environment variable or install clang-tidy.")
    endif ()
  endfunction ()

  # Support for https://github.com/matus-chochlik/ctcache
  find_program (CLANG_TIDY_CACHE_PATH NAMES "clang-tidy-cache")
  if (CLANG_TIDY_CACHE_PATH)
    find_clang_tidy (_CLANG_TIDY_PATH)
    set (CLANG_TIDY_PATH
        "${CLANG_TIDY_CACHE_PATH};${_CLANG_TIDY_PATH};${CLANG_TIDY_ARGS}"
        CACHE STRING "A combined command to run clang-tidy with caching wrapper")
  else ()
    find_clang_tidy (_CLANG_TIDY_PATH)
  endif ()
  set (CMAKE_CXX_CLANG_TIDY "${_CLANG_TIDY_PATH};--quiet")
  message (STATUS "Using clang-tidy: ${CMAKE_CXX_CLANG_TIDY}")
endif ()
