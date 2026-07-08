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

cmake_minimum_required(VERSION 3.16...3.29)

set(CMAKE_POLICY_VERSION_MINIMUM 3.12)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(
    FATAL_ERROR
      "Do not build in-source. Please remove CMakeCache.txt and the CMakeFiles/ directory. Then build out-of-source."
  )
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(CMAKE_CXX_COMPILER "/opt/homebrew/bin/g++-15")

project(
  credence
  VERSION 1.1.1
  LANGUAGES CXX)

include(GNUInstallDirs)

if(MSVC)
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} /Zc:__cplusplus /utf-8 /DEBUG /D_CRT_SECURE_NO_DEPRECATE /W4 /WX"
  )
else()
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -Werror -Wpedantic -DDEBUG")
  else()
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wpedantic -O2 -march=native")
  endif()
endif()

include(cmake/CPM.cmake)

cpmaddpackage("gh:StableCoder/cmake-scripts#24.04")
cpmaddpackage("gh:TheLartians/PackageProject.cmake@1.8.0")

set_source_files_properties(doctest.h PROPERTIES SKIP_LINTING TRUE)

# Enable sanitizer with `-DUSE_SANITIZER=<VALUE>` argument to CMake Address,
# Memory, MemoryWithOrigins, Undefined, Thread, Leak, 'Address;Undefined'
if(USE_SANITIZER)
  include(${cmake-scripts_SOURCE_DIR}/sanitizers.cmake)
endif()

include(${cmake-scripts_SOURCE_DIR}/tools.cmake)

# Include-what-you-use
if(IWYU)
  find_program(IWYU_EXECUTABLE include-what-you-use)
  if(IWYU_EXECUTABLE)
    if(APPLE)
      set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
          "${IWYU_EXECUTABLE};;-isysroot $(xcrun --sdk macosx --show-sdk-path)")
    else()
      set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${IWYU_EXECUTABLE};;")
    endif()
    message(STATUS "IWYU enabled for C++ targets.")
  else()
    message(
      WARNING "Include-What-You-Use executable not found. IWYU will not be run."
    )
  endif()
endif()

# ##############################################################################
# Dependencies
# ##############################################################################

cpmaddpackage("gh:doctest/doctest@2.4.12")

cpmaddpackage(
  NAME
  cxxopts
  VERSION
  3.2.0
  GITHUB_REPOSITORY
  jarro2783/cxxopts
  OPTIONS
  "CXXOPTS_BUILD_EXAMPLES NO"
  "CXXOPTS_BUILD_TESTS NO"
  "CXXOPTS_ENABLE_INSTALL YES")

cpmaddpackage(
  NAME
  easyjson
  VERSION
  1.3.1
  GITHUB_REPOSITORY
  jahan-addison/easyjson
  OPTIONS
  "EASYJSON_BUILD_EXAMPLES OFF"
  "EASYJSON_BUILD_TESTS OFF")

cpmaddpackage("gh:fmtlib/fmt#12.1.0")

cpmaddpackage(NAME matchit VERSION 1.0.1 GITHUB_REPOSITORY BowenFu/matchit.cpp)

# ##############################################################################
# re2c (B language lexer)
# ##############################################################################

# credence/language/lexer.cc is generated from lexer.re and committed to the
# repo (re2c output is plain, dependency-free C++, so a normal build doesn't
# need re2c at all -- it's picked up like any other source below). This target
# is only for regenerating lexer.cc after editing lexer.re.
find_program(RE2C_EXECUTABLE re2c)
if(RE2C_EXECUTABLE)
  add_custom_target(
    regenerate_lexer
    COMMAND
      ${RE2C_EXECUTABLE} -o
      "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/language/lexer.cc"
      "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/language/lexer.re" -W
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Regenerating B language lexer (re2c) from lexer.re")
else()
  message(
    STATUS "re2c not found. `regenerate_lexer` target unavailable (lexer.cc is "
           "already generated and committed, so this only matters if you edit "
           "lexer.re). Install with e.g. `brew install re2c`.")
endif()

# ##############################################################################
# clang-tidy (cmake --build build --target tidy)
# ##############################################################################

# Files matching this regex are skipped by the `tidy` target below.
set(CLANG_TIDY_IGNORE_REGEX
    "^(?!.*/(test|\\.cache)/)(?!.*language/lexer\\.cc).*\\.cc$"
    CACHE STRING "run-clang-tidy file filter regex")

find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
find_program(RUN_CLANG_TIDY_EXECUTABLE NAMES run-clang-tidy run-clang-tidy.py)
if(CLANG_TIDY_EXECUTABLE AND RUN_CLANG_TIDY_EXECUTABLE)
  include(ProcessorCount)
  ProcessorCount(NPROC)
  set(TIDY_COMPILE_COMMANDS_DIR ${CMAKE_BINARY_DIR}/tidy)
  add_custom_target(
    tidy
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TIDY_COMPILE_COMMANDS_DIR}
    COMMAND
      ${CMAKE_COMMAND}
      -DSOURCE_COMPILE_COMMANDS=${CMAKE_BINARY_DIR}/compile_commands.json
      -DOUTPUT_COMPILE_COMMANDS=${TIDY_COMPILE_COMMANDS_DIR}/compile_commands.json
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dedup_compile_commands.cmake
    COMMAND ${RUN_CLANG_TIDY_EXECUTABLE} -p ${TIDY_COMPILE_COMMANDS_DIR} -j
            ${NPROC} -warnings-as-errors=* "${CLANG_TIDY_IGNORE_REGEX}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running clang-tidy over credence/ sources"
    VERBATIM)
else()
  message(
    STATUS "clang-tidy or run-clang-tidy not found: `tidy` target unavailable.")
endif()

# ##############################################################################
# Executables
# ##############################################################################

file(GLOB_RECURSE headers CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/*.cc")
file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cc")

add_executable(${PROJECT_NAME} ${sources} ${headers})

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}>
         $<INSTALL_INTERFACE:${PROJECT_NAME}-${PROJECT_VERSION}>)

target_link_libraries(${PROJECT_NAME} PUBLIC cxxopts::cxxopts fmt::fmt matchit
                                             easyjson)

include(cmake/doctest.cmake)
