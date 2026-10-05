#include "kmeans/kmeans.h"
#include "test.h"

#include <string.h>

static void gen_blobs(km_dataset *ds, size_t n, size_t dim)
{
    km_gen_params p = km_gen_params_default();
    p.kind = KM_GEN_BLOBS;
    p.n = n;
    p.dim = dim;
    CHECK(km_dataset_generate(ds, &p) == KM_OK);
}

static km_config cfg_for(km_impl impl, size_t k, int threads)
{
    km_config c = km_config_default();
    c.k = k;
    c.tol = -1;
    c.max_iter = 20;
    c.impl = impl;
    c.threads = threads;
    return c;
}

static void test_equivalence(size_t n, size_t dim, size_t k, int threads)
{
    km_dataset ds = {0};
    gen_blobs(&ds, n, dim);
    km_config cs = cfg_for(KM_IMPL_SEQ, k, 0), co = cfg_for(KM_IMPL_OMP, k, threads);
    km_result rs = {0}, ro = {0};
    CHECK(km_run(&ds, &cs, NULL, NULL, &rs) == KM_OK);
    CHECK(km_run(&ds, &co, NULL, NULL, &ro) == KM_OK);
    CHECK(memcmp(rs.labels, ro.labels, n * sizeof(int32_t)) == 0);
    for (size_t i = 0; i < k * dim; i++)
        CHECK_NEAR(rs.centroids[i], ro.centroids[i], 1e-4);
    CHECK_NEAR(rs.inertia, ro.inertia, 1e-9 * rs.inertia);
    km_result_free(&rs);
    km_result_free(&ro);
    km_dataset_free(&ds);
}

static void test_determinism(void)
{
    km_dataset ds = {0};
    gen_blobs(&ds, 20001, 3);
    km_config co = cfg_for(KM_IMPL_OMP, 5, 4);
    km_result a = {0}, b = {0};
    CHECK(km_run(&ds, &co, NULL, NULL, &a) == KM_OK);
    CHECK(km_run(&ds, &co, NULL, NULL, &b) == KM_OK);
    CHECK(memcmp(a.centroids, b.centroids, 5 * 3 * sizeof(float)) == 0);
    CHECK(memcmp(a.labels, b.labels, ds.n * sizeof(int32_t)) == 0);
    km_result_free(&a);
    km_result_free(&b);
    km_dataset_free(&ds);
}

static void test_more_threads_than_points(void)
{
    km_dataset ds = {0};
    gen_blobs(&ds, 5, 2);
    km_config cs = cfg_for(KM_IMPL_SEQ, 2, 0), co = cfg_for(KM_IMPL_OMP, 2, 8);
    km_result rs = {0}, ro = {0};
    CHECK(km_run(&ds, &cs, NULL, NULL, &rs) == KM_OK);
    CHECK(km_run(&ds, &co, NULL, NULL, &ro) == KM_OK);
    CHECK(memcmp(rs.labels, ro.labels, 5 * sizeof(int32_t)) == 0);
    for (size_t i = 0; i < 4; i++)
        CHECK_NEAR(rs.centroids[i], ro.centroids[i], 1e-4);
    km_result_free(&rs);
    km_result_free(&ro);
    km_dataset_free(&ds);
}

static void test_empty_cluster(void)
{
    km_dataset ds = {0};
    CHECK(km_dataset_alloc(&ds, 300, 2) == KM_OK);
    float pts[6] = {0, 0, 1, 0, 0, 1};
    for (size_t i = 0; i < ds.n; i++)
        for (size_t d = 0; d < 2; d++)
            ds.points[i * 2 + d] = pts[(i % 3) * 2 + d];
    float init[8] = {0, 0, 1, 0, 0, 1, 1000, 1000};
    km_config cfg = cfg_for(KM_IMPL_OMP, 4, 3);
    cfg.init = KM_INIT_GIVEN;
    cfg.initial_centroids = init;
    km_result r = {0};
    CHECK(km_run(&ds, &cfg, NULL, NULL, &r) == KM_OK);
    CHECK(r.centroids[6] == 1000.0f && r.centroids[7] == 1000.0f);
    for (size_t i = 0; i < 8; i++)
        CHECK(!isnan(r.centroids[i]));
    km_result_free(&r);
    km_dataset_free(&ds);
}

static void test_default_threads(void)
{
    km_dataset ds = {0};
    gen_blobs(&ds, 1000, 2);
    km_config co = cfg_for(KM_IMPL_OMP, 4, 0);
    km_result r = {0};
    CHECK(km_run(&ds, &co, NULL, NULL, &r) == KM_OK);
    km_result_free(&r);
    km_dataset_free(&ds);
}

int main(void)
{
    static const int ts[] = {1, 2, 3, 8};
    static const size_t dims[] = {2, 7}, ks[] = {3, 8};
    for (size_t a = 0; a < 2; a++)
        for (size_t b = 0; b < 2; b++)
            for (size_t c = 0; c < 4; c++)
                test_equivalence(50000, dims[a], ks[b], ts[c]);
    test_equivalence(50001, 2, 8, 8); /* n % T != 0 */
    test_determinism();
    test_more_threads_than_points();
    test_empty_cluster();
    test_default_threads();
    return TEST_MAIN();
}
