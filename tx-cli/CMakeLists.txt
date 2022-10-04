find_package(fmt CONFIG REQUIRED)

# Generic test that uses conan libs
add_executable(tx-cli src/main.cxx)
set_target_properties(tx-cli PROPERTIES OUTPUT_NAME "tx")
target_link_libraries(
  tx-cli
  PUBLIC project_options project_warnings
  PRIVATE fmt::fmt-header-only
)

target_include_directories(
  tx-cli
  PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include"
)
