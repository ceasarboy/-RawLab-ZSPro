cmake_minimum_required(VERSION 3.22.1)
project(rawlab_android LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(FetchContent)
FetchContent_Declare(libraw
    # LibRaw 0.22 adds recent cameras (Panasonic DC-S9 color data, ...).
    # Official tarball from libraw.org; RAWLAB_LIBRAW_SOURCE may point at a
    # pre-extracted tree to build offline.
    URL https://www.libraw.org/data/LibRaw-0.22.0.tar.gz
    URL_HASH SHA256=1071e6e8011593c366ffdadc3d3513f57c90202d526e133174945ec1dd53f2a1
)
FetchContent_MakeAvailable(libraw)
# Exclude upstream placeholder implementations, as in its Makefile.am.
file(GLOB_RECURSE LIBRAW_SOURCES CONFIGURE_DEPENDS "${libraw_SOURCE_DIR}/src/*.cpp")
list(FILTER LIBRAW_SOURCES EXCLUDE REGEX "_ph\\.cpp$")
add_library(raw SHARED ${LIBRAW_SOURCES})
target_include_directories(raw PUBLIC "${libraw_SOURCE_DIR}")
target_compile_definitions(raw PRIVATE USE_ZLIB)
# Keep RAW development responsive in the device-debug build, like sony2fuji_core.
target_compile_options(raw PRIVATE $<$<CONFIG:Debug>:-O3>)
target_link_libraries(raw PRIVATE z)
target_link_options(raw PRIVATE "-Wl,-z,max-page-size=16384")

get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
set(SONY2FUJI_LIBRAW_TARGET raw)
set(BUILD_CLI OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(SONY2FUJI_ENABLE_GPU ON CACHE BOOL "" FORCE)
set(SONY2FUJI_ENABLE_OPENMP OFF CACHE BOOL "" FORCE)
add_subdirectory("${REPO}/lutools" "${CMAKE_CURRENT_BINARY_DIR}/core")
add_library(rawlab-jni SHARED rawlab_jni.cpp)
target_include_directories(rawlab-jni PRIVATE "${REPO}/lutools/include")
target_link_libraries(rawlab-jni PRIVATE sony2fuji_core)
target_link_options(rawlab-jni PRIVATE "-Wl,-z,max-page-size=16384")

add_executable(rawlab_gpu_tests EXCLUDE_FROM_ALL "${REPO}/RawLabAndroid/tests/gpu_smoke.cpp")
target_include_directories(rawlab_gpu_tests PRIVATE "${REPO}/lutools/include")
target_link_libraries(rawlab_gpu_tests PRIVATE sony2fuji_core)
