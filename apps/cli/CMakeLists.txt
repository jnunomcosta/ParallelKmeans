add_executable(kmeans_cli main.c args.c cmd_gen.c cmd_run.c cmd_bench.c)
set_target_properties(kmeans_cli PROPERTIES OUTPUT_NAME kmeans)
target_link_libraries(kmeans_cli PRIVATE kmeans::kmeans kmeans_warnings m)
target_compile_definitions(kmeans_cli PRIVATE KMEANS_VERSION="${PROJECT_VERSION}"
                                              KMEANS_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
