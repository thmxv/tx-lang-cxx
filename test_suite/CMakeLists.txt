cmake_minimum_required(VERSION 3.15...3.23)

project(CmakeConfigPackageTests LANGUAGES CXX)

# ---- Test as standalone project the exported config package ----

if(PROJECT_IS_TOP_LEVEL OR TEST_INSTALLED_VERSION)
  enable_testing()
  find_package(tx-lang CONFIG REQUIRED) # for tx-cli
endif()

# Test Suite
# add_custom_command(
#   OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tx
#   COMMAND ${CMAKE_COMMAND} -E copy_directory
#     ${CMAKE_CURRENT_SOURCE_DIR}/tx
#     ${CMAKE_CURRENT_BINARY_DIR}/tx
# )
# add_custom_target(
#   copy_test_suite ALL
#   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tx
# )
# set_property(
#   TARGET copy_test_suite
#   APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${CMAKE_CURRENT_BINARY_DIR}/tx
# )
add_custom_target(
  test_suite
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_tests.py ${CMAKE_BINARY_DIR}/tx-cli/tx
  # DEPENDS copy_test_suite
)
# set_tests_properties(
#   test_suite
#   PROPERTIES
#   PASS_REGULAR_EXPRESSION "All [0-9]+ tests passed [(][0-9]+ expectations[)]."
# )
