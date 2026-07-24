cmake_minimum_required(VERSION 3.20)
project(tacit LANGUAGES CXX VERSION 0.1.0)

add_library(tacit INTERFACE)
add_library(tacit::tacit ALIAS tacit)
target_include_directories(tacit INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(tacit INTERFACE cxx_std_23)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
