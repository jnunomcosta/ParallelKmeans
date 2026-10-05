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

int km_init_centroids(const km_dataset *ds, const km_config *cfg, float *out)
{
    switch (cfg->init)
    {
    case KM_INIT_RANDOM:
        return init_random(ds, cfg, out);
    case KM_INIT_PLUSPLUS:
    case KM_INIT_GIVEN:
        return KM_ERR_ARG; /* not implemented yet */
    }
    return KM_ERR_ARG;
}
