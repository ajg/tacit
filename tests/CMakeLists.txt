foreach(t strict_using blanks showcase extend tuple compose chaining combinators typelevel typeproject typeapply operators views compare nary vocabulary lift make stateless get tverbs sigils dollar lambda gates callables shapes order_cdl order_cld order_dcl order_dlc order_lcd order_ldc order_d_only vocabulary_file readme)
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

