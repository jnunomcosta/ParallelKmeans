cmake_minimum_required(VERSION 3.21)
project(ParallelKmeans VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(KMEANS_NATIVE "Add -march=native in Release builds" ON)
option(KMEANS_WERROR "Treat warnings as errors" OFF)
option(KMEANS_GUI "Build the raylib GUI" OFF)
# Sanitizers for our targets, e.g. "address,undefined". ThreadSanitizer is not supported:
# GCC's libgomp is not TSan-instrumented and produces false positives.
set(KMEANS_SANITIZE "" CACHE STRING "Value for -fsanitize= (empty: off)")

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

if(KMEANS_SANITIZE)
    target_compile_options(kmeans_warnings INTERFACE
        -fsanitize=${KMEANS_SANITIZE} -fno-omit-frame-pointer)
    target_link_options(kmeans_warnings INTERFACE -fsanitize=${KMEANS_SANITIZE})
endif()

enable_testing()

if(EMSCRIPTEN)
    # The web build is the GUI only, single-threaded (no OpenMP, no CLI, no tests).
    set(KMEANS_GUI ON CACHE BOOL "Build the raylib GUI" FORCE)
endif()

add_subdirectory(src)
if(NOT EMSCRIPTEN)
    add_subdirectory(apps/cli)
endif()
if(KMEANS_GUI)
    add_subdirectory(apps/gui)
endif()
if(NOT EMSCRIPTEN)
    add_subdirectory(tests)
endif()
