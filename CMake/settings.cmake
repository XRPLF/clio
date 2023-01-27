target_compile_options(clio  
  PUBLIC -Wall
         -Werror 
         -Wno-narrowing
         -Wno-deprecated-declarations 
         -Wno-dangling-else
         -Wextra 
         -Wshadow
         -Wnon-virtual-dtor
         -pedantic
         -Wno-error=unused-parameter
         )
