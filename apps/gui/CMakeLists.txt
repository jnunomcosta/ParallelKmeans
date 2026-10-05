include(FetchContent)

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

find_package(Threads REQUIRED)

add_executable(kmeans_gui main.c runner.c view.c palette.c)
set_target_properties(kmeans_gui PROPERTIES OUTPUT_NAME kmeans-gui)
target_link_libraries(kmeans_gui PRIVATE kmeans::kmeans raylib Threads::Threads m)
target_link_libraries(kmeans_gui PRIVATE kmeans_warnings)
