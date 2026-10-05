#include "args.h"
#include "commands.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    OPT_REPEAT = OPT_NEXT,
    OPT_WARMUP,
    OPT_ITERS,
    OPT_SCALING,
    OPT_INIT,
    OPT_CSV
};

#define CSV_HEADER                                                                                 \
    "scaling,impl,dataset,n,dim,k,threads,iters,repeats,median_s,min_s,stddev_s,per_iter_ms,"      \
    "speedup,efficiency,karp_flatt"
#define CENTROID_TOL 1e-3

static void usage(FILE *f)
{
    fputs("Usage: kmeans bench [options]\n"
          "\n"
          "Time seq vs omp, check that they agree, and print a table.\n"
          "Lists are comma-separated, e.g. --threads 1,2,4,8.\n"
          "\n"
          "Dataset:\n"
          "      --dataset KIND    uniform, blobs or rings (default: blobs)\n"
          "      --n LIST          points (per thread when weak) (default: 1e6)\n"
          "      --dim D           dimensions; rings needs 2 (default: 2)\n"
          "      --centers C       blobs or rings count (default: 8)\n"
          "      --spread S        blob stddev or ring noise (default: 0.5)\n"
          "      --seed S          generator and init seed (default: 69420)\n"
          "\n"
          "Benchmark:\n"
          "  -k, --k LIST          cluster counts (default: 8)\n"
          "  -t, --threads LIST    omp thread counts (default: 1,2,4,...,max)\n"
          "      --repeat N        timed runs per configuration (default: 5)\n"
          "      --warmup N        untimed runs per configuration (default: 1)\n"
          "      --iters N         fixed iterations per run (default: 50)\n"
          "      --scaling MODE    strong or weak (default: strong)\n"
          "      --init INIT       random or kmeans++ (default: kmeans++)\n"
          "      --csv FILE        also write results to FILE\n"
          "  -h, --help            show this help\n",
          f);
}

typedef struct stats
{
    double median, min, stddev;
} stats;

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static stats summarize(double *t, size_t n)
{
    qsort(t, n, sizeof *t, cmp_double);
    stats s;
    s.min = t[0];
    s.median = n % 2 ? t[n / 2] : 0.5 * (t[n / 2 - 1] + t[n / 2]);
    double mean = 0.0, ss = 0.0;
    for (size_t i = 0; i < n; i++)
        mean += t[i] / (double)n;
    for (size_t i = 0; i < n; i++)
        ss += (t[i] - mean) * (t[i] - mean);
    s.stddev = n > 1 ? sqrt(ss / (double)(n - 1)) : 0.0;
    return s;
}

/* Runs warmup + repeat times. The last timed run is kept in *last (caller frees). */
static int time_config(const km_dataset *ds, const km_config *cfg, size_t warmup, size_t repeat,
                       stats *st, km_result *last)
{
    double secs[1 << 16];
    if (repeat > sizeof secs / sizeof *secs)
        return KM_ERR_ARG;
    for (size_t i = 0; i < warmup; i++)
    {
        km_result r;
        int err = km_run(ds, cfg, NULL, NULL, &r);
        if (err != KM_OK)
            return err;
        km_result_free(&r);
    }
    for (size_t i = 0; i < repeat; i++)
    {
        km_result r;
        int err = km_run(ds, cfg, NULL, NULL, &r);
        if (err != KM_OK)
            return err;
        secs[i] = r.seconds;
        if (i + 1 < repeat)
            km_result_free(&r);
        else
            *last = r;
    }
    *st = summarize(secs, repeat);
    return KM_OK;
}

