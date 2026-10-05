#include "assign.h"

#include <stdlib.h>
#include <string.h>

#ifdef _OPENMP
#include <omp.h>
#endif

int km_omp_ws_init(km_omp_ws *ws, int threads, size_t k, size_t dim)
{
    memset(ws, 0, sizeof *ws);
    size_t bytes = k * dim * sizeof(double) + k * sizeof(int64_t);
    /* Each thread block is padded to whole 64-byte cache lines: no false sharing. */
    ws->stride = (bytes + 63) / 64 * 64;
    ws->threads = threads;
    ws->mem = aligned_alloc(64, (size_t)threads * ws->stride);
    return ws->mem ? KM_OK : KM_ERR_NOMEM;
}

void km_omp_ws_free(km_omp_ws *ws)
{
    free(ws->mem);
    memset(ws, 0, sizeof *ws);
}

double km_assign_omp(const km_dataset *ds, const float *centroids, size_t k, int32_t *labels,
                     double *sums, int64_t *counts, km_omp_ws *ws)
{
    size_t n = ds->n, dim = ds->dim;
    int used = 1;
    /* Inertia's reduction order is unspecified; it is only reported, never used for control. */
    double inertia = 0.0;

#pragma omp parallel num_threads(ws->threads) reduction(+ : inertia)
    {
        int t = 0;
#ifdef _OPENMP
        t = omp_get_thread_num();
#pragma omp single
        used = omp_get_num_threads();
#endif
        unsigned char *block = ws->mem + (size_t)t * ws->stride;
        double *ts = (double *)block;
        int64_t *tc = (int64_t *)(block + k * dim * sizeof(double));
        memset(ts, 0, k * dim * sizeof(double));
        memset(tc, 0, k * sizeof(int64_t));

#pragma omp for schedule(static)
        for (size_t i = 0; i < n; i++)
        {
            const float *p = ds->points + i * dim;
            float d2;
            int32_t c = km_nearest(p, centroids, k, dim, &d2);
            labels[i] = c;
            tc[c]++;
            for (size_t d = 0; d < dim; d++)
                ts[(size_t)c * dim + d] += p[d];
            inertia += d2;
        }
    }

    /* Serial merge in thread-index order: deterministic for a fixed team size. */
    memset(sums, 0, k * dim * sizeof(double));
    memset(counts, 0, k * sizeof(int64_t));
    for (int t = 0; t < used; t++)
    {
        unsigned char *block = ws->mem + (size_t)t * ws->stride;
        const double *ts = (const double *)block;
        const int64_t *tc = (const int64_t *)(block + k * dim * sizeof(double));
        for (size_t x = 0; x < k * dim; x++)
            sums[x] += ts[x];
        for (size_t j = 0; j < k; j++)
            counts[j] += tc[j];
    }
    return inertia;
}
