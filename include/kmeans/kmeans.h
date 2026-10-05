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

const char *km_strerror(int err);

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

typedef enum
{
    KM_IMPL_SEQ,
    KM_IMPL_OMP
} km_impl;
typedef enum
{
    KM_INIT_RANDOM,
    KM_INIT_PLUSPLUS,
    KM_INIT_GIVEN
} km_init;

typedef struct km_config
{
    size_t k;
    unsigned max_iter; /* default 300 */
    double tol;        /* converged when max centroid shift <= tol; < 0 disables. default 1e-4 */
    km_init init;      /* default KM_INIT_PLUSPLUS */
    const float *initial_centroids; /* k*dim floats, required iff init == KM_INIT_GIVEN */
    uint64_t seed;                  /* default 69420 */
    km_impl impl;                   /* default KM_IMPL_OMP */
    int threads;                    /* omp only; 0 = OpenMP default */
} km_config;
km_config km_config_default(void);

/* One Lloyd iteration, reported to the step callback. */
typedef struct km_step
{
    unsigned iteration; /* 1-based */
    size_t n, k, dim;
    const float *centroids; /* centroids used for this iteration's assignment */
    const int32_t *labels;  /* labels produced by this assignment */
    double inertia;         /* sum of squared distances for this assignment */
    double shift;           /* max centroid movement in the update that followed */
    double seconds;         /* wall time of assignment + update (callback excluded) */
} km_step;
/* Return false to stop early. Pointers are only valid during the call. */
typedef bool (*km_step_fn)(const km_step *step, void *user);

typedef struct km_result
{
    float *centroids; /* k*dim, after the last update */
    int32_t *labels;  /* n, from the last assignment */
    unsigned iterations;
    bool converged;
    double inertia; /* from the last assignment */
    double seconds; /* sum of per-iteration times; excludes init and callbacks */
} km_result;

int km_run(const km_dataset *ds, const km_config *cfg, km_step_fn on_step, void *user,
           km_result *out);
void km_result_free(km_result *r);

const char *km_impl_name(km_impl impl); /* "seq", "omp" */
const char *km_init_name(km_init init); /* "random", "kmeans++", "given" */
int km_max_threads(void);               /* omp_get_max_threads(), or 1 without OpenMP */

#endif
