#[===================================================================[
   Apply sanitizer flags built by the Conan profile.

   Parsing, validation, and flag construction are performed in
   conan/profiles/sanitizers. This module reads the following CMake variables
   injected by the Conan toolchain via extra_variables:

   - SANITIZERS:                The active sanitizers (e.g. "address").
   - SANITIZERS_COMPILER_FLAGS: Space-separated compiler flags.
   - SANITIZERS_LINKER_FLAGS:   Space-separated linker flags.

   It defines SANITIZERS_ENABLED for the rest of the build to key off, and
   applies the flags to the 'clio_options' interface library.
#]===================================================================]

include_guard(GLOBAL)

if(NOT DEFINED SANITIZERS)
    set(SANITIZERS_ENABLED FALSE)
    return()
endif()
set(SANITIZERS_ENABLED TRUE)

message(STATUS "=== Configuring sanitizers ===")
message(STATUS "  SANITIZERS: ${SANITIZERS}")
message(STATUS "  Compile flags: ${SANITIZERS_COMPILER_FLAGS}")
message(STATUS "  Link flags: ${SANITIZERS_LINKER_FLAGS}")

# Flags arrive as space-separated strings; split into CMake lists before use
separate_arguments(
    sanitizers_compiler_flags
    UNIX_COMMAND
    "${SANITIZERS_COMPILER_FLAGS}"
)
separate_arguments(
    sanitizers_linker_flags
    UNIX_COMMAND
    "${SANITIZERS_LINKER_FLAGS}"
)

target_compile_options(
    clio_options
    INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:${sanitizers_compiler_flags}>
        $<$<COMPILE_LANGUAGE:C>:${sanitizers_compiler_flags}>
)
target_link_options(clio_options INTERFACE ${sanitizers_linker_flags})
