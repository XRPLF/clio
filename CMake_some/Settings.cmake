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

# TODO: reenable when we change CI #884 if (is_gcc AND NOT lint) list(APPEND COMPILER_FLAGS -Wduplicated-branches
# -Wduplicated-cond -Wlogical-op -Wuseless-cast ) endif ()

if (is_clang)
  list(APPEND COMPILER_FLAGS -Wshadow # gcc is to aggressive with shadowing
                                      # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=78147
  )
endif ()

if (is_appleclang)
  list(APPEND COMPILER_FLAGS -Wreorder-init-list)
endif ()

# See https://github.com/cpp-best-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#gcc--clang for
# the flags description

target_compile_options(clio_options INTERFACE ${COMPILER_FLAGS})
