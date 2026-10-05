#ifndef KMEANS_ASSIGN_H
#define KMEANS_ASSIGN_H

#include "kmeans/kmeans.h"

/* Nearest centroid by squared Euclidean distance; ties go to the lowest index. */
static inline int32_t km_nearest(const float *p, const float *c, size_t k, size_t dim,
                                 float *best_d2)
{
    int32_t best = 0;
    float bd = 0.0f;
    for (size_t j = 0; j < k; j++)
    {
        float d2 = 0.0f;
        for (size_t d = 0; d < dim; d++)
        {
            float diff = p[d] - c[j * dim + d];
            d2 += diff * diff;
        }
        if (j == 0 || d2 < bd)
        {
            bd = d2;
            best = (int32_t)j;
        }
    }
    *best_d2 = bd;
    return best;
}

/* Zeroes and fills sums[k*dim] and counts[k]; writes labels[n]; returns inertia. */
double km_assign_seq(const km_dataset *ds, const float *centroids, size_t k, int32_t *labels,
                     double *sums, int64_t *counts);

/* Fills out[k*dim] with the starting centroids chosen by cfg->init. */
int km_init_centroids(const km_dataset *ds, const km_config *cfg, float *out);

#endif
