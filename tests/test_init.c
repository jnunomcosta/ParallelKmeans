#include "assign.h"
#include "kmeans/kmeans.h"
#include "test.h"

#include <string.h>

/* nb grids of 25 points each, centred at (100*b, 100*b). All points are distinct. */
static void blobs(km_dataset *ds, size_t nb)
{
    CHECK(km_dataset_alloc(ds, nb * 25, 2) == KM_OK);
    for (size_t b = 0; b < nb; b++)
        for (size_t i = 0; i < 25; i++)
        {
            float *p = ds->points + (b * 25 + i) * 2;
            p[0] = (float)b * 100.0f + (float)(i / 5) * 0.01f;
            p[1] = (float)b * 100.0f + (float)(i % 5) * 0.01f;
        }
}

static bool same_point(const float *a, const float *b, size_t dim)
{
    return memcmp(a, b, dim * sizeof(float)) == 0;
}

static void test_random(void)
{
    km_dataset ds = {0};
    blobs(&ds, 3);
    km_config cfg = km_config_default();
    cfg.k = 10;
    cfg.init = KM_INIT_RANDOM;
    float c[20];
    CHECK(km_init_centroids(&ds, &cfg, c) == KM_OK);
    for (size_t j = 0; j < cfg.k; j++)
    {
        bool found = false;
        for (size_t i = 0; i < ds.n; i++)
            found |= same_point(c + j * 2, ds.points + i * 2, 2);
        CHECK(found);
        for (size_t m = 0; m < j; m++)
            CHECK(!same_point(c + j * 2, c + m * 2, 2));
    }
    km_dataset_free(&ds);
}

static void test_plusplus(void)
{
    km_dataset ds = {0};
    blobs(&ds, 3);
    km_config cfg = km_config_default();
    cfg.k = 3;
    cfg.init = KM_INIT_PLUSPLUS;
    for (uint64_t seed = 1; seed <= 20; seed++)
    {
        cfg.seed = seed;
        float a[6], b[6];
        CHECK(km_init_centroids(&ds, &cfg, a) == KM_OK);
        CHECK(km_init_centroids(&ds, &cfg, b) == KM_OK);
        CHECK(memcmp(a, b, sizeof a) == 0);
        int seen[3] = {0};
        for (size_t j = 0; j < 3; j++)
            seen[(int)(a[j * 2] / 100.0f + 0.5f)]++;
        CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1);
    }
    km_dataset_free(&ds);
}

static void test_identical(void)
{
    km_dataset ds = {0};
    CHECK(km_dataset_alloc(&ds, 20, 2) == KM_OK);
    for (size_t i = 0; i < 40; i++)
        ds.points[i] = 3.0f;
    km_config cfg = km_config_default();
    cfg.k = 5;
    cfg.init = KM_INIT_PLUSPLUS;
    float c[10];
    CHECK(km_init_centroids(&ds, &cfg, c) == KM_OK);
    for (size_t i = 0; i < 10; i++)
        CHECK(c[i] == 3.0f);
    km_dataset_free(&ds);
}

static void test_given(void)
{
    km_dataset ds = {0};
    blobs(&ds, 2);
    float init[4] = {1, 2, 3, 4}, c[4];
    km_config cfg = km_config_default();
    cfg.k = 2;
    cfg.init = KM_INIT_GIVEN;
    cfg.initial_centroids = init;
    CHECK(km_init_centroids(&ds, &cfg, c) == KM_OK);
    CHECK(memcmp(init, c, sizeof c) == 0);
    km_dataset_free(&ds);
}

int main(void)
{
    test_random();
    test_plusplus();
    test_identical();
    test_given();
    return TEST_MAIN();
}