/* Returns true when omp matches seq. Prints the offending config and diff otherwise. */
static bool gate(const km_dataset *ds, const km_config *cfg, const km_result *seq,
                 const km_result *omp, int threads)
{
    size_t bad = 0, first = 0;
    for (size_t i = 0; i < ds->n; i++)
    {
        if (seq->labels[i] != omp->labels[i])
        {
            if (bad++ == 0)
                first = i;
        }
    }
    double maxdiff = 0.0;
    for (size_t i = 0; i < cfg->k * ds->dim; i++)
    {
        double d = fabs((double)seq->centroids[i] - (double)omp->centroids[i]);
        if (d > maxdiff)
            maxdiff = d;
    }
    if (bad == 0 && maxdiff <= CENTROID_TOL)
        return true;
    fprintf(stderr,
            "CORRECTNESS FAILURE: n=%zu k=%zu threads=%d: %zu labels differ (first at %zu), "
            "max centroid diff %g\n",
            ds->n, cfg->k, threads, bad, first, maxdiff);
    return false;
}

typedef struct row
{
    const char *scaling, *impl, *dataset;
    size_t n, dim, k;
    int threads;
    unsigned iters;
    size_t repeats;
    stats st;
    double per_iter_ms;
    double speedup, efficiency, karp_flatt; /* NAN when empty */
} row;

static void fmt_opt(char *buf, size_t len, double v, const char *empty)
{
    if (isnan(v))
        snprintf(buf, len, "%s", empty);
    else
        snprintf(buf, len, "%.6g", v);
}

static void emit_row(FILE *csv, const row *r)
{
    char sp[32], ef[32], kf[32], tsp[32], tef[32], tkf[32];
    fmt_opt(sp, sizeof sp, r->speedup, "");
    fmt_opt(ef, sizeof ef, r->efficiency, "");
    fmt_opt(kf, sizeof kf, r->karp_flatt, "");
    fmt_opt(tsp, sizeof tsp, r->speedup, "-");
    fmt_opt(tef, sizeof tef, r->efficiency, "-");
    fmt_opt(tkf, sizeof tkf, r->karp_flatt, "-");
    printf("%-7s %-4s %10zu %4zu %7d %10.5f %10.5f %10.5f %11.4f %8s %8s %8s\n", r->scaling,
           r->impl, r->n, r->k, r->threads, r->st.median, r->st.min, r->st.stddev, r->per_iter_ms,
           tsp, tef, tkf);
    fflush(stdout);
    if (csv)
    {
        fprintf(csv, "%s,%s,%s,%zu,%zu,%zu,%d,%u,%zu,%.9g,%.9g,%.9g,%.9g,%s,%s,%s\n", r->scaling,
                r->impl, r->dataset, r->n, r->dim, r->k, r->threads, r->iters, r->repeats,
                r->st.median, r->st.min, r->st.stddev, r->per_iter_ms, sp, ef, kf);
        fflush(csv);
    }
}

static const char *env_or_unset(const char *name)
{
    const char *v = getenv(name);
    return v ? v : "unset";
}

static void cpu_model(char *out, size_t len)
{
    snprintf(out, len, "unknown");
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f)
        return;
    char line[512];
    while (fgets(line, sizeof line, f))
    {
        if (strncmp(line, "model name", 10) == 0)
        {
            char *colon = strchr(line, ':');
            if (colon)
            {
                colon++;
                while (*colon == ' ' || *colon == '\t')
                    colon++;
                colon[strcspn(colon, "\n")] = '\0';
                snprintf(out, len, "%s", colon);
            }
            break;
        }
    }
    fclose(f);
}

typedef struct bench
{
    const km_gen_params *gen;
    km_config cfg; /* template: init, seed, tol, max_iter already fixed */
    size_t warmup, repeat;
    unsigned iters;
    FILE *csv;
    bool gate_failed;
} bench;

static row make_row(const bench *b, const char *scaling, km_impl impl, size_t n, size_t k,
                    int threads, const stats *st)
{
    row r = {scaling,
             km_impl_name(impl),
             args_dataset_name(b->gen->kind),
             n,
             b->gen->dim,
             k,
             threads,
             b->iters,
             b->repeat,
             *st,
             st->median / b->iters * 1000.0,
             NAN,
             NAN,
             NAN};
    return r;
}

