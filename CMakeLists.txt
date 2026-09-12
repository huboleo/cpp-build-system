cmake_minimum_required(VERSION 3.24)

project(bb LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(bb_compilation_database STATIC src/compilation_database.cpp)
target_compile_features(bb_compilation_database PRIVATE cxx_std_23)
set_target_properties(bb_compilation_database PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(bb_compilation_database PRIVATE nlohmann_json::nlohmann_json)

add_executable(bb
    src/main.cpp
    src/builder.cpp
    src/string_utils.cpp
    src/initializer.cpp
    src/process.cpp
)

target_compile_features(bb PRIVATE cxx_std_23)
target_link_libraries(bb PRIVATE nlohmann_json::nlohmann_json bb_compilation_database)
target_compile_definitions(bb PRIVATE
    BB_COMPILATION_DATABASE_LIBRARY="$<TARGET_FILE:bb_compilation_database>"
)
set_target_properties(bb PROPERTIES
    CXX_EXTENSIONS OFF
)

target_include_directories(bb PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
