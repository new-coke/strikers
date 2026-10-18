cmake_minimum_required(VERSION 3.24)
project(strikers_asset_tools LANGUAGES CXX)

# Host converters only: no VitaSDK, game executable, SDL, Aurora or GPU.
get_filename_component(STRIKERS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include(FetchContent)
FetchContent_Declare(xxhash
    URL "https://github.com/Cyan4973/xxHash/archive/refs/tags/v0.8.3.tar.gz"
    URL_HASH SHA256=aae608dfe8213dfd05d909a57718ef82f30722c392344583d3f39050c7f29a80
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR strikers_headers_only)
FetchContent_MakeAvailable(xxhash)

add_executable(strikers_compile_native_graphics
    ../compile_native_graphics.cpp
    ../../src/platform/native_graphics.cpp
    ../../src/platform/native_texture_record.cpp)
target_compile_features(strikers_compile_native_graphics PRIVATE cxx_std_20)
target_compile_definitions(strikers_compile_native_graphics PRIVATE STRIKERS_NATIVE_TEXTURE_TEST=1)
target_include_directories(strikers_compile_native_graphics PRIVATE
    "${STRIKERS_ROOT}/include" "${xxhash_SOURCE_DIR}")

add_executable(strikers_compile_native_audio ../compile_native_audio.cpp)
target_compile_features(strikers_compile_native_audio PRIVATE cxx_std_17)
target_include_directories(strikers_compile_native_audio PRIVATE "${STRIKERS_ROOT}/include")

install(TARGETS strikers_compile_native_graphics strikers_compile_native_audio RUNTIME DESTINATION bin)
