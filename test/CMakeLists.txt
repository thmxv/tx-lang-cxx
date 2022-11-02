cmake_minimum_required(VERSION 3.15...3.23)

project(CmakeConfigPackageTests LANGUAGES CXX)

# ---- Test as standalone project the exported config package ----

if(PROJECT_IS_TOP_LEVEL OR TEST_INSTALLED_VERSION)
  enable_testing()
  find_package(tx-lang CONFIG REQUIRED) # for tx , project_options, ...
  if(NOT TARGET tx-lang::project_options)
    message(FATAL_ERROR "Required config package not found!")
    return() # be strictly paranoid for Template Janitor github action! CK
  endif()
endif()

# ---- Dependencies ----

find_package(Catch2 CONFIG REQUIRED)

include(Catch)

add_library(catch_main OBJECT catch_main.cxx)
target_link_libraries(catch_main PUBLIC Catch2::Catch2)
target_link_libraries(catch_main PRIVATE tx-lang::project_options)

# CLI Tests

# Provide a simple smoke test to make sure that the CLI works and can display
# a --help message
add_test(NAME cli.has_help COMMAND ../tx-cli/tx --help)

# Provide a test to verify that the version being reported from the application
# matches the version given to CMake. This will be important once you package
# your program. Real world shows that this is the kind of simple mistake that
# is easy to make, but also easy to test for.
add_test(NAME cli.version_matches COMMAND ../tx-cli/tx --version)
set_tests_properties(
  cli.version_matches
  PROPERTIES
    PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}"
)

# C++ Tests

add_executable(tests tests.cxx)
target_link_libraries(
  tests
  PRIVATE tx-lang::project_warnings tx-lang::project_options catch_main
)

# automatically discover tests that are defined in catch based test files you can modify the unittests. Set TEST_PREFIX
# to whatever you want, or use different for different binaries
catch_discover_tests(
  tests
  TEST_PREFIX "unittests."
  REPORTER xml
  OUTPUT_DIR .
  OUTPUT_PREFIX "unittests."
  OUTPUT_SUFFIX .xml
)

# Add a file containing a set of constexpr tests
add_executable(constexpr_tests constexpr_tests.cxx)
target_link_libraries(
  constexpr_tests
  PRIVATE tx-lang::project_options tx-lang::project_warnings catch_main
)

catch_discover_tests(
  constexpr_tests
  TEST_PREFIX "constexpr."
  REPORTER xml
  OUTPUT_DIR .
  OUTPUT_PREFIX "constexpr."
  OUTPUT_SUFFIX .xml
)

# Disable the constexpr portion of the test, and build again this allows
# us to have an executable that we can debug when
# things go wrong with the constexpr testing
add_executable(relaxed_constexpr_tests constexpr_tests.cxx)
target_link_libraries(
  relaxed_constexpr_tests
  PRIVATE tx-lang::project_options tx-lang::project_warnings catch_main
)
target_compile_definitions(
  relaxed_constexpr_tests
  PRIVATE -DCATCH_CONFIG_RUNTIME_STATIC_REQUIRE
)

catch_discover_tests(
  relaxed_constexpr_tests
  TEST_PREFIX "relaxed_constexpr."
  REPORTER xml
  OUTPUT_DIR .
  OUTPUT_PREFIX "relaxed_constexpr."
  OUTPUT_SUFFIX .xml
)
