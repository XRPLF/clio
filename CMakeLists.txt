#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

set(CMAKE_VERBOSE_MAKEFILE TRUE)
project(reporting)
cmake_minimum_required(VERSION 3.17)
set (CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)
if ((NOT DEFINED BOOST_ROOT) AND (DEFINED ENV{BOOST_ROOT}))
  set (BOOST_ROOT $ENV{BOOST_ROOT})
endif ()
file (TO_CMAKE_PATH "${BOOST_ROOT}" BOOST_ROOT)

FIND_PACKAGE( Boost 1.75 COMPONENTS filesystem log log_setup thread system REQUIRED )

add_executable (reporting
    websocket_server_async.cpp
)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps")
include(ExternalProject)
message(${CMAKE_CURRENT_BINARY_DIR})
message(${CMAKE_MODULE_PATH})
include(cassandra)
#include(rippled/Builds/CMake/RippledCore.cmake)
add_subdirectory(rippled)
target_link_libraries(reporting PUBLIC xrpl_core grpc_pbufs)
add_dependencies(reporting xrpl_core)
add_dependencies(reporting grpc_pbufs)
get_target_property(grpc_includes grpc_pbufs INCLUDE_DIRECTORIES)
#get_target_property(xrpl_core_includes xrpl_core INCLUDE_DIRECTORIES)
get_target_property(proto_includes protobuf_src INCLUDE_DIRECTORIES)
message("hi")
message("${grpc_includes}")
message("${proto_includes}")
ExternalProject_Get_Property(protobuf_src SOURCE_DIR)
message("${SOURCE_DIR}")
INCLUDE_DIRECTORIES(${grpc_includes})
#INCLUDE_DIRECTORIES(${xrpl_core_includes})
INCLUDE_DIRECTORIES(${SOURCE_DIR}/src)
ExternalProject_Get_Property(grpc_src SOURCE_DIR)
INCLUDE_DIRECTORIES(${SOURCE_DIR}/include)
#include (FetchContent)
#FetchContent_Declare(
#  rippled
#  GIT_REPOSITORY https://github.com/ripple/rippled.git
#  GIT_TAG develop
#)
#FetchContent_MakeAvailable(rippled)


#include(FetchContent)
#
#FetchContent_Declare(
#  gRPC
#  GIT_REPOSITORY https://github.com/grpc/grpc
#  GIT_TAG        v1.28.0
#  )
#set(FETCHCONTENT_QUIET OFF)
#FetchContent_MakeAvailable(gRPC)
#
#
#
#set(_PROTOBUF_LIBPROTOBUF libprotobuf)
#set(_REFLECTION grpc++_reflection)
#set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
#set(_GRPC_GRPCPP grpc++)
#set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
#message(${CMAKE_CURRENT_BINARY_DIR})
#
#set (GRPC_GEN_DIR "${CMAKE_BINARY_DIR}/proto_gen_grpc")
#file (MAKE_DIRECTORY ${GRPC_GEN_DIR})
#set (GRPC_PROTO_SRCS)
#set (GRPC_PROTO_HDRS)
#set (GRPC_PROTO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/rippled/src/ripple/proto/org")
#file(GLOB_RECURSE GRPC_DEFINITION_FILES LIST_DIRECTORIES false "${GRPC_PROTO_ROOT}/*.proto")
#
#foreach(file ${GRPC_DEFINITION_FILES})
#  get_filename_component(_abs_file ${file} ABSOLUTE)
#  get_filename_component(_abs_dir ${_abs_file} DIRECTORY)
#  get_filename_component(_basename ${file} NAME_WE)
#  get_filename_component(_proto_inc ${GRPC_PROTO_ROOT} DIRECTORY) # updir one level
#  file(RELATIVE_PATH _rel_root_file ${_proto_inc} ${_abs_file})
#  get_filename_component(_rel_root_dir ${_rel_root_file} DIRECTORY)
#  file(RELATIVE_PATH _rel_dir ${CMAKE_CURRENT_SOURCE_DIR} ${_abs_dir})
#
#  set (src_1 "${GRPC_GEN_DIR}/${_rel_root_dir}/${_basename}.grpc.pb.cc")
#  set (src_2 "${GRPC_GEN_DIR}/${_rel_root_dir}/${_basename}.pb.cc")
#  set (hdr_1 "${GRPC_GEN_DIR}/${_rel_root_dir}/${_basename}.grpc.pb.h")
#  set (hdr_2 "${GRPC_GEN_DIR}/${_rel_root_dir}/${_basename}.pb.h")
#
#    add_custom_command(
#          OUTPUT "${src_1}" "${src_2}" "${hdr_1}" "${hdr_2}"
#          COMMAND ${_PROTOBUF_PROTOC}
#    ARGS --grpc_out=${GRPC_GEN_DIR}
#         --cpp_out=${GRPC_GEN_DIR}
#            --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
#         -I ${_proto_inc} -I ${_rel_dir}
#         ${_abs_file}
#          DEPENDS ${_abs_file}
#    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
#    set_source_files_properties(${src_1} ${src_2} ${hdr_1} ${hdr_2} PROPERTIES GENERATED TRUE)
#    list(APPEND GRPC_PROTO_SRCS ${src_1} ${src_2})
#    list(APPEND GRPC_PROTO_HDRS ${hdr_1} ${hdr_2})
#endforeach()
#
#add_library (grpc_protobufs STATIC ${GRPC_PROTO_SRCS} ${GRPC_PROTO_HDRS})
##target_include_directories (grpc_pbufs PRIVATE src)
#target_include_directories (grpc_protobufs SYSTEM PUBLIC ${GRPC_GEN_DIR})
#target_link_libraries (grpc_protobufs ${_REFLECTION} ${_PROTOBUF_LIBPROTOBUF} ${_GRPC_GRPCPP})


target_sources(reporting PRIVATE reporting/ETLSource.cpp reporting/ReportingBackend.cpp)


message(${Boost_LIBRARIES})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR} ${Boost_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
TARGET_LINK_LIBRARIES(reporting PUBLIC ${Boost_LIBRARIES})
#TARGET_LINK_LIBRARIES(reporting PUBLIC ${Boost_LIBRARIES})
