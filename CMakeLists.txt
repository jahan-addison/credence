cmake_minimum_required(VERSION 3.14...3.22)

# ---- Project ----

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


project(
  roxas
  VERSION 1.0
  LANGUAGES CXX
)

include(GNUInstallDirs)


# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

if (WIN32)
    set(SUBSYSTEM_LINKER_OPTIONS "-mwindows")
endif()

# ---- Add dependencies via CPM ----
include(cmake/CPM.cmake)

# Add python-dev library to interface with python

find_package(PythonLibs REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

# ---- Add source files ----

# Note: globbing sources is considered bad practice as CMake's generators may not detect new files
# automatically. Keep that in mind when changing files, or explicitly mention them here.
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/*.cc")

# ---- Create standalone executable ----

add_executable(${PROJECT_NAME} ${sources})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20 OUTPUT_NAME "roxas")

target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}>
                         $<INSTALL_INTERFACE:${PROJECT_NAME}-${PROJECT_VERSION}>
)

target_include_directories(${PROJECT_NAME} PUBLIC ${PYTHON_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} ${PYTHON_LIBRARIES})
