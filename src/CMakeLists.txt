find_package(OpenMP REQUIRED)

add_library(kmeans STATIC dataset.c csv.c init.c kmeans.c assign_seq.c assign_omp.c)
add_library(kmeans::kmeans ALIAS kmeans)
target_include_directories(kmeans PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(kmeans PUBLIC OpenMP::OpenMP_C)
target_link_libraries(kmeans PRIVATE kmeans_warnings m)
