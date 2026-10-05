function(km_add_test name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE kmeans kmeans_warnings m)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

km_add_test(test_dataset)

km_add_test(test_csv)
target_compile_definitions(test_csv PRIVATE TEST_TMP_DIR="${CMAKE_CURRENT_BINARY_DIR}")
