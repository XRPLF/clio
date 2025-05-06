# Build snapshot tool needed to be installed before building: go protoc-gen-go protoc-gen-go-grpc

set(GO_EXECUTABLE "go")
set(GO_SOURCE_DIR "${CMAKE_SOURCE_DIR}/tools/snapshot")
set(PROTO_INC_DIR "${xrpl_PACKAGE_FOLDER_RELEASE}/include/xrpl/proto")
set(PROTO_SOURCE_DIR "${PROTO_INC_DIR}/org/xrpl/rpc/v1/")
set(GO_OUTPUT "${CMAKE_BINARY_DIR}/clio_snapshot")
file(GLOB_RECURSE GO_SOURCES ${GO_SOURCE_DIR}/*.go)

set(PROTO_FILES
    ${PROTO_SOURCE_DIR}/xrp_ledger.proto ${PROTO_SOURCE_DIR}/ledger.proto ${PROTO_SOURCE_DIR}/get_ledger.proto
    ${PROTO_SOURCE_DIR}/get_ledger_entry.proto ${PROTO_SOURCE_DIR}/get_ledger_data.proto
    ${PROTO_SOURCE_DIR}/get_ledger_diff.proto
)

execute_process(COMMAND go env GOPATH OUTPUT_VARIABLE GOPATH_VALUE OUTPUT_STRIP_TRAILING_WHITESPACE)

# Target Go package path
set(GO_IMPORT_PATH "org/xrpl/rpc/v1")

# Initialize the options strings
set(GO_OPTS "")
set(GRPC_OPTS "")

# Loop through each proto file
foreach (proto ${PROTO_FILES})
  get_filename_component(proto_filename ${proto} NAME)

  # Build the --go_opt and --go-grpc_opt mappings
  set(GO_OPTS ${GO_OPTS} --go_opt=M${GO_IMPORT_PATH}/${proto_filename}=${GO_IMPORT_PATH})
  set(GRPC_OPTS ${GRPC_OPTS} --go-grpc_opt=M${GO_IMPORT_PATH}/${proto_filename}=${GO_IMPORT_PATH})
endforeach ()

foreach (proto ${PROTO_FILES})
  get_filename_component(proto_name ${proto} NAME_WE)
  add_custom_command(
    OUTPUT ${GO_SOURCE_DIR}/${GO_IMPORT_PATH}/${proto_name}.pb.go
    COMMAND
      protoc ${GO_OPTS} ${GRPC_OPTS} --go-grpc_out=${GO_SOURCE_DIR} -I${PROTO_INC_DIR} ${proto}
      --plugin=${GOPATH_VALUE}/bin/protoc-gen-go --plugin=${GOPATH_VALUE}/bin/protoc-gen-go-grpc
      --go_out=${GO_SOURCE_DIR}/
    DEPENDS ${proto}
    COMMENT "Generating Go code for ${proto}"
    VERBATIM
  )

  list(APPEND GENERATED_GO_FILES ${GO_SOURCE_DIR}/${GO_IMPORT_PATH}/${proto_name}.pb.go)
endforeach ()

add_custom_target(build_clio_snapshot ALL DEPENDS run_go_tests ${GO_OUTPUT})

add_custom_target(
  run_go_tests
  COMMAND go test ./...
  WORKING_DIRECTORY ${GO_SOURCE_DIR}
  DEPENDS ${GENERATED_GO_FILES}
  COMMENT "Running clio_snapshot unittests"
  VERBATIM
)

add_custom_command(
  OUTPUT ${GO_OUTPUT}
  COMMAND ${GO_EXECUTABLE} build -o ${GO_OUTPUT} ${GO_SOURCE_DIR}
  WORKING_DIRECTORY ${GO_SOURCE_DIR}
  DEPENDS ${GO_SOURCES}
  COMMENT "Building clio_snapshot"
  VERBATIM
)
