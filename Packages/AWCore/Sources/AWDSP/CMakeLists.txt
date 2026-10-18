# Builds AWDSP outside Xcode: on Linux or macOS, for the native tests and the offline renderer.
# (The app builds the same sources through Swift Package Manager.)
cmake_minimum_required(VERSION 3.20)
project(AWDSP CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

file(GLOB_RECURSE AWDSP_SOURCES CONFIGURE_DEPENDS Core/*.cpp DSP/*.cpp Modules/*.cpp)
add_library(awdsp STATIC ${AWDSP_SOURCES})
target_include_directories(awdsp PUBLIC include .)

set(TESTS ${CMAKE_CURRENT_SOURCE_DIR}/../../Tests/AWDSPNativeTests)
add_executable(awdsp-tests ${TESTS}/main.cpp)
target_link_libraries(awdsp-tests awdsp)
add_executable(aw-render ${TESTS}/render.cpp)
target_link_libraries(aw-render awdsp)

enable_testing()
add_test(NAME awdsp COMMAND awdsp-tests ${CMAKE_CURRENT_SOURCE_DIR}/../AWCore/Resources/DefaultState.json)