static int run_seq(const bench *b, const km_dataset *ds, size_t k, bool timed, stats *st,
                   km_result *last)
{
    km_config cfg = b->cfg;
    cfg.k = k;
    cfg.impl = KM_IMPL_SEQ;
    if (timed)
        return time_config(ds, &cfg, b->warmup, b->repeat, st, last);
    return km_run(ds, &cfg, NULL, NULL, last);
}

static int run_omp(bench *b, const km_dataset *ds, size_t k, int threads, stats *st,
                   km_result *last)
{
    km_config cfg = b->cfg;
    cfg.k = k;
    cfg.impl = KM_IMPL_OMP;
    cfg.threads = threads;
    return time_config(ds, &cfg, b->warmup, b->repeat, st, last);
}

static void check(bench *b, const km_dataset *ds, size_t k, const km_result *seq,
                  const km_result *omp, int threads)
{
    km_config cfg = b->cfg;
    cfg.k = k;
    if (!gate(ds, &cfg, seq, omp, threads))
        b->gate_failed = true;
}

static void set_scaling_metrics(row *r, double seq_median, double seq_per_iter, bool weak, int p)
{
    if (weak)
    {
        r->efficiency = seq_per_iter / r->per_iter_ms;
        return;
    }
    r->speedup = seq_median / r->st.median;
    r->efficiency = r->speedup / p;
    if (p > 1)
        r->karp_flatt = (1.0 / r->speedup - 1.0 / p) / (1.0 - 1.0 / p);
}

static int strong(bench *b, size_t n, const size_t *ks, size_t nk, const size_t *ts, size_t nt)
{
    km_gen_params gp = *b->gen;
    gp.n = n;
    km_dataset ds;
    int err = km_dataset_generate(&ds, &gp);
    if (err != KM_OK)
        return err;
    for (size_t ki = 0; ki < nk && err == KM_OK; ki++)
    {
        size_t k = ks[ki];
        stats sst;
        km_result sres = {0};
        err = run_seq(b, &ds, k, true, &sst, &sres);
        if (err != KM_OK)
            break;
        row r = make_row(b, "strong", KM_IMPL_SEQ, n, k, 1, &sst);
        emit_row(b->csv, &r);
        for (size_t ti = 0; ti < nt && err == KM_OK; ti++)
        {
            stats ost;
            km_result ores = {0};
            err = run_omp(b, &ds, k, (int)ts[ti], &ost, &ores);
            if (err != KM_OK)
                break;
            row o = make_row(b, "strong", KM_IMPL_OMP, n, k, (int)ts[ti], &ost);
            set_scaling_metrics(&o, sst.median, 0, false, (int)ts[ti]);
            emit_row(b->csv, &o);
            check(b, &ds, k, &sres, &ores, (int)ts[ti]);
            km_result_free(&ores);
        }
        km_result_free(&sres);
    }
    km_dataset_free(&ds);
    return err;
}

