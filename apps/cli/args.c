#define _POSIX_C_SOURCE 200809L

#include "args.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const ARGS_DATASET_NAMES[] = {"uniform", "blobs", "rings"};
const char *const ARGS_IMPL_NAMES[] = {"seq", "omp"};
const char *const ARGS_INIT_NAMES[] = {"random", "kmeans++"};

const char *args_dataset_name(km_gen_kind kind)
{
    return ARGS_DATASET_NAMES[kind];
}

int args_fail(const char *cmd, const char *fmt, ...)
{
    fprintf(stderr, "kmeans %s: ", cmd);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\nTry 'kmeans %s --help'\n", cmd);
    return 2;
}

int args_bad_option(const char *cmd, int c, char **argv)
{
    if (c == '?' && optopt > 0 && optopt < 256)
        return args_fail(cmd, "unknown option '-%c'", optopt);
    const char *what = argv[optind - 1];
    if (c == '?')
        return args_fail(cmd, "unknown option '%s'", what);
    return args_fail(cmd, "option '%s' requires a value", what);
}

int args_uint(const char *cmd, const char *opt, const char *s, uint64_t max, uint64_t *out)
{
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    if (end == s || *end != '\0' || isspace((unsigned char)*s) || !isfinite(v) || v < 0 ||
        v != floor(v) || v >= 18446744073709551616.0 || v > (double)max)
    {
        args_fail(cmd, "--%s: invalid value '%s' (expected a non-negative integer, at most %llu)",
                  opt, s, (unsigned long long)max);
        return -1;
    }
    *out = (uint64_t)v;
    return 0;
}

int args_double(const char *cmd, const char *opt, const char *s, double *out)
{
    char *end;
    double v = strtod(s, &end);
    if (end == s || *end != '\0' || isspace((unsigned char)*s) || !isfinite(v))
    {
        args_fail(cmd, "--%s: invalid value '%s' (expected a finite number)", opt, s);
        return -1;
    }
    *out = v;
    return 0;
}

int args_list(const char *cmd, const char *opt, const char *s, uint64_t max, size_t *out,
              size_t *count)
{
    size_t n = 0;
    const char *p = s;
    for (;;)
    {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        char item[64];
        if (len == 0 || len >= sizeof item)
        {
            args_fail(cmd, "--%s: invalid list '%s' (empty or overlong item)", opt, s);
            return -1;
        }
        if (n == ARGS_MAX_LIST)
        {
            args_fail(cmd, "--%s: too many items (at most %d)", opt, ARGS_MAX_LIST);
            return -1;
        }
        memcpy(item, p, len);
        item[len] = '\0';
        uint64_t v;
        if (args_uint(cmd, opt, item, max, &v) != 0)
            return -1;
        out[n++] = (size_t)v;
        if (!comma)
            break;
        p = comma + 1;
    }
    *count = n;
    return 0;
}

int args_enum(const char *cmd, const char *opt, const char *s, const char *const *names, int count,
              int *out)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(s, names[i]) == 0)
        {
            *out = i;
            return 0;
        }
    }
    args_fail(cmd, "--%s: invalid value '%s' (expected one of:", opt, s);
    for (int i = 0; i < count; i++)
        fprintf(stderr, " %s", names[i]);
    fprintf(stderr, ")\n");
    return -1;
}

int args_dataset_opt(const char *cmd, int opt, const char *arg, ds_opts *o)
{
    uint64_t u;
    double d;
    int e;
    switch (opt)
    {
    case OPT_DATASET:
        if (args_enum(cmd, "dataset", arg, ARGS_DATASET_NAMES, 3, &e) != 0)
            return -1;
        o->gen.kind = (km_gen_kind)e;
        break;
    case OPT_N:
        if (args_uint(cmd, "n", arg, SIZE_MAX, &u) != 0)
            return -1;
        o->gen.n = (size_t)u;
        break;
    case OPT_DIM:
        if (args_uint(cmd, "dim", arg, SIZE_MAX, &u) != 0)
            return -1;
        o->gen.dim = (size_t)u;
        break;
    case OPT_CENTERS:
        if (args_uint(cmd, "centers", arg, SIZE_MAX, &u) != 0)
            return -1;
        o->gen.centers = (size_t)u;
        break;
    case OPT_SPREAD:
        if (args_double(cmd, "spread", arg, &d) != 0)
            return -1;
        o->gen.spread = (float)d;
        break;
    case OPT_SEED:
        if (args_uint(cmd, "seed", arg, UINT64_MAX, &u) != 0)
            return -1;
        o->gen.seed = u;
        return 1; /* shared with the algorithm seed, so not counted as a dataset option */
    default:
        return 0;
    }
    o->given = true;
    return 1;
}
