if(NOT DEFINED DOCS)
  return()
endif()

find_package(Doxygen)
if (DOXYGEN_FOUND)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/README.md DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    message("Doxygen build started")

    # note the option ALL which allows to build the docs together with the application
    add_custom_target( clio_docs ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Clio API documentation"
        VERBATIM )
else (DOXYGEN_FOUND)
  message("Doxygen need to be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)
