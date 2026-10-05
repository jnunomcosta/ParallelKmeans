#include "kmeans/kmeans.h"
#include "test.h"

#include <string.h>

static int gen(km_dataset *ds, km_gen_kind kind, size_t dim, uint64_t seed)
{
    km_gen_params p = km_gen_params_default();
    p.kind = kind;
    p.n = 1000;
    p.dim = dim;
    p.seed = seed;
    return km_dataset_generate(ds, &p);
}

static void test_deterministic(km_gen_kind kind)
{
    km_dataset a = {0}, b = {0}, c = {0};
    CHECK(gen(&a, kind, 2, 1) == KM_OK);
    CHECK(gen(&b, kind, 2, 1) == KM_OK);
    CHECK(gen(&c, kind, 2, 2) == KM_OK);
    size_t bytes = a.n * a.dim * sizeof(float);
    CHECK(memcmp(a.points, b.points, bytes) == 0);
    CHECK(memcmp(a.points, c.points, bytes) != 0);
    km_dataset_free(&a);
    km_dataset_free(&b);
    km_dataset_free(&c);
}

int main(void)
{
    test_deterministic(KM_GEN_UNIFORM);
    test_deterministic(KM_GEN_BLOBS);
    test_deterministic(KM_GEN_RINGS);

    km_dataset ds = {0};
    CHECK(gen(&ds, KM_GEN_UNIFORM, 3, 7) == KM_OK);
    CHECK(ds.n == 1000 && ds.dim == 3);
    for (size_t i = 0; i < ds.n * ds.dim; i++)
        CHECK(ds.points[i] >= 0.0f && ds.points[i] < 10.0f);
    km_dataset_free(&ds);
    CHECK(ds.points == NULL && ds.n == 0);

    CHECK(gen(&ds, KM_GEN_RINGS, 3, 7) == KM_ERR_ARG);

    CHECK(km_dataset_alloc(&ds, 0, 2) == KM_ERR_ARG);
    CHECK(km_dataset_alloc(&ds, 2, 0) == KM_ERR_ARG);

    return TEST_MAIN();
}
