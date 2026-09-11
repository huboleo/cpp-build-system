cmake_minimum_required(VERSION 3.24)

project(bb LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(bb
    src/main.cpp
    src/string_utils.cpp
)

target_compile_features(bb PRIVATE cxx_std_23)
set_target_properties(bb PROPERTIES
    CXX_EXTENSIONS OFF
)

target_include_directories(bb PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
