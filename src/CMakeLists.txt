add_library(kmeans STATIC dataset.c)
add_library(kmeans::kmeans ALIAS kmeans)
target_include_directories(kmeans PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(kmeans PRIVATE kmeans_warnings m)
