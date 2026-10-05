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

    add_test(NAME cli_run_input COMMAND ${KM_CLI} run --input ${CMAKE_CURRENT_BINARY_DIR}/cli_gen.csv
                                        --k 4 --impl seq)
    set_tests_properties(cli_run_input PROPERTIES FIXTURES_REQUIRED cli_gen_csv
                                                  PASS_REGULAR_EXPRESSION "converged")
    add_test(NAME cli_run COMMAND ${KM_CLI} run --n 2e4 --k 4 --impl seq)
    set_tests_properties(cli_run PROPERTIES PASS_REGULAR_EXPRESSION "converged")
    km_cli_exit_test(cli_err_k_zero 2 run --k 0)
    km_cli_exit_test(cli_err_impl_gpu 2 run --impl gpu)
    km_cli_exit_test(cli_err_input_and_dataset 2 run --input x.csv --dataset blobs)

    set(KM_BENCH_CSV ${CMAKE_CURRENT_BINARY_DIR}/b.csv)
    add_test(NAME cli_bench COMMAND ${KM_CLI} bench --n 2e4 --k 4 --threads 1,2 --repeat 1
                                        --warmup 0 --iters 3 --csv ${KM_BENCH_CSV})
    set_tests_properties(cli_bench PROPERTIES FIXTURES_SETUP cli_bench_csv)
    add_test(NAME cli_bench_csv_header
             COMMAND ${CMAKE_COMMAND} -DCSV=${KM_BENCH_CSV} -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_bench_header.cmake)
    set_tests_properties(cli_bench_csv_header PROPERTIES FIXTURES_REQUIRED cli_bench_csv)
    add_test(NAME cli_bench_weak COMMAND ${KM_CLI} bench --scaling weak --n 1e4 --k 4 --threads 1,2
                                             --repeat 1 --warmup 0 --iters 3)
    km_cli_exit_test(cli_err_bench_threads 2 bench --threads 1,,2)
endif()
