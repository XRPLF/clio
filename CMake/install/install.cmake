set(CLIO_INSTALL_DIR /opt/clio)
set(CMAKE_INSTALL_PREFIX ${CLIO_INSTALL_DIR})

install(TARGETS clio_server DESTINATION bin)
# install(TARGETS clio_tests DESTINATION bin) # NOTE: Do we want to install the tests?
install(FILES example-config.json DESTINATION etc/clio)
install(SCRIPT "${CMAKE_SOURCE_DIR}/CMake/install/PostInstall.cmake")
