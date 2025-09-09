cmake_minimum_required(VERSION 3.16...3.29)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(
  credence
  VERSION 1.0.0
  LANGUAGES CXX
)

include(GNUInstallDirs)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus /utf-8 /DEBUG /D_CRT_SECURE_NO_DEPRECATE /W4 /WX")
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -Werror -Wpedantic -DDEBUG")
    else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wpedantic -O2 -march=native")
    endif()
endif()

include(cmake/CPM.cmake)

# https://github.com/StableCoder/cmake-scripts
CPMAddPackage("gh:StableCoder/cmake-scripts#24.04")
CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.8.0")


# Enable sanitizer with `-DUSE_SANITIZER=<VALUE>` argument to CMake
# enables sanitizers support using the the `USE_SANITIZER` flag available values are: Address,
# Memory, MemoryWithOrigins, Undefined, Thread, Leak, 'Address;Undefined'
if(USE_SANITIZER)
    include(${cmake-scripts_SOURCE_DIR}/sanitizers.cmake)
endif()

include(${cmake-scripts_SOURCE_DIR}/tools.cmake)

# Include-what-you-use

find_program(IWYU_EXECUTABLE include-what-you-use)
if(IWYU_EXECUTABLE)
    # Enable IWYU for C++ targets
    if (APPLE)
      set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${IWYU_EXECUTABLE};;-isysroot $(xcrun --sdk macosx --show-sdk-path)")
    else()
      set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${IWYU_EXECUTABLE};;")
    endif()
    message(STATUS "IWYU enabled for C++ targets.")
else()
    message(WARNING "Include-What-You-Use executable not found. IWYU will not be run.")
endif()

set_source_files_properties(doctest.h PROPERTIES SKIP_LINTING TRUE)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# Add python-dev library to interface with python

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Dependencies

CPMAddPackage("gh:doctest/doctest@2.4.9")

CPMAddPackage(
  NAME cxxopts
  VERSION 3.2.0
  GITHUB_REPOSITORY jarro2783/cxxopts
  OPTIONS "CXXOPTS_BUILD_EXAMPLES NO" "CXXOPTS_BUILD_TESTS NO" "CXXOPTS_ENABLE_INSTALL YES"
)

CPMAddPackage(
  NAME simplejson
  VERSION 1.1.4
  GITHUB_REPOSITORY jahan-addison/simplejson
  OPTIONS "BUILD_EXAMPLES OFF"
)

CPMAddPackage(
  NAME eternal
  VERSION 1.0.1
  GITHUB_REPOSITORY dirkarnez/eternal
  OPTIONS "WITH_TESTS OFF"
)

CPMAddPackage(
  NAME pybind11
  VERSION 3.0.1
  GITHUB_REPOSITORY pybind/pybind11
)

CPMAddPackage(
  NAME matchit
  VERSION 1.0.1
  GITHUB_REPOSITORY BowenFu/matchit.cpp
)

set(PYBIND11_FINDPYTHON ON)

# --- Executables ----

file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/*.cc")
file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cc")

add_executable(${PROJECT_NAME} ${sources} ${headers})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)

target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}>
                         $<INSTALL_INTERFACE:${PROJECT_NAME}-${PROJECT_VERSION}>
)

target_include_directories(${PROJECT_NAME} PUBLIC Python3::Python pybind11::headers cxxopts::cxxopts matchit simplejson ${CMAKE_BINARY_DIR}/_deps/eternal-src/include)
target_link_libraries(${PROJECT_NAME} PUBLIC Python3::Python pybind11::pybind11 cxxopts::cxxopts matchit simplejson eternal)

include(cmake/doctest.cmake)