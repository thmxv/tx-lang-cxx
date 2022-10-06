
# A very simple example of a configured file that might need to be
# converted to one that is publicly installed in the case that
# you are developing a library
configure_file(
  "config.hxx.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/internal_use_only/config.hxx"
  ESCAPE_QUOTES
)
target_sources(
  tx-runtime
  INTERFACE
    FILE_SET
      HEADERS
      BASE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/include"
      FILES "${CMAKE_CURRENT_BINARY_DIR}/include/internal_use_only/config.hxx"
)

