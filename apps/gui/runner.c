#include "runner.h"

#include <stdlib.h>
#include <string.h>

void framelist_clear(FrameList *fl)
{
    for (size_t i = 0; i < fl->count; i++)
        free(fl->items[i].centroids);
    fl->count = 0;
}

void framelist_free(FrameList *fl)
{
    framelist_clear(fl);
    free(fl->items);
    memset(fl, 0, sizeof *fl);
}

bool framelist_push(FrameList *fl, Frame f)
{
    if (fl->count == fl->cap)
    {
        size_t cap = fl->cap ? fl->cap * 2 : 64;
        Frame *items = realloc(fl->items, cap * sizeof *items);
        if (!items)
        {
            free(f.centroids);
            return false;
        }
        fl->items = items;
        fl->cap = cap;
    }
    fl->items[fl->count++] = f;
    return true;
}

static float *copy_floats(const float *src, size_t count)
{
    float *p = malloc(count * sizeof *p);
    if (p)
        memcpy(p, src, count * sizeof *p);
    return p;
}

static bool on_step(const km_step *s, void *user)
{
    Runner *r = user;
    float *c = copy_floats(s->centroids, s->k * s->dim);
    Frame f = {c, s->inertia, s->shift, s->seconds};
    pthread_mutex_lock(&r->mu);
    if (!c || !framelist_push(&r->pending, f))
    {
        pthread_mutex_unlock(&r->mu);
        return false;
    }
    r->info.iterations = s->iteration;
    pthread_mutex_unlock(&r->mu);
    return !atomic_load(&r->cancel);
}

static bool on_step_cancel_only(const km_step *s, void *user)
{
    (void)s;
    return !atomic_load(&((Runner *)user)->cancel);
}

static void finish(Runner *r, RunStatus status, int err)
{
    pthread_mutex_lock(&r->mu);
    r->info.status = status;
    r->info.err = err;
    pthread_mutex_unlock(&r->mu);
}

static void *run_job(void *arg)
{
    Runner *r = arg;
    km_result res;
    int err = km_run(&r->ds, &r->cfg, on_step, r, &res);
    if (err != KM_OK)
    {
        finish(r, RUN_ERROR, err);
        return NULL;
    }
    Frame last = {res.centroids, res.inertia, 0.0, 0.0};
    res.centroids = NULL; /* the frame list owns them now */
    pthread_mutex_lock(&r->mu);
    bool ok = framelist_push(&r->pending, last);
    r->info.iterations = res.iterations;
    r->info.converged = res.converged;
    r->info.seconds = res.seconds;
    pthread_mutex_unlock(&r->mu);
    km_result_free(&res);
    if (!ok)
        finish(r, RUN_ERROR, KM_ERR_NOMEM);
    else
        finish(r, atomic_load(&r->cancel) ? RUN_CANCELLED : RUN_DONE, KM_OK);
    return NULL;
}

static void *compare_job(void *arg)
{
    Runner *r = arg;
    km_config c = r->cfg;
    c.tol = -1.0;
    c.max_iter = 20;
    CompareResult cr = {0, 0, c.threads > 0 ? c.threads : km_max_threads()};
    double *out[2] = {&cr.seq_seconds, &cr.omp_seconds};
    km_impl impls[2] = {KM_IMPL_SEQ, KM_IMPL_OMP};
    for (int i = 0; i < 2; i++)
    {
        km_result res;
        c.impl = impls[i];
        int err = km_run(&r->ds, &c, on_step_cancel_only, r, &res);
        if (err != KM_OK)
        {
            finish(r, RUN_ERROR, err);
            return NULL;
        }
        *out[i] = res.seconds;
        km_result_free(&res);
        if (atomic_load(&r->cancel))
        {
            finish(r, RUN_CANCELLED, KM_OK);
            return NULL;
        }
    }
    pthread_mutex_lock(&r->mu);
    r->info.compare = cr;
    r->info.compare_ready = true;
    pthread_mutex_unlock(&r->mu);
    finish(r, RUN_DONE, KM_OK);
    return NULL;
}

void runner_init(Runner *r)
{
    memset(r, 0, sizeof *r);
    pthread_mutex_init(&r->mu, NULL);
    r->mu_ready = true;
    atomic_init(&r->cancel, false);
}

void runner_cancel(Runner *r)
{
    atomic_store(&r->cancel, true);
}

void runner_join(Runner *r)
{
    if (r->started)
    {
        pthread_join(r->thread, NULL);
        r->started = false;
    }
}

static void release_job(Runner *r)
{
    km_dataset_free(&r->ds);
    free(r->initial);
    r->initial = NULL;
    framelist_clear(&r->pending);
}

static int start(Runner *r, const km_dataset *ds, const km_config *cfg, bool compare)
{
    runner_cancel(r);
    runner_join(r);
    release_job(r);

    int err = km_dataset_alloc(&r->ds, ds->n, ds->dim);
    if (err != KM_OK)
        return err;
    memcpy(r->ds.points, ds->points, ds->n * ds->dim * sizeof(float));
    r->cfg = *cfg;
    if (cfg->init == KM_INIT_GIVEN)
    {
        r->initial = copy_floats(cfg->initial_centroids, cfg->k * ds->dim);
        if (!r->initial)
        {
            release_job(r);
            return KM_ERR_NOMEM;
        }
        r->cfg.initial_centroids = r->initial;
    }
    r->compare_job = compare;
    atomic_store(&r->cancel, false);
    memset(&r->info, 0, sizeof r->info);
    r->info.status = RUN_RUNNING;
    r->info.is_compare = compare;
    r->info.impl = cfg->impl;
    r->info.threads = cfg->impl == KM_IMPL_SEQ ? 1
                      : cfg->threads > 0       ? cfg->threads
                                               : km_max_threads();
    if (pthread_create(&r->thread, NULL, compare ? compare_job : run_job, r) != 0)
    {
        r->info.status = RUN_ERROR;
        r->info.err = KM_ERR_NOMEM;
        release_job(r);
        return KM_ERR_NOMEM;
    }
    r->started = true;
    return KM_OK;
}

int runner_start(Runner *r, const km_dataset *ds, const km_config *cfg)
{
    return start(r, ds, cfg, false);
}

int runner_compare(Runner *r, const km_dataset *ds, const km_config *cfg)
{
    return start(r, ds, cfg, true);
}

void runner_poll(Runner *r, FrameList *dst)
{
    pthread_mutex_lock(&r->mu);
    for (size_t i = 0; i < r->pending.count; i++)
        framelist_push(dst, r->pending.items[i]);
    r->pending.count = 0;
    pthread_mutex_unlock(&r->mu);
}

RunInfo runner_info(Runner *r)
{
    pthread_mutex_lock(&r->mu);
    RunInfo info = r->info;
    pthread_mutex_unlock(&r->mu);
    return info;
}

void runner_free(Runner *r)
{
    runner_cancel(r);
    runner_join(r);
    release_job(r);
    framelist_free(&r->pending);
    if (r->mu_ready)
        pthread_mutex_destroy(&r->mu);
    memset(r, 0, sizeof *r);
}