static int weak(bench *b, size_t n, const size_t *ks, size_t nk, const size_t *ts, size_t nt)
{
    int err = KM_OK;
    double base_per_iter[ARGS_MAX_LIST];
    km_gen_params gp = *b->gen;
    gp.n = n;
    km_dataset ds;
    err = km_dataset_generate(&ds, &gp);
    if (err != KM_OK)
        return err;
    for (size_t ki = 0; ki < nk && err == KM_OK; ki++)
    {
        stats sst;
        km_result sres = {0};
        err = run_seq(b, &ds, ks[ki], true, &sst, &sres);
        if (err != KM_OK)
            break;
        km_result_free(&sres);
        row r = make_row(b, "weak", KM_IMPL_SEQ, n, ks[ki], 1, &sst);
        emit_row(b->csv, &r);
        base_per_iter[ki] = r.per_iter_ms;
    }
    km_dataset_free(&ds);

    for (size_t ti = 0; ti < nt && err == KM_OK; ti++)
    {
        int p = (int)ts[ti];
        gp.n = n * (size_t)p;
        err = km_dataset_generate(&ds, &gp);
        if (err != KM_OK)
            break;
        for (size_t ki = 0; ki < nk && err == KM_OK; ki++)
        {
            stats ost;
            km_result ores = {0}, sres = {0};
            err = run_omp(b, &ds, ks[ki], p, &ost, &ores);
            if (err == KM_OK)
                err = run_seq(b, &ds, ks[ki], false, NULL, &sres); /* gate only, untimed */
            if (err != KM_OK)
            {
                km_result_free(&ores);
                break;
            }
            row o = make_row(b, "weak", KM_IMPL_OMP, gp.n, ks[ki], p, &ost);
            set_scaling_metrics(&o, 0, base_per_iter[ki], true, p);
            emit_row(b->csv, &o);
            check(b, &ds, ks[ki], &sres, &ores, p);
            km_result_free(&ores);
            km_result_free(&sres);
        }
        km_dataset_free(&ds);
    }
    return err;
}

