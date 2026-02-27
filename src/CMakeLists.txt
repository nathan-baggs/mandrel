add_library(mandrel STATIC
  main.cpp
)

install(TARGETS mandrel DESTINATION bin)

target_compile_options(mandrel PRIVATE
  -Wall
  -Wextra
  -pedantic
  -Werror
  -Wconversion-null
  -Wmissing-declarations
  -Woverlength-strings
  -Wpointer-arith
  -Wunused-local-typedefs
  -Wunused-result
  -Wvarargs
  -Wvla
  -Wwrite-strings
)
target_compile_features(mandrel PUBLIC cxx_std_26)
target_compile_definitions(mandrel PUBLIC -DNOMINMAX)
target_link_libraries(mandrel PUBLIC stdc++exp)
