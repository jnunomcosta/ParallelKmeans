ourKmeans: ourKmeans.c
		gcc ourKmeans.c -o masterpiece -lm -fopenmp

parallel: ourParallelKmeans.c
		gcc ourParallelKmeans.c -o masterpiece2point0 -lm -fopenmp

parallel2: ourParallelKmeans2.c
		gcc ourParallelKmeans2.c -o masterpiece3point0 -lm -fopenmp

clean:
		rm masterpiece masterpiece2point0 masterpiece3point0

run: masterpiece
		./masterpiece

parallelrun: masterpiece2point0
		./masterpiece2point0

parallelrun2: masterpiece3point0
		./masterpiece3point0