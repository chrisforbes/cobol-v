cmake_minimum_required(VERSION 3.20)
project(cobolv VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files for core logic
set(CORE_SOURCES
    src/lexer.cpp
    src/parser.cpp
    src/semantics.cpp
    src/emitter.cpp
    src/builtins.cpp
)

set(HEADERS
    include/lexer.hpp
    include/parser.hpp
    include/emitter.hpp
)

# Main executable
add_executable(cobolv src/main.cpp)
target_link_libraries(cobolv PRIVATE cobolv_core)

# Testing
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
FetchContent_Declare(
  spirv_headers
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
  GIT_TAG main
)
FetchContent_Declare(
  amber
  GIT_REPOSITORY https://github.com/google/amber.git
  GIT_TAG main
)
# For Windows: do not override the build's shared runtime settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(AMBER_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(AMBER_SKIP_SPIRV_TOOLS ON CACHE BOOL "" FORCE)
set(AMBER_SKIP_SHADERC ON CACHE BOOL "" FORCE)
set(AMBER_SKIP_LODEPNG ON CACHE BOOL "" FORCE)
set(AMBER_SKIP_SAMPLES OFF CACHE BOOL "" FORCE)
set(SPIRV-Headers_SOURCE_DIR ${spirv_headers_SOURCE_DIR} CACHE STRING "" FORCE)

FetchContent_MakeAvailable(googletest spirv_headers)

FetchContent_GetProperties(amber)
if(NOT amber_POPULATED)
    FetchContent_Populate(amber)
    # Patch Amber samples to make image_diff optional (requires lodepng)
    file(READ "${amber_SOURCE_DIR}/samples/CMakeLists.txt" AMBER_SAMPLES_CMAKELISTS)
    if(NOT AMBER_SAMPLES_CMAKELISTS MATCHES "if\\(AMBER_ENABLE_LODEPNG\\)")
        string(REPLACE "set(IMAGE_DIFF_SOURCES" "if(AMBER_ENABLE_LODEPNG)\nset(IMAGE_DIFF_SOURCES" AMBER_SAMPLES_CMAKELISTS "${AMBER_SAMPLES_CMAKELISTS}")
        string(REPLACE "set_target_properties(image_diff PROPERTIES OUTPUT_NAME \"image_diff\")" "set_target_properties(image_diff PROPERTIES OUTPUT_NAME \"image_diff\")\nendif()" AMBER_SAMPLES_CMAKELISTS "${AMBER_SAMPLES_CMAKELISTS}")
        file(WRITE "${amber_SOURCE_DIR}/samples/CMakeLists.txt" "${AMBER_SAMPLES_CMAKELISTS}")
    endif()
    add_subdirectory(${amber_SOURCE_DIR} ${amber_BINARY_DIR})
endif()

enable_testing()

# Core library
add_library(cobolv_core ${CORE_SOURCES} ${HEADERS})
target_include_directories(cobolv_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${spirv_headers_SOURCE_DIR}/include
)

add_executable(test_lexer tests/test_lexer.cpp)
target_link_libraries(test_lexer PRIVATE cobolv_core gtest_main)

add_executable(test_parser tests/test_parser.cpp)
target_link_libraries(test_parser PRIVATE cobolv_core gtest_main)

add_executable(test_emitter tests/test_emitter.cpp)
target_link_libraries(test_emitter PRIVATE cobolv_core gtest_main)

add_executable(test_e2e tests/test_e2e.cpp)
target_link_libraries(test_e2e PRIVATE cobolv_core gtest_main)

include(GoogleTest)
gtest_discover_tests(test_lexer)
gtest_discover_tests(test_parser)
gtest_discover_tests(test_emitter)
gtest_discover_tests(test_e2e)
