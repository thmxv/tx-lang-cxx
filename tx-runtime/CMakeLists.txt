find_package(fmt CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)

add_library(
  tx-runtime
  INTERFACE
    include/tx/chunk.hxx
    include/tx/common.hxx
    include/tx/debug.hxx
    include/tx/dyn_array.hxx
    include/tx/memory.hxx
    include/tx/type_traits.hxx
    include/tx/value.hxx
    # 
    include/tx/debug_inl.hxx
    include/tx/memory_inl.hxx
    #
    include/tx/tx.hxx
)
target_link_libraries(
  tx-runtime
  INTERFACE
    project_options
    project_warnings
    fmt::fmt-header-only
    Microsoft.GSL::GSL
)
target_include_directories(
  tx-runtime
  INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
add_subdirectory(configured_files)
