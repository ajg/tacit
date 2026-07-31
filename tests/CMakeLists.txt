foreach(t strict_using blanks showcase extend tuple compose chaining combinators typelevel typeproject typeapply operators views compare nary vocabulary lift make stateless get tverbs sigils dollar lambda gates callables vocabulary_file readme)
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE tacit::tacit)
  target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_options(${t} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra>
    # /utf-8 (λ.hpp and the comment prose) and /Zc:preprocessor (the X-macro engine uses
    # __VA_OPT__, and /std:c++latest does NOT imply the conforming preprocessor) are load-bearing.
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/utf-8;/Zc:preprocessor>)
  add_test(NAME ${t} COMMAND ${t})
endforeach()

# The single-file headers: built against single/ ONLY — deliberately not linked to tacit::tacit, so
# the include path proves the files are standalone. single/ is generated (tools/amalgamate) and
# committed; CI regenerates and diffs it, which is what keeps this test honest against edits to
# include/tacit/.
add_executable(single_check single_check.cpp)
target_compile_features(single_check PRIVATE cxx_std_23) # usually carried by tacit::tacit, absent here
target_include_directories(single_check PRIVATE ${CMAKE_SOURCE_DIR}/single)
target_compile_options(single_check PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra>
  $<$<CXX_COMPILER_ID:MSVC>:/W4;/utf-8;/Zc:preprocessor>)
add_test(NAME single_check COMMAND single_check)
