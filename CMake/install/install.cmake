set(CLIO_INSTALL_DIR "/opt/clio")
set(CMAKE_INSTALL_PREFIX ${CLIO_INSTALL_DIR})

install(CODE "execute_process(COMMAND strip --remove-section=.comment --remove-section=.note $<TARGET_FILE:clio_server>)")
install(TARGETS clio_server DESTINATION bin)
# install(TARGETS clio_tests DESTINATION bin) # NOTE: Do we want to install the tests?

file(READ example-config.json config)
string(REGEX REPLACE "./clio.log" "/var/log/clio/clio.log" config "${config}")
string(REGEX REPLACE 50051 51233 config "${config}")
file(WRITE ${CMAKE_BINARY_DIR}/install-config.json "${config}")
install(FILES ${CMAKE_BINARY_DIR}/install-config.json DESTINATION etc RENAME config.json)

install(SCRIPT "${CMAKE_SOURCE_DIR}/CMake/install/PostInstall.cmake")
# configure_file("${CMAKE_SOURCE_DIR}/CMake/clio.service.in" "${CMAKE_BINARY_DIR}/clio.service")

pkg_check_modules(SYSTEMD "systemd")
if(SYSTEMD_FOUND AND "${SYSTEMD_SERVICES_INSTALL_DIR}" STREQUAL "")
    execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE}
        --variable=systemdsystemunitdir systemd
        OUTPUT_VARIABLE SYSTEMD_SERVICES_INSTALL_DIR)
    string(REGEX REPLACE "[ \t\n]+" "" SYSTEMD_SERVICES_INSTALL_DIR
        "${SYSTEMD_SERVICES_INSTALL_DIR}")
elseif(NOT SYSTEMD_FOUND AND SYSTEMD_SERVICES_INSTALL_DIR)
    message (FATAL_ERROR "Variable SYSTEMD_SERVICES_INSTALL_DIR is\
        defined, but we can't find systemd using pkg-config")
endif()

if(SYSTEMD_FOUND)
    set(WITH_SYSTEMD "ON")
    message(STATUS "systemd services install dir: ${SYSTEMD_SERVICES_INSTALL_DIR}")
else()
    set(WITH_SYSTEMD "OFF")
endif(SYSTEMD_FOUND)

if(SYSTEMD_FOUND)
  configure_file("${CMAKE_SOURCE_DIR}/CMake/clio.service.in" "${CMAKE_BINARY_DIR}/clio.service")
  install(FILES "${CMAKE_BINARY_DIR}/clio.service" DESTINATION ${SYSTEMD_SERVICES_INSTALL_DIR})
endif(SYSTEMD_FOUND)
