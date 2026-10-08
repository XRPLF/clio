# Custom CMake command definitions for gersemi formatting.
# These stubs teach gersemi the signatures of project-specific commands
# so it can format their invocations correctly.

function(setup_target_for_coverage_gcovr)
    set(options NONE)
    set(oneValueArgs BASE_DIRECTORY NAME FORMAT)
    set(multiValueArgs EXCLUDE EXECUTABLE EXECUTABLE_ARGS DEPENDENCIES)
    cmake_parse_arguments(
        THIS_FUNCTION_PREFIX
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )
endfunction()

function(append_coverage_compiler_flags_to_target name mode)
endfunction()

function(patch_nix_binary target)
endfunction()

function(rpcspec_generate_instantiations)
    set(options)
    set(oneValueArgs OUT_VAR VALUE_TYPE VIEW_HEADER INCLUDE_DIR)
    set(multiValueArgs HANDLERS)
    cmake_parse_arguments(
        THIS_FUNCTION_PREFIX
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )
endfunction()
