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
  -Woverloaded-virtual
  -pedantic
  -Wpedantic
  -Wunused
)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  list(APPEND COMPILER_FLAGS
    -Wduplicated-branches
    -Wduplicated-cond
    -Wlogical-op
    -Wuseless-cast
  )
endif ()

# See https://github.com/cpp-best-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#gcc--clang for the flags description

target_compile_options (clio PUBLIC ${COMPILER_FLAGS})
