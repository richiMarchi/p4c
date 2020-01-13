# Copyright 2013-present Barefoot Networks, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Makefile for the POLYCUBE P4-16 back-end.
# To be included in the main P4C compiler makefile

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY)

set (P4C_POLYCUBE_SRCS
  p4c-polycube.cpp
  polycubeBackend.cpp
  polycubeProgram.cpp
  polycubeTable.cpp
  polycubeControl.cpp
  polycubeParser.cpp
  polycubeOptions.cpp
  target.cpp
  polycubeType.cpp
  codeGen.cpp
  polycubeModel.cpp
  midend.cpp
  lower.cpp
  )

set (P4C_POLYCUBE_HDRS
  codeGen.h
  polycubeBackend.h
  polycubeControl.h
  polycubeModel.h
  polycubeObject.h
  polycubeProgram.h
  polycubeOptions.h
  polycubeParser.h
  polycubeTable.h
  polycubeType.h
  midend.h
  target.h
  lower.h
  )

set (SUPPORTS_KERNEL False)

add_cpplint_files(${CMAKE_CURRENT_SOURCE_DIR} "${P4C_POLYCUBE_SRCS};${P4C_POLYCUBE_HDRS}")

set (P4C_POLYCUBE_DIST_HEADERS p4include/polycube_model.p4)

build_unified(P4C_POLYCUBE_SRCS)
add_executable(p4c-polycube ${P4C_POLYCUBE_SRCS})
target_link_libraries (p4c-polycube ${P4C_LIBRARIES} ${P4C_LIB_DEPS})
add_dependencies(p4c-polycube genIR frontend)

install (TARGETS p4c-polycube
  RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})
install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/p4include
  DESTINATION ${P4C_ARTIFACTS_OUTPUT_DIRECTORY})

# hack to get around the fact that the test scripts expect the backend
# binary to be in the top level directory. This should go away when we
# remove automake and fix the scripts.
add_custom_target(linkp4cpolycube
  COMMAND ${CMAKE_COMMAND} -E create_symlink `realpath --relative-to=${P4C_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/p4c-polycube` ${P4C_BINARY_DIR}/p4c-polycube
  COMMAND ${CMAKE_COMMAND} -E make_directory ${P4C_BINARY_DIR}/p4include &&
          ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${P4C_POLYCUBE_DIST_HEADERS} ${P4C_BINARY_DIR}/p4include
  COMMAND ${CMAKE_COMMAND} -E create_symlink `realpath --relative-to=${CMAKE_CURRENT_BINARY_DIR} ${P4C_BINARY_DIR}/p4include` ${CMAKE_CURRENT_BINARY_DIR}/p4include
  COMMAND ${CMAKE_COMMAND} -E create_symlink `realpath --relative-to=${CMAKE_CURRENT_BINARY_DIR} ${P4C_BINARY_DIR}/p4_14include` ${CMAKE_CURRENT_BINARY_DIR}/p4_14include
  )

add_dependencies(p4c_driver linkp4cpolycube)

# needs to be installed
# p4include_HEADERS += $(srcdir)/%reldir%/p4include/polycube_model.p4