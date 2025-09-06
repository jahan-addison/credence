option(ENABLE_TEST_COVERAGE "Enable test coverage" OFF)
option(TEST_INSTALLED_VERSION "Test the version found by find_package" ON)

list(REMOVE_ITEM sources "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/main.cc")

add_executable(Test_Suite ${sources} ${test_sources})

target_link_libraries(Test_Suite doctest::doctest Python3::Python matchit cxxopts::cxxopts simplejson)

set_target_properties(Test_Suite PROPERTIES CXX_STANDARD 20 OUTPUT_NAME "test_suite")

target_include_directories(
  Test_Suite PUBLIC $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}>
                         $<INSTALL_INTERFACE:${PROJECT_NAME}-${PROJECT_VERSION}>
)

# enable compiler warnings
if(NOT TEST_INSTALLED_VERSION)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(Test_Suite PUBLIC -DDEBUG -Wall -Wpedantic -Wextra -Werror)
  elseif(MSVC)
    target_compile_options(Test_Suite PUBLIC /W4 /WX)
    target_compile_definitions(Test_Suite PUBLIC DOCTEST_CONFIG_USE_STD_HEADERS)
  endif()
endif()

enable_testing()

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

doctest_discover_tests(Test_Suite)

# ---- code coverage ----

if(ENABLE_TEST_COVERAGE)
  target_compile_options(Test_Suite PUBLIC -O0 -g -fprofile-arcs -ftest-coverage)
  target_link_options(Test_Suite PUBLIC -fprofile-arcs -ftest-coverage)
endif()
