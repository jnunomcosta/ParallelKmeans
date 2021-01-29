#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <omp.h>

typedef struct Point
{
    double x;
    double y;
} point;

int main(int argc, char **argv)
{
    printf("Num threads = %d ; Max threads = %d\n",omp_get_num_threads(),omp_get_max_threads());
    srand(69420);
    int k = 20,n=1000000,max_executions=8;

    point* points;
    points = (point*) malloc(sizeof(struct Point) * n);

    for (int i = 0; i < n; i++)
    {
        points[i].x = (double)rand() / (double)(RAND_MAX / 10);
        points[i].y = (double)rand() / (double)(RAND_MAX / 10);
    }

    point centroids[k], centroid_original[k];

    for (int i = 0; i < k; i++)
    {
        centroids[i].x = centroid_original[i].x = (double)rand() / (double)(RAND_MAX / 10);
        centroids[i].y = centroid_original[i].y = (double)rand() / (double)(RAND_MAX / 10);
    }


    point** clusters;
    clusters = (point**) malloc(sizeof(point*) * k);
    for(int i=0;i<k;i++){
        clusters[i] = (point*) malloc(sizeof(struct Point) * n);
    }

    double meanExecTime = 0;
    int execution = 0;

    while(execution<max_executions) {

        for(int tryhard=0;tryhard<k;tryhard++){
            centroids[tryhard].x=centroid_original[tryhard].x;
            centroids[tryhard].y=centroid_original[tryhard].y;
        }

        point centroids_anteriores[k];
        int iteracoes = 0, mudou = 1;

        double b4 = omp_get_wtime();
        for (; mudou; iteracoes++) {
            int clusters_size[k];
            for (int i = 0; i < k; i++) {
                clusters_size[i] = 0;
            }

            #pragma omp parallel for schedule(static)
            for (int i = 0; i < n; i++) {
                int cluster_closest_to = 0, onde_escrever;
                double distance, smallest_distance;
                for (int j = 0; j < k; j++) {
                    distance = sqrt(powf((centroids[j].x - points[i].x), 2) + powf((centroids[j].y - points[i].y), 2));
                    if (j == 0) {
                        smallest_distance = distance;
                    } else {
                        if (distance < smallest_distance) {
                            smallest_distance = distance;
                            cluster_closest_to = j;
                        }
                    }
                }
                #pragma omp critical
                onde_escrever = clusters_size[cluster_closest_to]++;

                clusters[cluster_closest_to][onde_escrever] = points[i];
            }

            int mudou_ou_nao = 1;

            #pragma omp parallel for schedule(dynamic)
            for (int i = 0; i < k; i++) {
                double x = 0, y = 0;
                for (int j = 0; j < clusters_size[i]; j++) {
                    x += clusters[i][j].x;
                    y += clusters[i][j].y;
                }
                if (!(x == 0 && y == 0)) {
                    centroids_anteriores[i] = centroids[i];
                    centroids[i].x = (double) x / clusters_size[i];
                    centroids[i].y = (double) y / clusters_size[i];

                    if (!(((centroids_anteriores[i].x - 0.00001f) < centroids[i].x &&
                           (centroids_anteriores[i].x + 0.00001f) > centroids[i].x)
                          &&
                          ((centroids_anteriores[i].y - 0.00001f) < centroids[i].y &&
                           (centroids_anteriores[i].y + 0.00001f) > centroids[i].y))) {
                        mudou_ou_nao = 0;
                    }
                }
            }

            mudou = !mudou_ou_nao;
        }

        double auxiasda = (omp_get_wtime()-b4);
        printf("Media = %f segundos -> iteracao = %d\n",auxiasda,execution);

        meanExecTime += auxiasda;
        execution++;

    }

    printf("%d execucoes\nMedia = %f segundos\n",execution,meanExecTime/execution);

    for(int i=0;i<k;i++){
        printf("Centroide %d -> (%f,%f)\n",i,centroids[i].x,centroids[i].y);
    }

    return 0;
}