cmake_minimum_required(VERSION 3.22)

project(
  mandrel
  VERSION "0.0.1"
  DESCRIPTION "graphics hackingraphics hacking"
  LANGUAGES C CXX
)

include(FetchContent)

set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(FETCHCONTENT_QUIET FALSE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -m32")

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2)
FetchContent_GetProperties(googletest)
if (NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

FetchContent_Declare(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	GIT_TAG v1.92.6-docking)
FetchContent_MakeAvailable(imgui)
add_library(imguilib STATIC
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_dx9.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
 	${imgui_SOURCE_DIR}/imgui_demo.cpp
 	${imgui_SOURCE_DIR}/imgui_draw.cpp
 	${imgui_SOURCE_DIR}/imgui_tables.cpp
 	${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(imguilib PUBLIC ${imgui_SOURCE_DIR})

add_subdirectory(src)

enable_testing()
include(CTest)
add_subdirectory(tests)

set(CPACK_PACKAGE_NAME "mandrel")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_SOURCE_IGNORE_FILES "build")
include(CPack)

