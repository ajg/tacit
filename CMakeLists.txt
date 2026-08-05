cmake_minimum_required(VERSION 3.20)
project(tacit LANGUAGES CXX VERSION 0.7.0)

include(GNUInstallDirs)

add_library(tacit INTERFACE)
add_library(tacit::tacit ALIAS tacit)
target_include_directories(tacit INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(tacit INTERFACE cxx_std_23)

# ---- install + find_package(tacit) support ----
include(CMakePackageConfigHelpers)
set(TACIT_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/tacit)
install(TARGETS tacit EXPORT tacitTargets)
install(DIRECTORY include/ TYPE INCLUDE)
install(EXPORT tacitTargets NAMESPACE tacit:: DESTINATION ${TACIT_CMAKEDIR})
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/tacitConfigVersion.cmake
  COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/tacitConfig.cmake
  "include(\"\${CMAKE_CURRENT_LIST_DIR}/tacitTargets.cmake\")\n")
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/tacitConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/tacitConfigVersion.cmake
  DESTINATION ${TACIT_CMAKEDIR})

# tests only when tacit is the top-level project (skipped for add_subdirectory / FetchContent users)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include(CTest)
  if(BUILD_TESTING)
    add_subdirectory(tests)
  endif()
endif()
