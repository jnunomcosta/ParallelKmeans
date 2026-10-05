#include "kmeans/kmeans.h"
#include "test.h"

#include <stdlib.h>

/* Two 10x10 grids of points, around (0,0) and (100,100). */
static void two_blobs(km_dataset *ds)
{
    CHECK(km_dataset_alloc(ds, 200, 2) == KM_OK);
    for (size_t b = 0; b < 2; b++)
        for (size_t i = 0; i < 100; i++)
        {
            float *p = ds->points + (b * 100 + i) * 2;
            p[0] = (float)b * 100.0f + (float)(i / 10) * 0.01f - 0.045f;
            p[1] = (float)b * 100.0f + (float)(i % 10) * 0.01f - 0.045f;
        }
}

static km_config base_config(size_t k)
{
    km_config c = km_config_default();
    c.k = k;
    c.init = KM_INIT_RANDOM;
    c.impl = KM_IMPL_SEQ;
    return c;
}

static void test_two_blobs(void)
{
    km_dataset ds = {0};
    two_blobs(&ds);
    km_config cfg = base_config(2);
    km_result r = {0};
    CHECK(km_run(&ds, &cfg, NULL, NULL, &r) == KM_OK);
    CHECK(r.converged);
    int lo = r.centroids[0] < 50.0f ? 0 : 1; /* index of the centroid near the origin */
    CHECK_NEAR(r.centroids[lo * 2], 0.0, 0.05);
    CHECK_NEAR(r.centroids[lo * 2 + 1], 0.0, 0.05);
    CHECK_NEAR(r.centroids[(1 - lo) * 2], 100.0, 0.05);
    CHECK_NEAR(r.centroids[(1 - lo) * 2 + 1], 100.0, 0.05);
    for (size_t i = 0; i < ds.n; i++)
        CHECK(r.labels[i] == (i < 100 ? lo : 1 - lo));
    km_result_free(&r);
    km_dataset_free(&ds);
}

static void test_max_iter(void)
{
    km_gen_params p = km_gen_params_default();
    p.kind = KM_GEN_UNIFORM;
    p.n = 2000;
    km_dataset ds = {0};
    CHECK(km_dataset_generate(&ds, &p) == KM_OK);
    km_config cfg = base_config(5);
    cfg.max_iter = 7;
    cfg.tol = -1;
    km_result r = {0};
    CHECK(km_run(&ds, &cfg, NULL, NULL, &r) == KM_OK);
    CHECK(r.iterations == 7 && !r.converged);
    km_result_free(&r);
    km_dataset_free(&ds);
}

typedef struct
{
    unsigned calls, stop_at;
    int ordered;
} cb_state;

static bool on_step(const km_step *s, void *user)
{
    cb_state *st = user;
    st->calls++;
    if (s->iteration != st->calls)
        st->ordered = 0;
    return !(st->stop_at && s->iteration == st->stop_at);
}

static void test_callback(void)
{
    km_dataset ds = {0};
    two_blobs(&ds);
    km_config cfg = base_config(2);
    cfg.tol = -1;
    cfg.max_iter = 10;
    cb_state st = {0, 0, 1};
    km_result r = {0};
    CHECK(km_run(&ds, &cfg, on_step, &st, &r) == KM_OK);
    CHECK(st.calls == r.iterations && st.calls == 10 && st.ordered);
    km_result_free(&r);

    cb_state st3 = {0, 3, 1};
    CHECK(km_run(&ds, &cfg, on_step, &st3, &r) == KM_OK);
    CHECK(r.iterations == 3 && st3.calls == 3 && st3.ordered);
    km_result_free(&r);
    km_dataset_free(&ds);
}

static void test_k_equals_n(void)
{
    km_dataset ds = {0};
    CHECK(km_dataset_alloc(&ds, 4, 2) == KM_OK);
    float pts[8] = {0, 0, 1, 0, 0, 3, 5, 5};
    for (int i = 0; i < 8; i++)
        ds.points[i] = pts[i];
    km_config cfg = base_config(4);
    km_result r = {0};
    CHECK(km_run(&ds, &cfg, NULL, NULL, &r) == KM_OK);
    CHECK(r.inertia == 0.0);
    km_result_free(&r);
    km_dataset_free(&ds);
}

static void test_invalid(void)
{
    km_dataset ds = {0};
    two_blobs(&ds);
    km_result r = {0};
    km_config cfg;

    cfg = base_config(0);
    CHECK(km_run(&ds, &cfg, NULL, NULL, &r) == KM_ERR_ARG);
    cfg = base_config(ds.n + 1);
    CHECK(km_run(&ds, &cfg, NULL, NULL, &r) == KM_ERR_ARG);
    cfg = base_config(2);
    cfg.max_iter = 0;
    CHECK(km_run(&ds, &cfg, NULL, NULL, &r) == KM_ERR_ARG);
    cfg = base_config(2);
    cfg.init = KM_INIT_GIVEN;
    cfg.initial_centroids = NULL;
    CHECK(km_run(&ds, &cfg, NULL, NULL, &r) == KM_ERR_ARG);
    cfg = base_config(2);
    cfg.threads = -1;
    CHECK(km_run(&ds, &cfg, NULL, NULL, &r) == KM_ERR_ARG);

    km_dataset empty = {0, 2, NULL};
    cfg = base_config(1);
    CHECK(km_run(&empty, &cfg, NULL, NULL, &r) == KM_ERR_ARG);
    km_dataset zero_dim = {5, 0, ds.points};
    CHECK(km_run(&zero_dim, &cfg, NULL, NULL, &r) == KM_ERR_ARG);
    km_dataset_free(&ds);
}

int main(void)
{
    test_two_blobs();
    test_max_iter();
    test_callback();
    test_k_equals_n();
    test_invalid();
    return TEST_MAIN();
}
