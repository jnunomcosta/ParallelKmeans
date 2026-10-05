#ifndef KMEANS_KMEANS_H
#define KMEANS_KMEANS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Error codes: 0 is success, negatives are errors. */
enum
{
    KM_OK = 0,
    KM_ERR_ARG = -1,
    KM_ERR_NOMEM = -2,
    KM_ERR_IO = -3,
    KM_ERR_PARSE = -4
};

/* n points of dim floats each, row-major. */
typedef struct km_dataset
{
    size_t n, dim;
    float *points;
} km_dataset;

typedef enum
{
    KM_GEN_UNIFORM,
    KM_GEN_BLOBS,
    KM_GEN_RINGS
} km_gen_kind;
typedef struct km_gen_params
{
    km_gen_kind kind;
    size_t n, dim;
    size_t centers; /* blobs: number of blobs; rings: number of rings */
    float spread;   /* blobs: stddev; rings: radial noise stddev */
    uint64_t seed;
} km_gen_params;
/* blobs, n=100000, dim=2, centers=8, spread=0.5, seed=69420 */
km_gen_params km_gen_params_default(void);

int km_dataset_alloc(km_dataset *ds, size_t n, size_t dim);
void km_dataset_free(km_dataset *ds);
int km_dataset_generate(km_dataset *ds, const km_gen_params *p);
int km_dataset_load_csv(km_dataset *ds, const char *path);
int km_dataset_save_csv(const km_dataset *ds, const char *path);

#endif
