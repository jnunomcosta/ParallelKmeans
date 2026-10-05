#include "kmeans/kmeans.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    if (f)
    {
        fputs(text, f);
        fclose(f);
    }
}

int main(void)
{
    const char *path = TEST_TMP_DIR "/test.csv";
    km_dataset ds = {0}, back = {0};

    km_gen_params p = km_gen_params_default();
    p.n = 500;
    p.dim = 3;
    CHECK(km_dataset_generate(&ds, &p) == KM_OK);
    CHECK(km_dataset_save_csv(&ds, path) == KM_OK);
    CHECK(km_dataset_load_csv(&back, path) == KM_OK);
    CHECK(back.n == ds.n && back.dim == ds.dim);
    if (back.n == ds.n && back.dim == ds.dim)
        CHECK(memcmp(ds.points, back.points, ds.n * ds.dim * sizeof(float)) == 0);
    km_dataset_free(&ds);
    km_dataset_free(&back);

    write_file(path, "x,y\n1,2\n\n3.5,4\n");
    CHECK(km_dataset_load_csv(&back, path) == KM_OK);
    CHECK(back.n == 2 && back.dim == 2);
    if (back.n == 2 && back.dim == 2)
    {
        CHECK(back.points[0] == 1.0f && back.points[3] == 4.0f);
        CHECK(back.points[2] == 3.5f);
    }
    km_dataset_free(&back);

    write_file(path, "1,2\n3,4,5\n");
    CHECK(km_dataset_load_csv(&back, path) == KM_ERR_PARSE);
    write_file(path, "1,2\n3,abc\n");
    CHECK(km_dataset_load_csv(&back, path) == KM_ERR_PARSE);

    CHECK(km_dataset_load_csv(&back, TEST_TMP_DIR "/does-not-exist.csv") == KM_ERR_IO);

    return TEST_MAIN();
}
