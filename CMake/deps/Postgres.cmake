set(POSTGRES_INSTALL_DIR ${CMAKE_BINARY_DIR}/postgres)
set(POSTGRES_LIBS pq pgcommon pgport)
ExternalProject_Add(postgres
    GIT_REPOSITORY https://github.com/postgres/postgres.git
    GIT_TAG REL_14_1
    GIT_SHALLOW 1
    LOG_CONFIGURE 1
    LOG_BUILD 1
    CONFIGURE_COMMAND ./configure --prefix ${POSTGRES_INSTALL_DIR} --without-readline --verbose
    BUILD_COMMAND ${CMAKE_COMMAND} -E env --unset=MAKELEVEL make VERBOSE=${CMAKE_VERBOSE_MAKEFILE} -j32
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND  ${CMAKE_COMMAND} -E env make -s --no-print-directory install
    UPDATE_COMMAND ""
    BUILD_BYPRODUCTS
        ${POSTGRES_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}pq${CMAKE_STATIC_LIBRARY_SUFFIX}}
        ${POSTGRES_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}pgcommon${CMAKE_STATIC_LIBRARY_SUFFIX}}
        ${POSTGRES_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}pgport${CMAKE_STATIC_LIBRARY_SUFFIX}}
        )

foreach(_lib ${POSTGRES_LIBS})
    add_library(${_lib} STATIC IMPORTED GLOBAL)
    add_dependencies(${_lib} postgres)
    set_target_properties(${_lib} PROPERTIES
        IMPORTED_LOCATION ${POSTGRES_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}${_lib}.a)
    set_target_properties(${_lib} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${POSTGRES_INSTALL_DIR}/include)
    target_link_libraries(clio PUBLIC ${POSTGRES_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}${_lib}${CMAKE_STATIC_LIBRARY_SUFFIX})
endforeach()

target_include_directories(clio PUBLIC ${POSTGRES_INSTALL_DIR}/include)
