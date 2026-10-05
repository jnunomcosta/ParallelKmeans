#include "commands.h"

#include <stdio.h>
#include <string.h>

static void usage(FILE *f)
{
    fputs("Usage: kmeans <command> [options]\n"
          "\n"
          "Commands:\n"
          "  gen    write a generated dataset to CSV\n"
          "  run    cluster a dataset and print a summary\n"
          "  bench  time seq vs omp across sizes and thread counts\n"
          "\n"
          "Options:\n"
          "  -h, --help     show this help\n"
          "      --version  show the version\n"
          "\n"
          "Try 'kmeans <command> --help' for command options.\n",
          f);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        usage(stderr);
        return 2;
    }
    const char *c = argv[1];
    if (strcmp(c, "--help") == 0 || strcmp(c, "-h") == 0)
    {
        usage(stdout);
        return 0;
    }
    if (strcmp(c, "--version") == 0)
    {
        puts(KMEANS_VERSION);
        return 0;
    }
    if (strcmp(c, "gen") == 0)
        return cmd_gen(argc - 1, argv + 1);
    if (strcmp(c, "run") == 0)
        return cmd_run(argc - 1, argv + 1);
    if (strcmp(c, "bench") == 0)
        return cmd_bench(argc - 1, argv + 1);
    fprintf(stderr, "kmeans: unknown command '%s'\nTry 'kmeans --help'\n", c);
    return 2;
}
