# Standalone consumer that pulls tacit in via add_subdirectory — the same path FetchContent and
# CPM.cmake take. Not part of the main test suite (it needs its own configure); driven by the
# `packaging` CI job, and runnable by hand: `cmake -S tests/packaging -B build-pkg && ctest --test-dir build-pkg`.
cmake_minimum_required(VERSION 3.20)
project(tacit_packaging_check CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. tacit_build)  # tacit as a subproject; its tests self-skip

add_executable(consume consume.cpp)
target_link_libraries(consume PRIVATE tacit::tacit)

enable_testing()
add_test(NAME consume COMMAND consume)
