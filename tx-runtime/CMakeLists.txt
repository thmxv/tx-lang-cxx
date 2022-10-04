find_package(fmt CONFIG REQUIRED)

# Generic test that uses conan libs
add_library(tx-runtime INTERFACE)
target_link_libraries(
  tx-runtime
  INTERFACE project_options project_warnings fmt::fmt-header-only
)

target_include_directories(
  tx-runtime
  # PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include"
  INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
