if(NOT EMSCRIPTEN)
    find_package(OpenMP REQUIRED)
endif()

add_library(kmeans STATIC dataset.c csv.c init.c kmeans.c assign_seq.c assign_omp.c)
add_library(kmeans::kmeans ALIAS kmeans)
target_include_directories(kmeans PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(EMSCRIPTEN)
    # Without OpenMP the pragmas are ignored and "omp" runs on one thread.
    target_compile_options(kmeans PRIVATE -Wno-unknown-pragmas)
else()
    target_link_libraries(kmeans PUBLIC OpenMP::OpenMP_C)
endif()
target_link_libraries(kmeans PRIVATE kmeans_warnings m)
