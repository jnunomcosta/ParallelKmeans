#include "args.h"
#include "commands.h"

#include <stdio.h>
#include <stdlib.h>

enum
{
    OPT_IMPL = OPT_NEXT,
    OPT_INIT,
    OPT_MAX_ITER,
    OPT_TOL,
    OPT_LABELS_OUT,
    OPT_CENTROIDS_OUT
};

static void usage(FILE *f)
{
    fputs("Usage: kmeans run [options]\n"
          "\n"
          "Cluster a CSV file or a generated dataset and print a summary.\n"
          "\n"
          "Data (either --input or the dataset options, not both):\n"
          "  -i, --input FILE      read points from a CSV file\n"
          "      --dataset KIND    uniform, blobs or rings (default: blobs)\n"
          "      --n N             number of points (default: 100000)\n"
          "      --dim D           dimensions; rings needs 2 (default: 2)\n"
          "      --centers C       blobs or rings count (default: 8)\n"
          "      --spread S        blob stddev or ring noise (default: 0.5)\n"
          "\n"
          "Algorithm:\n"
          "  -k, --k K             number of clusters (default: 8)\n"
          "      --impl IMPL       seq or omp (default: omp)\n"
          "  -t, --threads T       omp threads, 0 = OpenMP default (default: 0)\n"
          "      --init INIT       random or kmeans++ (default: kmeans++)\n"
          "      --max-iter N      iteration limit (default: 300)\n"
          "      --tol X           stop when max centroid shift <= X (default: 1e-4)\n"
          "      --seed S          generator and init seed (default: 69420)\n"
          "\n"
          "Output:\n"
          "      --labels-out FILE     write one label per line\n"
          "      --centroids-out FILE  write the centroids as CSV\n"
          "  -v, --verbose         print one line per iteration\n"
          "  -h, --help            show this help\n",
          f);
}

static bool print_step(const km_step *s, void *user)
{
    (void)user;
    printf("%4u  %.6e  %.3e  %.3f\n", s->iteration, s->inertia, s->shift, s->seconds * 1e3);
    return true;
}

static int write_labels(const char *path, const int32_t *labels, size_t n)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return KM_ERR_IO;
    for (size_t i = 0; i < n; i++)
        fprintf(f, "%d\n", (int)labels[i]);
    return fclose(f) == 0 ? KM_OK : KM_ERR_IO;
}

static int fail(int err)
{
    fprintf(stderr, "kmeans run: %s\n", km_strerror(err));
    return 1;
}

int cmd_run(int argc, char **argv)
{
    static const struct option longopts[] = {
        {"input", required_argument, 0, 'i'},
        {"n", required_argument, 0, OPT_N},
        ARGS_DATASET_LONGOPTS,
        {"k", required_argument, 0, 'k'},
        {"impl", required_argument, 0, OPT_IMPL},
        {"threads", required_argument, 0, 't'},
        {"init", required_argument, 0, OPT_INIT},
        {"max-iter", required_argument, 0, OPT_MAX_ITER},
        {"tol", required_argument, 0, OPT_TOL},
        {"labels-out", required_argument, 0, OPT_LABELS_OUT},
        {"centroids-out", required_argument, 0, OPT_CENTROIDS_OUT},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};
    ds_opts o = {km_gen_params_default(), false};
    km_config cfg = km_config_default();
    cfg.k = 8;
    const char *input = NULL, *labels_out = NULL, *centroids_out = NULL;
    bool verbose = false;
    opterr = 0;
    int c;
    while ((c = getopt_long(argc, argv, ":i:k:t:vh", longopts, NULL)) != -1)
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
            return args_bad_option("run", c, argv);
        case 'i':
            input = optarg;
            break;
        case 'k':
            if (args_uint("run", "k", optarg, SIZE_MAX, &u) != 0)
                return 2;
            cfg.k = (size_t)u;
            break;
        case 't':
            if (args_uint("run", "threads", optarg, 100000, &u) != 0)
                return 2;
            cfg.threads = (int)u;
            break;
        case OPT_IMPL:
            if (args_enum("run", "impl", optarg, ARGS_IMPL_NAMES, 2, &e) != 0)
                return 2;
            cfg.impl = (km_impl)e;
            break;
        case OPT_INIT:
            if (args_enum("run", "init", optarg, ARGS_INIT_NAMES, 2, &e) != 0)
                return 2;
            cfg.init = (km_init)e;
            break;
        case OPT_MAX_ITER:
            if (args_uint("run", "max-iter", optarg, UINT32_MAX, &u) != 0)
                return 2;
            cfg.max_iter = (unsigned)u;
            break;
        case OPT_TOL:
            if (args_double("run", "tol", optarg, &cfg.tol) != 0)
                return 2;
            break;
        case OPT_LABELS_OUT:
            labels_out = optarg;
            break;
        case OPT_CENTROIDS_OUT:
            centroids_out = optarg;
            break;
        case 'v':
            verbose = true;
            break;
        default:
            if (args_dataset_opt("run", c, optarg, &o) < 0)
                return 2;
        }
    }
    if (optind < argc)
        return args_fail("run", "unexpected argument '%s'", argv[optind]);
    if (input && o.given)
        return args_fail("run", "--input cannot be combined with dataset options");
    if (cfg.k == 0)
        return args_fail("run", "--k must be at least 1");
    if (cfg.max_iter == 0)
        return args_fail("run", "--max-iter must be at least 1");
    if (!input && (o.gen.n == 0 || o.gen.dim == 0))
        return args_fail("run", "--n and --dim must be at least 1");
    cfg.seed = o.gen.seed;

    km_dataset ds = {0};
    int err = input ? km_dataset_load_csv(&ds, input) : km_dataset_generate(&ds, &o.gen);
    if (err != KM_OK)
        return fail(err);

    km_result r;
    if (verbose)
        puts("iter  inertia       shift      ms");
    err = km_run(&ds, &cfg, verbose ? print_step : NULL, NULL, &r);
    if (err != KM_OK)
    {
        km_dataset_free(&ds);
        return fail(err);
    }

    int threads = cfg.impl == KM_IMPL_SEQ ? 1 : (cfg.threads > 0 ? cfg.threads : km_max_threads());
    printf("impl        %s\n", km_impl_name(cfg.impl));
    printf("threads     %d\n", threads);
    printf("n           %zu\n", ds.n);
    printf("dim         %zu\n", ds.dim);
    printf("k           %zu\n", cfg.k);
    printf("init        %s\n", km_init_name(cfg.init));
    printf("iterations  %u\n", r.iterations);
    printf("converged   %s\n", r.converged ? "yes" : "no");
    printf("inertia     %.6e\n", r.inertia);
    printf("total ms    %.3f\n", r.seconds * 1e3);
    printf("ms/iter     %.3f\n", r.seconds * 1e3 / r.iterations);
    if (cfg.k * ds.dim <= 64)
    {
        puts("centroids");
        for (size_t j = 0; j < cfg.k; j++)
        {
            printf("  %zu:", j);
            for (size_t d = 0; d < ds.dim; d++)
                printf(" %.5f", (double)r.centroids[j * ds.dim + d]);
            putchar('\n');
        }
    }

    err = KM_OK;
    if (labels_out)
        err = write_labels(labels_out, r.labels, ds.n);
    if (err == KM_OK && centroids_out)
    {
        km_dataset view = {cfg.k, ds.dim, r.centroids};
        err = km_dataset_save_csv(&view, centroids_out);
    }
    km_result_free(&r);
    km_dataset_free(&ds);
    return err == KM_OK ? 0 : fail(err);
}
