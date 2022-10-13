find_package(fmt CONFIG REQUIRED)

# Generic test that uses conan libs
add_executable(
  tx-cli
    src/main.cxx
)
set_target_properties(tx-cli PROPERTIES OUTPUT_NAME "tx")
target_link_libraries(
  tx-cli
  PRIVATE
    project_options
    project_warnings
    tx-runtime
    fmt::fmt-header-only
)
target_link_options(
  tx-cli
  PRIVATE
    -Wl,--gc-sections
)

# target_include_directories(
#   tx-cli
# )
