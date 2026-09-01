add_executable(hysmap_tests
  test_hypergraph.cpp
  test_routing.cpp
  test_incremental.cpp
  test_partition.cpp
  test_mapper.cpp
  test_export.cpp
)
target_link_libraries(hysmap_tests PRIVATE hysmap Catch2::Catch2WithMain)
target_compile_features(hysmap_tests PRIVATE cxx_std_20)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(hysmap_tests)
