function(km_add_test name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE kmeans kmeans_warnings m)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

km_add_test(test_dataset)

km_add_test(test_csv)
target_compile_definitions(test_csv PRIVATE TEST_TMP_DIR="${CMAKE_CURRENT_BINARY_DIR}")
km_add_test(test_kmeans)
km_add_test(test_init)
target_include_directories(test_init PRIVATE ${PROJECT_SOURCE_DIR}/src)

km_add_test(test_parallel)

if(TARGET kmeans_cli)
    set(KM_CLI $<TARGET_FILE:kmeans_cli>)
    # Runs the CLI and checks its exact exit code.
    function(km_cli_exit_test name expect)
        add_test(NAME ${name}
                 COMMAND ${CMAKE_COMMAND} -DCMD=${KM_CLI} "-DARGS=${ARGN}" -DEXPECT=${expect} -P
                         ${CMAKE_CURRENT_SOURCE_DIR}/cli_exit.cmake)
    endfunction()

    add_test(NAME cli_help COMMAND ${KM_CLI} --help)
    add_test(NAME cli_version COMMAND ${KM_CLI} --version)
    set_tests_properties(cli_version PROPERTIES PASS_REGULAR_EXPRESSION "^2\\.0\\.0")
    km_cli_exit_test(cli_unknown_command 2 frobnicate)

    # gen writes a file that run can read back (run is added with the run subcommand).
    add_test(NAME cli_gen COMMAND ${KM_CLI} gen --n 2e4 --seed 7 -o ${CMAKE_CURRENT_BINARY_DIR}/cli_gen.csv)
    set_tests_properties(cli_gen PROPERTIES FIXTURES_SETUP cli_gen_csv PASS_REGULAR_EXPRESSION
                                            "wrote 20000 points \\(dim 2\\)")
    km_cli_exit_test(cli_err_n_fraction 2 gen --n 1.5 -o x.csv)
    km_cli_exit_test(cli_err_dataset_enum 2 gen --dataset gpu -o x.csv)
    km_cli_exit_test(cli_err_gen_no_output 2 gen)
endif()
