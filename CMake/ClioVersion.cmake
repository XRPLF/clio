#[===================================================================[
   read version from source
#]===================================================================]

file (STRINGS src/main/Build.cpp BUILD_INFO)
foreach (line_ ${BUILD_INFO})
  if (line_ MATCHES "versionString[ ]*=[ ]*\"(.+)\"")
    set (clio_version ${CMAKE_MATCH_1})
  endif ()
endforeach ()
if (clio_version)
  message (STATUS "clio version: ${clio_version}")
else ()
  message (FATAL_ERROR "unable to determine clio version")
endif ()
