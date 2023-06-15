cmake_minimum_required(VERSION 3.25)
project(DirectXPlayground)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(external/minhook)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")

add_library(DirectXPlayground SHARED ${SOURCES})

target_include_directories(DirectXPlayground PRIVATE external/minhook/include)
