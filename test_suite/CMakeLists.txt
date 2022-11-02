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
  run_test_suite
  COMMAND ${CMAKE_BINARY_DIR}/scripts/run_tests.py
    ${CMAKE_BINARY_DIR}/tx-cli/tx
    ${CMAKE_CURRENT_SOURCE_DIR}
  # DEPENDS copy_test_suite
)

add_test(
  NAME run_test_suite
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_tests.py ${CMAKE_BINARY_DIR}/tx-cli/tx
  DEPENDS copy_test_suite
)
set_tests_properties(
  run_test_suite
  PROPERTIES
  PASS_REGULAR_EXPRESSION "All [0-9]+ tests passed [(][0-9]+ expectations[)]."
)
