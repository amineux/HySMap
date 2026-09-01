cmake_minimum_required(VERSION 3.20)
project(HySMap VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)

set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  eigen
  GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
  GIT_TAG 3.4.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(eigen)

add_library(hysmap
  src/hypergraph.cpp
  src/mesh.cpp
  src/cost.cpp
  src/incremental.cpp
  src/snn_generator.cpp
  src/partition.cpp
  src/placement.cpp
  src/mapper.cpp
  src/io.cpp
)
add_library(hysmap::hysmap ALIAS hysmap)

target_include_directories(hysmap
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(hysmap PUBLIC Eigen3::Eigen)
target_compile_features(hysmap PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(hysmap PRIVATE /W4 /permissive-)
else()
  target_compile_options(hysmap PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(hysmap_cli src/cli.cpp)
set_target_properties(hysmap_cli PROPERTIES OUTPUT_NAME hysmap)
target_link_libraries(hysmap_cli PRIVATE hysmap)
if(NOT MSVC)
  target_compile_options(hysmap_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(HYSMAP_BUILD_TESTS "Build HySMap unit tests" ON)
if(HYSMAP_BUILD_TESTS)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(Catch2)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(TARGETS hysmap hysmap_cli
  EXPORT hysmapTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/hysmap DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT hysmapTargets
  FILE hysmapTargets.cmake
  NAMESPACE hysmap::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hysmap
)
