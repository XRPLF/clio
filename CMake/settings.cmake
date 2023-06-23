target_compile_options(clio  
  PUBLIC -Wall
         #-Werror  # TODO: turn back on
         -Wno-narrowing
         -Wno-deprecated-declarations 
         -Wno-dangling-else
         -Wno-unused-local-typedef      # TODO: tmp added 
         -Wno-unused-but-set-variable)  # TODO: tmp added to stop annoying boost warnings
