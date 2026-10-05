#define _POSIX_C_SOURCE 200809L

#include "assign.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

const char *km_strerror(int err)
{
    switch (err)
    {
    case KM_OK:
        return "success";
    case KM_ERR_ARG:
        return "invalid argument";
    case KM_ERR_NOMEM:
        return "out of memory";
    case KM_ERR_IO:
        return "I/O error";
    case KM_ERR_PARSE:
        return "parse error";
    }
    return "unknown error";
}

const char *km_impl_name(km_impl impl)
{
    return impl == KM_IMPL_SEQ ? "seq" : "omp";
}

const char *km_init_name(km_init init)
{
    switch (init)
    {
    case KM_INIT_RANDOM:
        return "random";
    case KM_INIT_PLUSPLUS:
        return "kmeans++";
    case KM_INIT_GIVEN:
        return "given";
    }
    return "unknown";
}

int km_max_threads(void)
{
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

km_config km_config_default(void)
{
    km_config c = {0};
    c.max_iter = 300;
    c.tol = 1e-4;
    c.init = KM_INIT_PLUSPLUS;
    c.seed = 69420;
    c.impl = KM_IMPL_OMP;
    return c;
}

void km_result_free(km_result *r)
{
    if (!r)
        return;
    free(r->centroids);
    free(r->labels);
    memset(r, 0, sizeof *r);
}

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static double assign(const km_config *cfg, const km_dataset *ds, const float *centroids,
                     int32_t *labels, double *sums, int64_t *counts)
{
    switch (cfg->impl)
    {
    case KM_IMPL_SEQ:
        break;
    case KM_IMPL_OMP:
        /* TODO(spec 03) */
        break;
    }
    return km_assign_seq(ds, centroids, cfg->k, labels, sums, counts);
}

int km_run(const km_dataset *ds, const km_config *cfg, km_step_fn on_step, void *user,
           km_result *out)
{
    if (!ds || !cfg || !out || !ds->points)
        return KM_ERR_ARG;
    memset(out, 0, sizeof *out);
    size_t n = ds->n, dim = ds->dim, k = cfg->k;
    if (n == 0 || dim == 0 || k == 0 || k > n || cfg->max_iter == 0 || cfg->threads < 0 ||
        (cfg->init == KM_INIT_GIVEN && !cfg->initial_centroids))
        return KM_ERR_ARG;

    float *cur = malloc(k * dim * sizeof(float));
    float *next = malloc(k * dim * sizeof(float));
    int32_t *labels = malloc(n * sizeof(int32_t));
    double *sums = malloc(k * dim * sizeof(double));
    int64_t *counts = malloc(k * sizeof(int64_t));
    int err = KM_OK;
    if (!cur || !next || !labels || !sums || !counts)
        err = KM_ERR_NOMEM;
    if (err == KM_OK)
        err = km_init_centroids(ds, cfg, cur);

    double total = 0.0, inertia = 0.0;
    unsigned it = 0;
    bool converged = false;
    while (err == KM_OK && it < cfg->max_iter)
    {
        double t0 = now_seconds();
        inertia = assign(cfg, ds, cur, labels, sums, counts);
        double shift2 = 0.0;
        for (size_t j = 0; j < k; j++)
        {
            double s2 = 0.0;
            for (size_t d = 0; d < dim; d++)
            {
                float old = cur[j * dim + d];
                float nv = counts[j] > 0 ? (float)(sums[j * dim + d] / (double)counts[j]) : old;
                next[j * dim + d] = nv;
                double diff = (double)nv - (double)old;
                s2 += diff * diff;
            }
            if (s2 > shift2)
                shift2 = s2;
        }
        double shift = sqrt(shift2); /* real sqrt, once per iteration */
        double dt = now_seconds() - t0;
        total += dt;
        it++;

        bool keep_going = true;
        if (on_step)
        {
            km_step step = {it, n, k, dim, cur, labels, inertia, shift, dt};
            keep_going = on_step(&step, user);
        }
        float *tmp = cur;
        cur = next;
        next = tmp;
        if (cfg->tol >= 0 && shift <= cfg->tol)
        {
            converged = true;
            break;
        }
        if (!keep_going)
            break;
    }

    free(next);
    free(sums);
    free(counts);
    if (err != KM_OK)
    {
        free(cur);
        free(labels);
        return err;
    }
    out->centroids = cur;
    out->labels = labels;
    out->iterations = it;
    out->converged = converged;
    out->inertia = inertia;
    out->seconds = total;
    return KM_OK;
}
