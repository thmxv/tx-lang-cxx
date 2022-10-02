find_package(fmt CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)

# Generic test that uses conan libs
add_executable(tx main.cpp)
target_link_libraries(
  tx
  PUBLIC project_options project_warnings
  PRIVATE fmt::fmt-header-only CLI11::CLI11
)

target_include_directories(
  tx
  PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include"
)
