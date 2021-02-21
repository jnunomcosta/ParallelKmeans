#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

typedef struct Point
{
    double x;
    double y;
} point;

int main(int argc, char **argv)
{
    int max_threads = omp_get_max_threads();
    printf("Maximum number of Threads = %d\n", max_threads);
    srand(69420);
    int stdin_input;

    int k = 2;
    stdin_input = 0;
    printf("Number of Clusters (as an integer bigger than 1):\n");
    scanf("%d", &stdin_input);
    if (stdin_input < 2)
    {
        printf("Invalid number of Clusters, defaulting to 2\n");
    }
    else
    {
        k = stdin_input;
    }

    int n = 10;
    stdin_input = 0;
    printf("Number of Points (as an integer bigger than 9):\n");
    scanf("%d", &stdin_input);
    if (stdin_input < 10)
    {
        printf("Invalid number of Points, defaulting to 10\n");
    }
    else
    {
        n = stdin_input;
    }

    int max_executions = 1;
    stdin_input = 0;
    printf("Number of Executions (as an integer bigger than 0):\n");
    scanf("%d", &stdin_input);
    if (stdin_input < 1)
    {
        printf("Invalid number of Executions, defaulting to 1\n");
    }
    else
    {
        max_executions = stdin_input;
    }

    point *points;
    points = (point *)malloc(sizeof(struct Point) * n);

    for (int i = 0; i < n; i++)
    {
        points[i].x = (double)rand() / (double)(RAND_MAX / 10);
        points[i].y = (double)rand() / (double)(RAND_MAX / 10);
    }

    point centroids[k], original_centroids[k];

    for (int i = 0; i < k; i++)
    {
        centroids[i].x = original_centroids[i].x = (double)rand() / (double)(RAND_MAX / 10);
        centroids[i].y = original_centroids[i].y = (double)rand() / (double)(RAND_MAX / 10);
    }

    point ***clusters;
    clusters = (point ***)malloc(sizeof(point **) * k);
    for (int i = 0; i < k; i++)
    {
        clusters[i] = (point **)malloc(sizeof(point *) * max_threads);
        for (int j = 0; j < max_threads; j++)
        {
            clusters[i][j] = (point *)malloc(sizeof(struct Point) * n);
        }
    }

    double meanExecTime = 0;
    int execution = 0;

    point previous_centroids[k];
    int iterations = 0, changed = 1;

    while (execution < max_executions)
    {

        for (int i = 0; i < k; i++)
        {
            centroids[i].x = original_centroids[i].x;
            centroids[i].y = original_centroids[i].y;
        }

        changed = 1;
        iterations = 0;

        double b4 = omp_get_wtime();
        for (; changed; iterations++)
        {
            int clusters_size[k][max_threads];

            for (int i = 0; i < k; i++)
            {
                for (int j = 0; j < max_threads; j++)
                {
                    clusters_size[i][j] = 0;
                }
            }

            #pragma omp parallel for schedule(static)
            for (int i = 0; i < n; i++)
            {
                int cluster_closest_to = 0;
                double distance, smallest_distance;
                for (int j = 0; j < k; j++)
                {
                    distance = sqrt(powf((centroids[j].x - points[i].x), 2) + powf((centroids[j].y - points[i].y), 2));
                    if (j == 0)
                    {
                        smallest_distance = distance;
                    }
                    else
                    {
                        if (distance < smallest_distance)
                        {
                            smallest_distance = distance;
                            cluster_closest_to = j;
                        }
                    }
                }

                clusters[cluster_closest_to][omp_get_thread_num()][clusters_size[cluster_closest_to][omp_get_thread_num()]++] = points[i];
            }

            int has_changed = 1;

            #pragma omp parallel for schedule(dynamic)
            for (int i = 0; i < k; i++)
            {
                double x = 0, y = 0;
                int cluster_size = 0;
                for (int j = 0; j < max_threads; j++)
                {
                    for (int w = 0; w < clusters_size[i][j]; w++)
                    {
                        x += clusters[i][j][w].x;
                        y += clusters[i][j][w].y;
                        cluster_size++;
                    }
                }

                if (!(x == 0 && y == 0))
                {
                    previous_centroids[i] = centroids[i];
                    centroids[i].x = (double)x / cluster_size;
                    centroids[i].y = (double)y / cluster_size;

                    if (!(((previous_centroids[i].x - 0.00001f) < centroids[i].x && (previous_centroids[i].x + 0.00001f) > centroids[i].x) && 
                          ((previous_centroids[i].y - 0.00001f) < centroids[i].y && (previous_centroids[i].y + 0.00001f) > centroids[i].y)))
                    {
                        has_changed = 0;
                    }
                }
            }

            changed = !has_changed;
        }

        double time_delta = (omp_get_wtime() - b4);
        printf("Time = %f seconds | execution = %d\n", time_delta, execution + 1);

        meanExecTime += time_delta;
        execution++;
    }

    printf("Average time of the %d executions: %f\n", execution, meanExecTime / execution);

    stdin_input = 0;
    printf("If you want to see the results please insert '1'\n");
    scanf("%d", &stdin_input);
    if (stdin_input == 1)
    {
        for (int i = 0; i < k; i++)
        {
            printf("Centroid %d -> (%f,%f)\n", i, centroids[i].x, centroids[i].y);
        }
        printf("The algorithm converged in %d iterations\n", iterations);
    }

    return 0;
}