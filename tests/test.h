#ifndef KMEANS_TEST_H
#define KMEANS_TEST_H

#include <math.h>
#include <stdio.h>

static int km_test_failures = 0;

#define CHECK(cond)                                                                                \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);               \
            km_test_failures++;                                                                    \
        }                                                                                          \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                                      \
    do                                                                                             \
    {                                                                                              \
        double km_a = (a), km_b = (b);                                                             \
        if (!(fabs(km_a - km_b) <= (eps)))                                                         \
        {                                                                                          \
            fprintf(stderr, "%s:%d: CHECK_NEAR failed: %s=%g vs %s=%g\n", __FILE__, __LINE__, #a,  \
                    km_a, #b, km_b);                                                               \
            km_test_failures++;                                                                    \
        }                                                                                          \
    } while (0)

/* Use as the last statement of main(): return TEST_MAIN(); */
#define TEST_MAIN() (km_test_failures ? 1 : 0)

#endif
