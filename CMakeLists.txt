cmake_minimum_required(VERSION 3.21)
project(ParallelKmeans VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(KMEANS_NATIVE "Add -march=native in Release builds" ON)
option(KMEANS_WERROR "Treat warnings as errors" OFF)
option(KMEANS_GUI "Build the raylib GUI" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(KMEANS_NATIVE)
    add_compile_options("$<$<CONFIG:Release>:-march=native>")
endif()

add_library(kmeans_warnings INTERFACE)
target_compile_options(kmeans_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow)
if(KMEANS_WERROR)
    target_compile_options(kmeans_warnings INTERFACE -Werror)
endif()

enable_testing()

add_subdirectory(src)
add_subdirectory(apps/cli)
if(KMEANS_GUI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/apps/gui/CMakeLists.txt")
    add_subdirectory(apps/gui)
endif()
add_subdirectory(tests)
