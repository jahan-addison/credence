# ##############################################################################
# Copyright (c) Jahan Addison
#
# This software is dual-licensed under the Apache License, Version 2.0 or the
# GNU General Public License, Version 3.0 or later.
#
# You may use this work, in part or in whole, under the terms of either license.
#
# See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root for the
# full text of these licenses.
# ##############################################################################

# ##############################################################################
# Standard Library (cmake --build build --target stdlib)
# ##############################################################################

# The standard library is hand-written assembly in stdlib/<arch>/<os>/stdlib.s,
# and its object file is committed beside it, so the compiler and test suite
# never assemble it. The `stdlib` target re-assembles each object in the
# Dockerfile.stdlib image after a change to a stdlib.s file. Docker is required,
# and without it the target is unavailable and the committed objects are used.

# The platforms the `stdlib` target assembles. A cross-compilation toolchain
# file sets this to its own platform.
set(CREDENCE_STDLIB_PLATFORMS
    "arm64/bsd;arm64/linux;x86_64/bsd;x86_64/linux"
    CACHE STRING "Standard library platforms (<arch>/<os>) to assemble")

set(CREDENCE_STDLIB_IMAGE
    "credence-stdlib"
    CACHE STRING "Docker image tag of the standard library assemblers")

# The assembler of each platform. The Darwin objects are built for macOS 11, the
# first release on arm64, and macOS 10.15, the first 64-bit only release, so a
# program built for any later version links with them.
set(CREDENCE_STDLIB_ASSEMBLER_arm64_linux aarch64-linux-gnu-as)
set(CREDENCE_STDLIB_ASSEMBLER_x86_64_linux x86_64-linux-gnu-as)
set(CREDENCE_STDLIB_ASSEMBLER_arm64_bsd clang -c -x assembler -target
                                        arm64-apple-macos11.0)
set(CREDENCE_STDLIB_ASSEMBLER_x86_64_bsd clang -c -x assembler -target
                                         x86_64-apple-macos10.15)

# On macOS an app bundle is searched first by default, and `docker` would find
# the Docker Desktop application instead of the docker command.
set(CREDENCE_FIND_APPBUNDLE ${CMAKE_FIND_APPBUNDLE})
set(CMAKE_FIND_APPBUNDLE NEVER)
find_program(DOCKER_EXECUTABLE docker)
set(CMAKE_FIND_APPBUNDLE ${CREDENCE_FIND_APPBUNDLE})
if(NOT DOCKER_EXECUTABLE)
  message(
    STATUS "docker not found: `stdlib` target unavailable (the objects in "
           "stdlib/ are committed, so this only matters if you edit stdlib.s).")
  return()
endif()

# Files written in the container are owned by the host user.
set(CREDENCE_STDLIB_DOCKER_USER)
if(UNIX)
  execute_process(
    COMMAND id -u
    OUTPUT_VARIABLE CREDENCE_STDLIB_UID
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(
    COMMAND id -g
    OUTPUT_VARIABLE CREDENCE_STDLIB_GID
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(CREDENCE_STDLIB_DOCKER_USER
      --user "${CREDENCE_STDLIB_UID}:${CREDENCE_STDLIB_GID}")
endif()

# The image build is cached by docker, so it runs on every build of the target
# and an image removed since the last build is rebuilt. The build context is
# stdlib/, which keeps the build directories out of it.
add_custom_target(
  stdlib_image
  COMMAND
    ${DOCKER_EXECUTABLE} build --quiet --tag ${CREDENCE_STDLIB_IMAGE} --file
    "${CMAKE_CURRENT_SOURCE_DIR}/Dockerfile.stdlib"
    "${CMAKE_CURRENT_SOURCE_DIR}/stdlib"
  COMMENT "Building the ${CREDENCE_STDLIB_IMAGE} docker image"
  VERBATIM)

set(CREDENCE_STDLIB_OBJECTS)
foreach(platform IN LISTS CREDENCE_STDLIB_PLATFORMS)
  string(REPLACE "/" "_" platform_name "${platform}")
  if(NOT DEFINED CREDENCE_STDLIB_ASSEMBLER_${platform_name})
    message(
      FATAL_ERROR
        "Unknown standard library platform '${platform}' in "
        "CREDENCE_STDLIB_PLATFORMS, expected one of arm64/bsd, arm64/linux, "
        "x86_64/bsd, or x86_64/linux.")
  endif()
  set(source "stdlib/${platform}/stdlib.s")
  set(object "stdlib/${platform}/stdlib.o")
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/${object}"
    COMMAND
      ${DOCKER_EXECUTABLE} run --rm ${CREDENCE_STDLIB_DOCKER_USER} --volume
      "${CMAKE_CURRENT_SOURCE_DIR}/stdlib:/source/stdlib" --workdir /source
      ${CREDENCE_STDLIB_IMAGE} ${CREDENCE_STDLIB_ASSEMBLER_${platform_name}} -o
      ${object} ${source}
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${source}" stdlib_image
    COMMENT "Assembling ${object}"
    VERBATIM)
  list(APPEND CREDENCE_STDLIB_OBJECTS "${CMAKE_CURRENT_SOURCE_DIR}/${object}")
endforeach()

add_custom_target(stdlib DEPENDS ${CREDENCE_STDLIB_OBJECTS})
