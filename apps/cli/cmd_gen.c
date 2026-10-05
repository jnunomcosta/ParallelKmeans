#include "args.h"
#include "commands.h"

#include <stdio.h>

static void usage(FILE *f)
{
    fputs("Usage: kmeans gen [dataset options] -o FILE\n"
          "\n"
          "Write a generated dataset to a CSV file.\n"
          "\n"
          "Options:\n"
          "  -o, --output FILE   output CSV file (required)\n"
          "      --dataset KIND  uniform, blobs or rings (default: blobs)\n"
          "      --n N           number of points (default: 100000)\n"
          "      --dim D         dimensions; rings needs 2 (default: 2)\n"
          "      --centers C     blobs or rings count (default: 8)\n"
          "      --spread S      blob stddev or ring noise (default: 0.5)\n"
          "      --seed S        generator seed (default: 69420)\n"
          "  -h, --help          show this help\n",
          f);
}

int cmd_gen(int argc, char **argv)
{
    static const struct option longopts[] = {{"output", required_argument, 0, 'o'},
                                             {"n", required_argument, 0, OPT_N},
                                             ARGS_DATASET_LONGOPTS,
                                             {"help", no_argument, 0, 'h'},
                                             {0, 0, 0, 0}};
    ds_opts o = {km_gen_params_default(), false};
    const char *output = NULL;
    opterr = 0;
    int c;
    while ((c = getopt_long(argc, argv, ":o:h", longopts, NULL)) != -1)
    {
        if (c == 'h')
        {
            usage(stdout);
            return 0;
        }
        if (c == 'o')
            output = optarg;
        else if (c == '?' || c == ':')
            return args_bad_option("gen", c, argv);
        else if (args_dataset_opt("gen", c, optarg, &o) < 0)
            return 2;
    }
    if (optind < argc)
        return args_fail("gen", "unexpected argument '%s'", argv[optind]);
    if (!output)
        return args_fail("gen", "--output is required");
    if (o.gen.n == 0 || o.gen.dim == 0)
        return args_fail("gen", "--n and --dim must be at least 1");

    km_dataset ds = {0};
    int err = km_dataset_generate(&ds, &o.gen);
    if (err == KM_OK)
        err = km_dataset_save_csv(&ds, output);
    if (err != KM_OK)
    {
        fprintf(stderr, "kmeans gen: %s\n", km_strerror(err));
        km_dataset_free(&ds);
        return 1;
    }
    printf("wrote %zu points (dim %zu) to %s\n", ds.n, ds.dim, output);
    km_dataset_free(&ds);
    return 0;
}
