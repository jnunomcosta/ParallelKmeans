#include "kmeans/kmeans.h"
#include "rng.h"

#include <stdlib.h>

km_gen_params km_gen_params_default(void)
{
    km_gen_params p = {KM_GEN_BLOBS, 100000, 2, 8, 0.5f, 1342756};
    return p;
}

int km_dataset_alloc(km_dataset *ds, size_t n, size_t dim)
{
    if (!ds || n == 0 || dim == 0)
        return KM_ERR_ARG;
    if (n > SIZE_MAX / dim / sizeof(float))
        return KM_ERR_NOMEM;
    ds->points = malloc(n * dim * sizeof(float));
    if (!ds->points)
    {
        ds->n = ds->dim = 0;
        return KM_ERR_NOMEM;
    }
    ds->n = n;
    ds->dim = dim;
    return KM_OK;
}

void km_dataset_free(km_dataset *ds)
{
    if (!ds)
        return;
    free(ds->points);
    ds->points = NULL;
    ds->n = ds->dim = 0;
}

static void gen_uniform(km_dataset *ds, km_rng *rng)
{
    for (size_t i = 0; i < ds->n * ds->dim; i++)
        ds->points[i] = (float)(10.0 * km_rng_uniform(rng));
}

static int gen_blobs(km_dataset *ds, const km_gen_params *p, km_rng *rng)
{
    float *centres = malloc(p->centers * ds->dim * sizeof(float));
    if (!centres)
        return KM_ERR_NOMEM;
    for (size_t i = 0; i < p->centers * ds->dim; i++)
        centres[i] = (float)(1.0 + 8.0 * km_rng_uniform(rng));
    for (size_t i = 0; i < ds->n; i++)
    {
        const float *c = centres + km_rng_below(rng, p->centers) * ds->dim;
        for (size_t d = 0; d < ds->dim; d++)
            ds->points[i * ds->dim + d] = c[d] + p->spread * (float)km_rng_normal(rng);
    }
    free(centres);
    return KM_OK;
}

static void gen_rings(km_dataset *ds, const km_gen_params *p, km_rng *rng)
{
    for (size_t i = 0; i < ds->n; i++)
    {
        double radius = 4.0 * (double)(km_rng_below(rng, p->centers) + 1) / (double)p->centers;
        radius += (double)p->spread * km_rng_normal(rng);
        double angle = 6.283185307179586 * km_rng_uniform(rng);
        ds->points[i * 2] = (float)(5.0 + radius * cos(angle));
        ds->points[i * 2 + 1] = (float)(5.0 + radius * sin(angle));
    }
}

int km_dataset_generate(km_dataset *ds, const km_gen_params *p)
{
    if (!ds || !p)
        return KM_ERR_ARG;
    if (p->kind != KM_GEN_UNIFORM && p->centers == 0)
        return KM_ERR_ARG;
    if (p->kind == KM_GEN_RINGS && p->dim != 2)
        return KM_ERR_ARG;
    if (p->kind != KM_GEN_UNIFORM && p->kind != KM_GEN_BLOBS && p->kind != KM_GEN_RINGS)
        return KM_ERR_ARG;
    int err = km_dataset_alloc(ds, p->n, p->dim);
    if (err != KM_OK)
        return err;

    km_rng rng;
    km_rng_seed(&rng, p->seed);
    switch (p->kind)
    {
    case KM_GEN_UNIFORM:
        gen_uniform(ds, &rng);
        break;
    case KM_GEN_BLOBS:
        err = gen_blobs(ds, p, &rng);
        break;
    case KM_GEN_RINGS:
        gen_rings(ds, p, &rng);
        break;
    }
    if (err != KM_OK)
        km_dataset_free(ds);
    return err;
}
