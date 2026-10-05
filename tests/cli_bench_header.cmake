# Usage: cmake -DCSV=<file> -P cli_bench_header.cmake
set(expected "scaling,impl,dataset,n,dim,k,threads,iters,repeats,median_s,min_s,stddev_s,per_iter_ms,speedup,efficiency,karp_flatt")
file(STRINGS ${CSV} lines)
list(GET lines 0 header)
if(NOT header STREQUAL expected)
    message(FATAL_ERROR "unexpected CSV header: ${header}")
endif()
