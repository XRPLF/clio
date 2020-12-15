set(CLIO_INSTALL_DIR "/opt/clio")
set(CMAKE_INSTALL_PREFIX ${CLIO_INSTALL_DIR})

install(TARGETS clio_server DESTINATION bin)
# install(TARGETS clio_tests DESTINATION bin) # NOTE: Do we want to install the tests?

#install(FILES example-config.json DESTINATION etc RENAME config.json)
file(READ example-config.json config)
string(REGEX REPLACE "./clio_log" "/var/log/clio/" config "${config}")
file(WRITE ${CMAKE_BINARY_DIR}/install-config.json "${config}")
install(FILES ${CMAKE_BINARY_DIR}/install-config.json DESTINATION etc RENAME config.json)

configure_file("${CMAKE_SOURCE_DIR}/CMake/install/clio.service.in" "${CMAKE_BINARY_DIR}/clio.service")

install(FILES "${CMAKE_BINARY_DIR}/clio.service" DESTINATION /lib/systemd/system)

