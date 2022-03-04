execute_process(
  COMMAND ln -sf ${CMAKE_INSTALL_PREFIX}/${CLIO_INSTALL_DIR}/bin/clio_server /usr/local/bin/clio_server
  COMMAND_ECHO STDOUT
  )
