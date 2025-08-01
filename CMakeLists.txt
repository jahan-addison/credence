cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(
  roxas
  VERSION 1.0
  LANGUAGES CXX
)

include(GNUInstallDirs)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus /utf-8 /D_CRT_SECURE_NO_DEPRECATE /W4 /WX")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wpedantic")
endif()

include(cmake/CPM.cmake)

# https://github.com/StableCoder/cmake-scripts
CPMAddPackage("gh:StableCoder/cmake-scripts#24.04")

# Enable sanitizer with `-DUSE_SANITIZER=<VALUE>` argument to CMake
# enables sanitizers support using the the `USE_SANITIZER` flag available values are: Address,
# Memory, MemoryWithOrigins, Undefined, Thread, Leak, 'Address;Undefined'
if(USE_SANITIZER)
    include(${cmake-scripts_SOURCE_DIR}/sanitizers.cmake)
endif()

include(${cmake-scripts_SOURCE_DIR}/tools.cmake)

if(DEFINED ENV{CI})
  include_what_you_use()
endif()

cppcheck("--enable=all"
         "--language=c++"
         "--error-exitcode=1"
         "--force"
         "--suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/cppcheck.suppress"
         "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}"
         "${CMAKE_CURRENT_SOURCE_DIR}/test"
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# Add python-dev library to interface with python

find_package (Python3 COMPONENTS Interpreter Development)
include_directories(${PYTHON_INCLUDE_DIRS})

# CPM Packages

CPMAddPackage(
  NAME cxxopts
  VERSION 3.2.0
  GITHUB_REPOSITORY jarro2783/cxxopts
)

# Create standalone executable

file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/*.cc")

add_executable(${PROJECT_NAME} ${sources})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20 OUTPUT_NAME "roxas")

target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}>
                         $<INSTALL_INTERFACE:${PROJECT_NAME}-${PROJECT_VERSION}>
)

target_include_directories(${PROJECT_NAME} PUBLIC Python3::Python cxxopts::cxxopts)
target_link_libraries(${PROJECT_NAME} PUBLIC Python3::Python cxxopts::cxxopts)
