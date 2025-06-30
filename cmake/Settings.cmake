set(COMPILER_FLAGS
    -pedantic
    -Wall
    -Wcast-align
    -Wdouble-promotion
    -Werror
    -Wextra
    -Wformat=2
    -Wimplicit-fallthrough
    -Wmisleading-indentation
    -Wno-dangling-else
    -Wno-deprecated-declarations
    -Wno-narrowing
    -Wno-unused-but-set-variable
    -Wnon-virtual-dtor
    -Wnull-dereference
    -Wold-style-cast
    -Wpedantic
    -Wunreachable-code
    -Wunused
    # FIXME: The following bunch are needed for gcc12 atm.
    -Wno-missing-requires
    -Wno-restrict
    -Wno-null-dereference
    -Wno-maybe-uninitialized
    -Wno-unknown-warning-option # and this to work with clang
    # TODO: Address these and others in https://github.com/XRPLF/clio/issues/1273
)

# TODO: re-enable when we change CI #884 if (is_gcc AND NOT lint) list(APPEND COMPILER_FLAGS -Wduplicated-branches
# -Wduplicated-cond -Wlogical-op -Wuseless-cast ) endif ()

if (is_clang)
  list(APPEND COMPILER_FLAGS -Wshadow # gcc is to aggressive with shadowing
                                      # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=78147
  )
endif ()

if (is_appleclang)
  list(APPEND COMPILER_FLAGS -Wreorder-init-list)
endif ()

if (san)
  # When building with sanitizers some compilers will actually produce extra warnings/errors. We don't want this yet, at
  # least not until we have fixed all runtime issues reported by the sanitizers. Once that is done we can start removing
  # some of these and trying to fix it in our codebase. We can never remove all of below because most of them are
  # reported from deep inside libraries like boost or libxrpl.
  #
  # TODO: Address in https://github.com/XRPLF/clio/issues/1885
  list(
    APPEND
    COMPILER_FLAGS
    -Wno-error=tsan # Disables treating TSAN warnings as errors
    -Wno-tsan # Disables TSAN warnings (thread-safety analysis)
    -Wno-uninitialized # Disables warnings about uninitialized variables (AddressSanitizer, UndefinedBehaviorSanitizer,
                       # etc.)
    -Wno-stringop-overflow # Disables warnings about potential string operation overflows (AddressSanitizer)
    -Wno-unsafe-buffer-usage # Disables warnings about unsafe memory operations (AddressSanitizer)
    -Wno-frame-larger-than # Disables warnings about stack frame size being too large (AddressSanitizer)
    -Wno-unused-function # Disables warnings about unused functions (LeakSanitizer, memory-related issues)
    -Wno-unused-but-set-variable # Disables warnings about unused variables (MemorySanitizer)
    -Wno-thread-safety-analysis # Disables warnings related to thread safety usage (ThreadSanitizer)
    -Wno-thread-safety # Disables warnings related to thread safety usage (ThreadSanitizer)
    -Wno-sign-compare # Disables warnings about signed/unsigned comparison (UndefinedBehaviorSanitizer)
    -Wno-nonnull # Disables warnings related to null pointer dereferencing (UndefinedBehaviorSanitizer)
    -Wno-address # Disables warnings about address-related issues (UndefinedBehaviorSanitizer)
    -Wno-array-bounds # Disables array bounds checks (UndefinedBehaviorSanitizer)
  )
endif ()

# See https://github.com/cpp-best-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#gcc--clang for
# the flags description

if (time_trace)
  if (is_clang OR is_appleclang)
    list(APPEND COMPILER_FLAGS -ftime-trace)
  else ()
    message(FATAL_ERROR "Clang or AppleClang is required to use `-ftime-trace`")
  endif ()
endif ()

target_compile_options(clio_options INTERFACE ${COMPILER_FLAGS})
