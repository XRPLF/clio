set(CLIO_INSTALL_DIR "/opt/clio")
set(CMAKE_INSTALL_PREFIX "${CLIO_INSTALL_DIR}" CACHE PATH "Install prefix" FORCE)

set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

include(GNUInstallDirs)

install(TARGETS clio_server DESTINATION "${CMAKE_INSTALL_BINDIR}")

file(READ docs/examples/config/example-config.json config)
string(REGEX REPLACE "./clio_log" "/var/log/clio/" config "${config}")
file(WRITE ${CMAKE_BINARY_DIR}/install-config.json "${config}")
install(FILES ${CMAKE_BINARY_DIR}/install-config.json DESTINATION etc RENAME config.json)

configure_file("${CMAKE_SOURCE_DIR}/cmake/install/clio.service.in" "${CMAKE_BINARY_DIR}/clio.service")
install(FILES "${CMAKE_BINARY_DIR}/clio.service" DESTINATION /lib/systemd/system)
