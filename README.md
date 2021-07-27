# ParallelKmeans
C implementation of the popular clustering algorithm K-Means, the code was written as an assignment for a Master's class called Parallel Computing, with the objective of using OpenMP to parallelize the algorithm in order to make it faster. Our implemation can become 12 times faster when using various threads compared to the sequential version.
## Sequencial version:
### To compile
`make`
### To run
`make run`

## Parallel version:
### To compile
`make parallel`
### To run
`make parallelrun`

## Parallel Without Critical Section version:
### To compile
`make parallel2`
### To run
`make parallelrun2`
