foreach(t strict_using blanks using_underscore showcase extend tuple compose chaining combinators typelevel typeproject typeapply operators views compare nary vocabulary lift)
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE tacit::tacit)
  target_compile_options(${t} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra>)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
