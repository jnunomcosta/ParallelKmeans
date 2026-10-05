#ifndef KMEANS_GUI_RUNNER_H
#define KMEANS_GUI_RUNNER_H

#include "kmeans/kmeans.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

/* One Lloyd iteration (or the final centroids): k*dim centroids plus per-step numbers. */
typedef struct Frame
{
    float *centroids;
    double inertia, shift, seconds;
} Frame;

typedef struct FrameList
{
    Frame *items;
    size_t count, cap;
} FrameList;

void framelist_clear(FrameList *fl);
void framelist_free(FrameList *fl);
/* Takes ownership of f->centroids. Returns false (and frees them) when out of memory. */
bool framelist_push(FrameList *fl, Frame f);

typedef enum
{
    RUN_IDLE,
    RUN_RUNNING,
    RUN_DONE,
    RUN_CANCELLED,
    RUN_ERROR
} RunStatus;

typedef struct CompareResult
{
    double seq_seconds, omp_seconds;
    int threads;
} CompareResult;

/* Snapshot of the runner state, safe to read on the UI thread. */
typedef struct RunInfo
{
    RunStatus status;
    int err;
    bool is_compare;
    unsigned iterations; /* frames received so far while running, final count when done */
    bool converged;
    double seconds; /* sum of per-iteration times */
    km_impl impl;
    int threads; /* threads used by the run (1 for seq) */
    bool compare_ready;
    CompareResult compare;
} RunInfo;

typedef struct Runner
{
    pthread_t thread;
    bool started;
    pthread_mutex_t mu;
    bool mu_ready;

    /* Owned copies for the job; written only while no thread is running. */
    km_dataset ds;
    km_config cfg;
    float *initial;
    bool compare_job;

    atomic_bool cancel;

    /* Guarded by mu. */
    FrameList pending;
    RunInfo info;
} Runner;

void runner_init(Runner *r);
/* Cancels and joins any running job first. Returns a km error code. */
int runner_start(Runner *r, const km_dataset *ds, const km_config *cfg);
/* Same, for the seq-vs-omp comparison (tol = -1, max_iter = 20, no frames). */
int runner_compare(Runner *r, const km_dataset *ds, const km_config *cfg);
/* Appends new frames to dst. Cheap. */
void runner_poll(Runner *r, FrameList *dst);
RunInfo runner_info(Runner *r);
void runner_cancel(Runner *r);
void runner_join(Runner *r);
void runner_free(Runner *r);

#endif
