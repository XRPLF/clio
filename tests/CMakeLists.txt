# Set coverage build options
if (coverage)
  if (NOT tests)
    message(FATAL_ERROR "Coverage requires tests to be enabled")
  endif ()
  include(CodeCoverage)
  append_coverage_compiler_flags_to_target(clio_options INTERFACE)
endif ()

if (tests OR integration_tests)
  add_subdirectory(common)
endif ()

if (tests)
  add_subdirectory(unit)
endif ()

if (integration_tests)
  add_subdirectory(integration)
endif ()