int cmd_bench(int argc, char **argv)
{
    static const struct option longopts[] = {{"n", required_argument, 0, OPT_N},
                                             ARGS_DATASET_LONGOPTS,
                                             {"k", required_argument, 0, 'k'},
                                             {"threads", required_argument, 0, 't'},
                                             {"repeat", required_argument, 0, OPT_REPEAT},
                                             {"warmup", required_argument, 0, OPT_WARMUP},
                                             {"iters", required_argument, 0, OPT_ITERS},
                                             {"scaling", required_argument, 0, OPT_SCALING},
                                             {"init", required_argument, 0, OPT_INIT},
                                             {"csv", required_argument, 0, OPT_CSV},
                                             {"help", no_argument, 0, 'h'},
                                             {0, 0, 0, 0}};
    static const char *const scaling_names[] = {"strong", "weak"};
    ds_opts o = {km_gen_params_default(), false};
    bench b = {0};
    b.cfg = km_config_default();
    b.repeat = 5;
    b.warmup = 1;
    b.iters = 50;
    size_t ns[ARGS_MAX_LIST] = {1000000}, ks[ARGS_MAX_LIST] = {8}, ts[ARGS_MAX_LIST];
    size_t nn = 1, nk = 1, nt = 0;
    int weak_mode = 0;
    const char *csv_path = NULL;
    opterr = 0;
    int c;
    while ((c = getopt_long(argc, argv, ":k:t:h", longopts, NULL)) != -1)
    {
        uint64_t u;
        int e;
        switch (c)
        {
        case 'h':
            usage(stdout);
            return 0;
        case '?':
        case ':':
            return args_bad_option("bench", c, argv);
        case OPT_N:
            if (args_list("bench", "n", optarg, SIZE_MAX, ns, &nn) != 0)
                return 2;
            break;
        case 'k':
            if (args_list("bench", "k", optarg, SIZE_MAX, ks, &nk) != 0)
                return 2;
            break;
        case 't':
            if (args_list("bench", "threads", optarg, 100000, ts, &nt) != 0)
                return 2;
            break;
        case OPT_REPEAT:
            if (args_uint("bench", "repeat", optarg, 65536, &u) != 0)
                return 2;
            b.repeat = (size_t)u;
            break;
        case OPT_WARMUP:
            if (args_uint("bench", "warmup", optarg, 1000, &u) != 0)
                return 2;
            b.warmup = (size_t)u;
            break;
        case OPT_ITERS:
            if (args_uint("bench", "iters", optarg, UINT32_MAX, &u) != 0)
                return 2;
            b.iters = (unsigned)u;
            break;
        case OPT_SCALING:
            if (args_enum("bench", "scaling", optarg, scaling_names, 2, &weak_mode) != 0)
                return 2;
            break;
        case OPT_INIT:
            if (args_enum("bench", "init", optarg, ARGS_INIT_NAMES, 2, &e) != 0)
                return 2;
            b.cfg.init = (km_init)e;
            break;
        case OPT_CSV:
            csv_path = optarg;
            break;
        default:
            if (args_dataset_opt("bench", c, optarg, &o) < 0)
                return 2;
        }
    }
    if (optind < argc)
        return args_fail("bench", "unexpected argument '%s'", argv[optind]);
    if (b.repeat == 0 || b.iters == 0)
        return args_fail("bench", "--repeat and --iters must be at least 1");
    if (o.gen.dim == 0)
        return args_fail("bench", "--dim must be at least 1");
    for (size_t i = 0; i < nn; i++)
        if (ns[i] == 0)
            return args_fail("bench", "--n values must be at least 1");
    for (size_t i = 0; i < nk; i++)
        if (ks[i] == 0)
            return args_fail("bench", "--k values must be at least 1");
    if (nt == 0)
    {
        int max = km_max_threads();
        for (int t = 1; t < max; t *= 2)
            ts[nt++] = (size_t)t;
        ts[nt++] = (size_t)max;
    }
    for (size_t i = 0; i < nt; i++)
        if (ts[i] == 0)
            return args_fail("bench", "--threads values must be at least 1");

    b.gen = &o.gen;
    b.cfg.seed = o.gen.seed;
    b.cfg.tol = -1.0; /* fixed work: every run does exactly --iters iterations */
    b.cfg.max_iter = b.iters;
    b.csv = NULL;
    if (csv_path)
    {
        b.csv = fopen(csv_path, "w");
        if (!b.csv)
        {
            fprintf(stderr, "kmeans bench: %s: %s\n", csv_path, km_strerror(KM_ERR_IO));
            return 1;
        }
        fprintf(b.csv, "%s\n", CSV_HEADER);
        fflush(b.csv);
    }

    char cpu[256];
    cpu_model(cpu, sizeof cpu);
    printf("# kmeans %s\n", KMEANS_VERSION);
    printf("# compiler: %s\n", __VERSION__);
    printf("# build type: %s\n", KMEANS_BUILD_TYPE);
    printf("# max threads: %d\n", km_max_threads());
    printf("# cpu: %s\n", cpu);
    printf("# OMP_PROC_BIND=%s OMP_PLACES=%s OMP_NUM_THREADS=%s\n", env_or_unset("OMP_PROC_BIND"),
           env_or_unset("OMP_PLACES"), env_or_unset("OMP_NUM_THREADS"));
    printf("# dataset: %s dim=%zu centers=%zu spread=%g seed=%llu\n", args_dataset_name(o.gen.kind),
           o.gen.dim, o.gen.centers, (double)o.gen.spread, (unsigned long long)o.gen.seed);
    printf("# scaling=%s init=%s iters=%u repeat=%zu warmup=%zu\n", scaling_names[weak_mode],
           km_init_name(b.cfg.init), b.iters, b.repeat, b.warmup);
    printf("%-7s %-4s %10s %4s %7s %10s %10s %10s %11s %8s %8s %8s\n", "scaling", "impl", "n", "k",
           "threads", "median_s", "min_s", "stddev_s", "per_iter_ms", "speedup", "effic", "karp");

    int err = KM_OK;
    for (size_t i = 0; i < nn && err == KM_OK; i++)
        err = weak_mode ? weak(&b, ns[i], ks, nk, ts, nt) : strong(&b, ns[i], ks, nk, ts, nt);

    if (b.csv)
        fclose(b.csv);
    if (err != KM_OK)
    {
        fprintf(stderr, "kmeans bench: %s\n", km_strerror(err));
        return 1;
    }
    return b.gate_failed ? 3 : 0;
}
