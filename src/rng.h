#ifndef KMEANS_RNG_H
#define KMEANS_RNG_H

#include <math.h>
#include <stdint.h>

/* xoshiro256** seeded through splitmix64. */
typedef struct km_rng
{
    uint64_t s[4];
} km_rng;

static inline uint64_t km_splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static inline void km_rng_seed(km_rng *r, uint64_t seed)
{
    for (int i = 0; i < 4; i++)
        r->s[i] = km_splitmix64(&seed);
}

static inline uint64_t km_rng_rotl(uint64_t x, int k)
{
    return (x << k) | (x >> (64 - k));
}

static inline uint64_t km_rng_next(km_rng *r)
{
    uint64_t result = km_rng_rotl(r->s[1] * 5, 7) * 9;
    uint64_t t = r->s[1] << 17;
    r->s[2] ^= r->s[0];
    r->s[3] ^= r->s[1];
    r->s[1] ^= r->s[2];
    r->s[0] ^= r->s[3];
    r->s[2] ^= t;
    r->s[3] = km_rng_rotl(r->s[3], 45);
    return result;
}

/* Double in [0,1) from the top 53 bits. */
static inline double km_rng_uniform(km_rng *r)
{
    return (double)(km_rng_next(r) >> 11) * (1.0 / 9007199254740992.0);
}

/* Integer in [0,n), n > 0. Modulo bias is negligible for 64-bit output. */
static inline uint64_t km_rng_below(km_rng *r, uint64_t n)
{
    return km_rng_next(r) % n;
}

/* Standard normal via Box-Muller. */
static inline double km_rng_normal(km_rng *r)
{
    double u1 = 1.0 - km_rng_uniform(r); /* in (0,1] so log is finite */
    double u2 = km_rng_uniform(r);
    return sqrt(-2.0 * log(u1)) * cos(6.283185307179586 * u2);
}

#endif
