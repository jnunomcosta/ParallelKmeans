#include "assign.h"

#include <string.h>

double km_assign_seq(const km_dataset *ds, const float *centroids, size_t k, int32_t *labels,
                     double *sums, int64_t *counts)
{
    size_t dim = ds->dim;
    memset(sums, 0, k * dim * sizeof(double));
    memset(counts, 0, k * sizeof(int64_t));
    double inertia = 0.0;
    for (size_t i = 0; i < ds->n; i++)
    {
        const float *p = ds->points + i * dim;
        float d2;
        int32_t c = km_nearest(p, centroids, k, dim, &d2);
        labels[i] = c;
        counts[c]++;
        for (size_t d = 0; d < dim; d++)
            sums[(size_t)c * dim + d] += p[d];
        inertia += d2;
    }
    return inertia;
}
