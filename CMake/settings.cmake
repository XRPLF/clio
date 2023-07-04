target_compile_options(clio  
  PUBLIC -Wall
         -Werror
         -Wno-narrowing
         -Wno-deprecated-declarations 
         -Wno-dangling-else
         -Wno-unused-but-set-variable
         -Wno-sign-compare)

if("${CMAKE_C_COMPILER_ID}" MATCHES "GNU" OR "${CMAKE_CXX_COMPILER_ID}"
                                                   MATCHES "GNU")
  target_compile_options(clio PUBLIC -Wno-maybe-uninitialized)                                                 
endif()

