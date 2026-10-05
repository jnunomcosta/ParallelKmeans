# ParallelKmeans
C implementation of the popular clustering algorithm K-Means, the code was written as an assignment for a Master's class called Parallel Computing, with the objective of using OpenMP to parallelize the algorithm in order to make it faster. Our implemation can become 12 times faster when using various threads compared to the sequential version.
Only fully tested on gcc.

## Build and run
```
cmake --preset release
cmake --build --preset release -j
./build/release/apps/cli/kmeans --help
```
