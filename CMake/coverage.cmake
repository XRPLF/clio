# call add_coverage(module_name) to add coverage targets for the given module
function(add_coverage module)
  if("${CMAKE_C_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang"
     OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang")
    message("[Coverage] Building with llvm Code Coverage Tools")
    # Using llvm gcov ; llvm install by xcode
    set(LLVM_COV_PATH /Library/Developer/CommandLineTools/usr/bin)
    if(NOT EXISTS ${LLVM_COV_PATH}/llvm-cov)
      message(FATAL_ERROR "llvm-cov not found! Aborting.")
    endif()

    # set Flags
    target_compile_options(${module} PRIVATE 
      -fprofile-instr-generate
      -fcoverage-mapping)
    
    target_link_options(${module} PUBLIC 
      -fprofile-instr-generate
      -fcoverage-mapping)

    target_compile_options(clio PRIVATE 
      -fprofile-instr-generate
      -fcoverage-mapping)
    
    target_link_options(clio PUBLIC 
      -fprofile-instr-generate
      -fcoverage-mapping)

    # llvm-cov
    add_custom_target(
      ${module}-ccov-preprocessing
      COMMAND LLVM_PROFILE_FILE=${module}.profraw $<TARGET_FILE:${module}>
      COMMAND ${LLVM_COV_PATH}/llvm-profdata merge -sparse ${module}.profraw -o
              ${module}.profdata
      DEPENDS ${module})

    add_custom_target(
      ${module}-ccov-show
      COMMAND ${LLVM_COV_PATH}/llvm-cov show $<TARGET_FILE:${module}>
              -instr-profile=${module}.profdata -show-line-counts-or-regions
      DEPENDS ${module}-ccov-preprocessing)

    # add summary for CI parse
    add_custom_target(
      ${module}-ccov-report
      COMMAND
        ${LLVM_COV_PATH}/llvm-cov report $<TARGET_FILE:${module}>
        -instr-profile=${module}.profdata
        -ignore-filename-regex=".*_makefiles|.*unittests|.*_deps"
        -show-region-summary=false
      DEPENDS ${module}-ccov-preprocessing)

    # exclude libs and unittests self
    add_custom_target(
      ${module}-ccov
      COMMAND
        ${LLVM_COV_PATH}/llvm-cov show $<TARGET_FILE:${module}>
        -instr-profile=${module}.profdata -show-line-counts-or-regions
        -output-dir=${module}-llvm-cov -format="html"
        -ignore-filename-regex=".*_makefiles|.*unittests|.*_deps" > /dev/null 2>&1
      DEPENDS ${module}-ccov-preprocessing)

    add_custom_command(
      TARGET ${module}-ccov
      POST_BUILD
      COMMENT
        "Open ${module}-llvm-cov/index.html in your browser to view the coverage report."
    )
  elseif("${CMAKE_C_COMPILER_ID}" MATCHES "GNU" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    message("[Coverage] Building with Gcc Code Coverage Tools")

    find_program(GCOV_PATH gcov)
    if(NOT GCOV_PATH)
      message(FATAL_ERROR "gcov not found! Aborting...")
    endif() # NOT GCOV_PATH
    find_program(GCOVR_PATH gcovr)
    if(NOT GCOVR_PATH)
      message(FATAL_ERROR "gcovr not found! Aborting...")
    endif() # NOT GCOVR_PATH

    set(COV_OUTPUT_PATH ${module}-gcc-cov)
    target_compile_options(${module} PRIVATE -fprofile-arcs -ftest-coverage
                                             -fPIC)
    target_link_libraries(${module} PRIVATE gcov)

    target_compile_options(clio PRIVATE -fprofile-arcs -ftest-coverage
                                             -fPIC)
    target_link_libraries(clio PRIVATE gcov)
    # this target is used for CI as well generate the summary out.xml will send
    # to github action to generate markdown, we can paste it to comments or
    # readme
    add_custom_target(
      ${module}-ccov
      COMMAND ${module} ${TEST_PARAMETER}
      COMMAND rm -rf ${COV_OUTPUT_PATH}
      COMMAND mkdir ${COV_OUTPUT_PATH}
      COMMAND
        gcovr -r ${CMAKE_SOURCE_DIR} --object-directory=${PROJECT_BINARY_DIR} -x
        ${COV_OUTPUT_PATH}/out.xml --exclude='${CMAKE_SOURCE_DIR}/unittests/'
        --exclude='${PROJECT_BINARY_DIR}/'
      COMMAND
        gcovr -r ${CMAKE_SOURCE_DIR} --object-directory=${PROJECT_BINARY_DIR}
        --html ${COV_OUTPUT_PATH}/report.html
        --exclude='${CMAKE_SOURCE_DIR}/unittests/'
        --exclude='${PROJECT_BINARY_DIR}/'
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
      COMMENT "Running gcovr to produce Cobertura code coverage report.")

    # generate the detail report
    add_custom_target(
      ${module}-ccov-report
      COMMAND ${module} ${TEST_PARAMETER}
      COMMAND rm -rf ${COV_OUTPUT_PATH}
      COMMAND mkdir ${COV_OUTPUT_PATH}
      COMMAND
        gcovr -r ${CMAKE_SOURCE_DIR} --object-directory=${PROJECT_BINARY_DIR}
        --html-details ${COV_OUTPUT_PATH}/index.html
        --exclude='${CMAKE_SOURCE_DIR}/unittests/'
        --exclude='${PROJECT_BINARY_DIR}/'
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
      COMMENT "Running gcovr to produce Cobertura code coverage report.")
    add_custom_command(
      TARGET ${module}-ccov-report
      POST_BUILD
      COMMENT
        "Open ${COV_OUTPUT_PATH}/index.html in your browser to view the coverage report."
    )
  else()
    message(FATAL_ERROR "Complier not support yet")
  endif()
endfunction()
