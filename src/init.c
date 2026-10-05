#include "assign.h"
#include "rng.h"

#include <stdlib.h>
#include <string.h>

static int init_random(const km_dataset *ds, const km_config *cfg, float *out)
{
    uint32_t *idx = NULL;
    if (ds->n > UINT32_MAX)
        return KM_ERR_ARG;
    idx = malloc(ds->n * sizeof *idx);
    if (!idx)
        return KM_ERR_NOMEM;
    for (size_t i = 0; i < ds->n; i++)
        idx[i] = (uint32_t)i;
    km_rng rng;
    km_rng_seed(&rng, cfg->seed);
    for (size_t j = 0; j < cfg->k; j++)
    {
        size_t pick = j + (size_t)km_rng_below(&rng, ds->n - j);
        uint32_t t = idx[j];
        idx[j] = idx[pick];
        idx[pick] = t;
        memcpy(out + j * ds->dim, ds->points + (size_t)idx[j] * ds->dim, ds->dim * sizeof(float));
    }
    free(idx);
    return KM_OK;
}

static int init_plusplus(const km_dataset *ds, const km_config *cfg, float *out)
{
    size_t n = ds->n, dim = ds->dim;
    double *d2 = malloc(n * sizeof *d2); /* distance to the nearest chosen centroid */
    size_t *chosen = malloc(cfg->k * sizeof *chosen);
    if (!d2 || !chosen)
    {
        free(d2);
        free(chosen);
        return KM_ERR_NOMEM;
    }
    km_rng rng;
    km_rng_seed(&rng, cfg->seed);
    size_t pick = (size_t)km_rng_below(&rng, n);
    for (size_t j = 0;; j++)
    {
        chosen[j] = pick;
        const float *c = ds->points + pick * dim;
        memcpy(out + j * dim, c, dim * sizeof(float));
        if (j + 1 == cfg->k)
            break;

        double total = 0.0;
        for (size_t i = 0; i < n; i++)
        {
            const float *p = ds->points + i * dim;
            double dist = 0.0;
            for (size_t d = 0; d < dim; d++)
            {
                double diff = (double)p[d] - (double)c[d];
                dist += diff * diff;
            }
            d2[i] = j == 0 || dist < d2[i] ? dist : d2[i];
            total += d2[i];
        }

        if (total > 0.0)
        {
            double target = km_rng_uniform(&rng) * total, acc = 0.0;
            pick = n;
            for (size_t i = 0; i < n; i++)
            {
                if (d2[i] <= 0.0)
                    continue;
                pick = i; /* last positive entry wins if rounding overshoots */
                acc += d2[i];
                if (acc > target)
                    break;
            }
        }
        else
        {
            /* All remaining points coincide with a centroid: take the next unused index. */
            pick = 0;
            for (;; pick++)
            {
                bool used = false;
                for (size_t m = 0; m <= j; m++)
                    used |= chosen[m] == pick;
                if (!used)
                    break;
            }
        }
    }
    free(d2);
    free(chosen);
    return KM_OK;
}

int km_init_centroids(const km_dataset *ds, const km_config *cfg, float *out)
{
    switch (cfg->init)
    {
    case KM_INIT_RANDOM:
        return init_random(ds, cfg, out);
    case KM_INIT_PLUSPLUS:
        return init_plusplus(ds, cfg, out);
    case KM_INIT_GIVEN:
        memcpy(out, cfg->initial_centroids, cfg->k * ds->dim * sizeof(float));
        return KM_OK;
    }
    return KM_ERR_ARG;
}
