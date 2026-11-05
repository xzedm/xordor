# Turns on a useful set of compiler warnings for one of our own targets.
# Warnings catch real bugs (shadowed names, missing returns, implicit sign
# changes) early, which is cheaper than finding them in a debugger.
# Third-party targets are left alone: their warnings are not ours to fix.

function(xe_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE
      /W4          # high warning level
      /permissive- # standard-conforming C++
      /utf-8       # source files are UTF-8
    )
    if(XE_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic
      -Wshadow               # a local variable hides another one
      -Wnon-virtual-dtor     # deleting through a base pointer would leak
      -Woverloaded-virtual   # a "override" that does not override
      -Wnull-dereference
    )
    if(XE_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()
