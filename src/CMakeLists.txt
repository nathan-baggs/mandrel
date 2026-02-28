add_library(mandrel INTERFACE)

target_include_directories(mandrel INTERFACE ${PROJECT_SOURCE_DIR}/include)

target_sources(mandrel INTERFACE
  ${PROJECT_SOURCE_DIR}/include/mandrel/allocators/page_allocator.h
  ${PROJECT_SOURCE_DIR}/include/mandrel/allocators/free_list_allocator.h
  ${PROJECT_SOURCE_DIR}/include/mandrel/allocators/global_allocator.h
)

install(TARGETS mandrel DESTINATION bin)

target_compile_options(mandrel INTERFACE
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
target_compile_features(mandrel INTERFACE cxx_std_26)
target_compile_definitions(mandrel INTERFACE -DNOMINMAX)
target_link_libraries(mandrel INTERFACE stdc++exp)
