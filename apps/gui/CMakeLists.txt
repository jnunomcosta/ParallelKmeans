include(FetchContent)

if(EMSCRIPTEN)
    set(PLATFORM Web CACHE STRING "" FORCE)
    # raylib's miniaudio uses EM_ASM, which needs -std=gnu*.
    set(CMAKE_C_EXTENSIONS ON)
endif()

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)

FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 5.5
                            GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)

# raygui is header-only: point SOURCE_SUBDIR at a folder that does not exist so that
# its CMake project is not added.
FetchContent_Declare(raygui GIT_REPOSITORY https://github.com/raysan5/raygui.git GIT_TAG 4.0
                            GIT_SHALLOW TRUE SOURCE_SUBDIR none)
FetchContent_MakeAvailable(raygui)

if(NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
endif()

# raygui produces warnings, so its implementation is built without our warning flags.
add_library(raygui_impl STATIC raygui_impl.c)
target_include_directories(raygui_impl SYSTEM PUBLIC ${raygui_SOURCE_DIR}/src)
target_link_libraries(raygui_impl PUBLIC raylib)
target_compile_options(raygui_impl PRIVATE -w)

add_executable(kmeans_gui main.c font.c runner.c view.c palette.c panel.c timeline.c stats.c image_mode.c)
set_target_properties(kmeans_gui PROPERTIES OUTPUT_NAME kmeans-gui)
target_link_libraries(kmeans_gui PRIVATE kmeans::kmeans raygui_impl raylib m)
target_link_libraries(kmeans_gui PRIVATE kmeans_warnings)

if(EMSCRIPTEN)
    # The font is embedded in the wasm module and found at the root of its virtual file system.
    target_compile_definitions(kmeans_gui PRIVATE KMEANS_GUI_FONT_FILE="/DejaVuSans.ttf")
    target_compile_options(kmeans_gui PRIVATE -Wno-unknown-pragmas)
    set_target_properties(kmeans_gui PROPERTIES SUFFIX ".html")
    target_link_options(kmeans_gui PRIVATE -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sASYNCIFY=0
                        --embed-file ${CMAKE_CURRENT_SOURCE_DIR}/assets/DejaVuSans.ttf@/DejaVuSans.ttf
                        --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)
    set_target_properties(kmeans_gui PROPERTIES LINK_DEPENDS
                          ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)
else()
    # Absolute path, so the binary finds the font wherever it is run from.
    target_compile_definitions(kmeans_gui PRIVATE
                               KMEANS_GUI_FONT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/assets/DejaVuSans.ttf")
    target_link_libraries(kmeans_gui PRIVATE Threads::Threads)
endif()
