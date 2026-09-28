cmake_minimum_required(VERSION 3.24)

project(bb LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BB_BUILD_TESTS "Build bb's tests" ON)

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

set(BB_SDK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Code used by both bb and the build runner.
add_library(bb_shared OBJECT
    src/build.cpp
    src/process.cpp
    src/compilation_database.cpp
    src/compile_flags.cpp
    src/file_lock.cpp
)
target_compile_features(bb_shared PUBLIC cxx_std_23)
target_include_directories(bb_shared PUBLIC
    "${BB_SDK_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(bb_shared PRIVATE nlohmann_json::nlohmann_json)
target_compile_definitions(bb_shared PRIVATE BB_SDK_INCLUDE_DIR="${BB_SDK_INCLUDE_DIR}")

# Linked into every project's build runner, together with its build.cpp.
add_library(bb_runtime STATIC
    src/build_runner.cpp
)
target_link_libraries(bb_runtime PRIVATE bb_shared)

# Everything in the bb executable except main(), so the tests can link it too.
add_library(bb_core STATIC
    src/builder.cpp
    src/string_utils.cpp
    src/initializer.cpp
    src/hashing.cpp
    src/fingerprint.cpp
    src/dependency_file.cpp
    src/compiler.cpp
    src/build_runner_cache.cpp
)
target_link_libraries(bb_core PUBLIC bb_shared BLAKE3::blake3 nlohmann_json::nlohmann_json)
target_compile_definitions(bb_core PRIVATE
    BB_RUNTIME_LIBRARY="$<TARGET_FILE:bb_runtime>"
)
# bb compiles build runners against this archive, so it must exist whenever bb does.
add_dependencies(bb_core bb_runtime)

add_executable(bb
    src/main.cpp
)
target_link_libraries(bb PRIVATE bb_core)

set_target_properties(bb_shared bb_runtime bb_core bb PROPERTIES
    CXX_EXTENSIONS OFF
)

if(BB_BUILD_TESTS)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    )
    set(BUILD_GMOCK OFF)
    set(INSTALL_GTEST OFF)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(bb_tests
        tests/build_api_test.cpp
        tests/cli_test.cpp
        tests/compilation_database_test.cpp
        tests/compile_flags_test.cpp
        tests/compiler_test.cpp
        tests/dependency_file_test.cpp
        tests/file_lock_test.cpp
        tests/fingerprint_test.cpp
        tests/hashing_test.cpp
        tests/initializer_test.cpp
        tests/process_test.cpp
        tests/string_utils_test.cpp
    )
    target_link_libraries(bb_tests PRIVATE bb_core GTest::gtest_main nlohmann_json::nlohmann_json)
    target_compile_definitions(bb_tests PRIVATE
        BB_EXECUTABLE="$<TARGET_FILE:bb>"
        BB_SDK_INCLUDE_DIR="${BB_SDK_INCLUDE_DIR}"
    )
    set_target_properties(bb_tests PROPERTIES CXX_EXTENSIONS OFF)
    # The command-line tests run the real bb binary.
    add_dependencies(bb_tests bb)

    include(GoogleTest)
    gtest_discover_tests(bb_tests)
endif()
