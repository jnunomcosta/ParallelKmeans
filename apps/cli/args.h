#ifndef KMEANS_CLI_ARGS_H
#define KMEANS_CLI_ARGS_H

#include "kmeans/kmeans.h"

#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARGS_MAX_LIST 64

/* Option codes for long-only options shared by the subcommands. */
enum
{
    OPT_DATASET = 256,
    OPT_N,
    OPT_DIM,
    OPT_CENTERS,
    OPT_SPREAD,
    OPT_SEED,
    OPT_NEXT /* first free code for subcommand-specific options */
};

/* Long options for the dataset options except --n (gen/run and bench parse it differently). */
#define ARGS_DATASET_LONGOPTS                                                                      \
    {"dataset", required_argument, 0, OPT_DATASET}, {"dim", required_argument, 0, OPT_DIM},        \
        {"centers", required_argument, 0, OPT_CENTERS},                                            \
        {"spread", required_argument, 0, OPT_SPREAD},                                              \
    {                                                                                              \
        "seed", required_argument, 0, OPT_SEED                                                     \
    }

typedef struct ds_opts
{
    km_gen_params gen;
    bool given; /* any of --dataset --n --dim --centers --spread was passed */
} ds_opts;

/* Print "kmeans <cmd>: <message>" and the hint to stderr. Returns 2. */
int args_fail(const char *cmd, const char *fmt, ...);
/* Report a getopt '?' or ':' result. Returns 2. */
int args_bad_option(const char *cmd, int c, char **argv);

/* Parsers return 0 on success. On failure they print the error and return -1. */
int args_uint(const char *cmd, const char *opt, const char *s, uint64_t max, uint64_t *out);
int args_double(const char *cmd, const char *opt, const char *s, double *out);
int args_list(const char *cmd, const char *opt, const char *s, uint64_t max, size_t *out,
              size_t *count);
/* names[i] maps to value i. */
int args_enum(const char *cmd, const char *opt, const char *s, const char *const *names, int count,
              int *out);

/* Handles the shared dataset options. Returns 1 if handled, 0 if opt is not one, -1 on error. */
int args_dataset_opt(const char *cmd, int opt, const char *arg, ds_opts *o);

const char *args_dataset_name(km_gen_kind kind);
extern const char *const ARGS_DATASET_NAMES[];
extern const char *const ARGS_IMPL_NAMES[];
extern const char *const ARGS_INIT_NAMES[];

#endif
