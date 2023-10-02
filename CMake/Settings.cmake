set(COMPILER_FLAGS
  -Wall
  -Wcast-align
  -Wdouble-promotion
  -Wextra
  -Werror
  -Wformat=2
  -Wimplicit-fallthrough
  -Wmisleading-indentation
  -Wno-narrowing
  -Wno-deprecated-declarations
  -Wno-dangling-else
  -Wno-unused-but-set-variable
  -Wnon-virtual-dtor
  -Wnull-dereference
  -Wold-style-cast
  -pedantic
  -Wpedantic
  -Wunused
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  list(APPEND COMPILER_FLAGS
    -Wshadow # gcc is to aggressive with shadowing https://gcc.gnu.org/bugzilla/show_bug.cgi?id=78147
  )
endif ()

# See https://github.com/cpp-best-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#gcc--clang for the flags description

target_compile_options (clio PUBLIC ${COMPILER_FLAGS})
