cmake_minimum_required(VERSION 3.24)

project(bb LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(blake3
    URL https://github.com/BLAKE3-team/BLAKE3/archive/refs/tags/1.8.7.tar.gz
    SOURCE_SUBDIR c
)
FetchContent_MakeAvailable(blake3)

add_library(bb_runtime STATIC
    src/build_runner.cpp
    src/build.cpp
    src/process.cpp
    src/compilation_database.cpp
)
target_compile_features(bb_runtime PRIVATE cxx_std_23)
set_target_properties(bb_runtime PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(bb_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(bb_runtime PRIVATE nlohmann_json::nlohmann_json)

add_executable(bb
    src/main.cpp
    src/builder.cpp
    src/string_utils.cpp
    src/initializer.cpp
    src/hashing.cpp
    src/fingerprint.cpp
    src/dependency_file.cpp
    src/compiler.cpp
    src/file_lock.cpp
    src/build_runner_cache.cpp
)

target_compile_features(bb PRIVATE cxx_std_23)
target_link_libraries(bb PRIVATE bb_runtime BLAKE3::blake3 nlohmann_json::nlohmann_json)
target_compile_definitions(bb PRIVATE
    BB_RUNTIME_LIBRARY="$<TARGET_FILE:bb_runtime>"
    BB_SDK_INCLUDE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/include"
)
set_target_properties(bb PROPERTIES
    CXX_EXTENSIONS OFF
)

target_include_directories(bb PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
