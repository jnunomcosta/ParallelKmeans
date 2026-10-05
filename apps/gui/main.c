#define _POSIX_C_SOURCE 200809L

#include "app.h"
#include "kmeans/kmeans.h"
#include "panel.h"
#include "view.h"

#include <raylib.h>

#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    OPT_DATASET = 256,
    OPT_N,
    OPT_CENTERS,
    OPT_SEED,
    OPT_IMPL,
    OPT_IMAGE,
    OPT_SCREENSHOT,
    OPT_FRAME,
    OPT_NO_VORONOI,
    OPT_NO_TRAILS,
    OPT_SIZE
};

typedef struct Options
{
    km_gen_params gen;
    size_t k;
    km_impl impl;
    int threads;
    const char *input;
    const char *image;
    const char *screenshot;
    long frame; /* -1 = last */
    bool voronoi, trails;
    int width, height;
} Options;

static const char *const DATASET_NAMES[] = {"uniform", "blobs", "rings"};
static const char *const IMPL_NAMES[] = {"seq", "omp"};

static void usage(FILE *f)
{
    fputs("Usage: kmeans-gui [options]\n"
          "\n"
          "Interactive K-Means visualizer.\n"
          "\n"
          "      --dataset KIND    uniform, blobs or rings (default: blobs)\n"
          "      --n N             number of points (default: 100000)\n"
          "      --centers C       blobs or rings count (default: 8)\n"
          "  -k, --k K             number of clusters (default: 8)\n"
          "      --impl IMPL       seq or omp (default: omp)\n"
          "  -t, --threads T       omp threads, 0 = OpenMP default (default: 0)\n"
          "      --seed S          generator and init seed (default: 69420)\n"
          "  -i, --input FILE      load a CSV file\n"
          "      --image FILE      start in image mode\n"
          "      --screenshot FILE.png  render once and exit\n"
          "      --frame N         frame shown in the screenshot (default: last)\n"
          "      --no-voronoi      start with Voronoi shading off\n"
          "      --no-trails       start with trails off\n"
          "      --size WxH        window size (default: 1280x800)\n"
          "  -h, --help            show this help\n",
          f);
}

static int fail(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static int fail(const char *fmt, ...)
{
    fputs("kmeans-gui: ", stderr);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\nTry 'kmeans-gui --help'\n", stderr);
    return 2;
}

static int parse_uint(const char *opt, const char *s, uint64_t max, uint64_t *out)
{
    char *end;
    double v = strtod(s, &end);
    if (end == s || *end != '\0' || isspace((unsigned char)*s) || !isfinite(v) || v < 0 ||
        v != floor(v) || v >= 18446744073709551616.0 || v > (double)max)
    {
        fail("--%s: invalid value '%s' (expected a non-negative integer, at most %llu)", opt, s,
             (unsigned long long)max);
        return -1;
    }
    *out = (uint64_t)v;
    return 0;
}

static int parse_enum(const char *opt, const char *s, const char *const *names, int count, int *out)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(s, names[i]) == 0)
        {
            *out = i;
            return 0;
        }
    }
    fail("--%s: invalid value '%s'", opt, s);
    return -1;
}

/* Returns 0 to continue, otherwise exit code + 1. */
static int parse_args(int argc, char **argv, Options *o)
{
    static const struct option longopts[] = {{"dataset", required_argument, 0, OPT_DATASET},
                                             {"n", required_argument, 0, OPT_N},
                                             {"k", required_argument, 0, 'k'},
                                             {"centers", required_argument, 0, OPT_CENTERS},
                                             {"seed", required_argument, 0, OPT_SEED},
                                             {"impl", required_argument, 0, OPT_IMPL},
                                             {"threads", required_argument, 0, 't'},
                                             {"input", required_argument, 0, 'i'},
                                             {"image", required_argument, 0, OPT_IMAGE},
                                             {"screenshot", required_argument, 0, OPT_SCREENSHOT},
                                             {"frame", required_argument, 0, OPT_FRAME},
                                             {"no-voronoi", no_argument, 0, OPT_NO_VORONOI},
                                             {"no-trails", no_argument, 0, OPT_NO_TRAILS},
                                             {"size", required_argument, 0, OPT_SIZE},
                                             {"help", no_argument, 0, 'h'},
                                             {0, 0, 0, 0}};
    int c;
    uint64_t u;
    int e;
    while ((c = getopt_long(argc, argv, "k:t:i:h", longopts, NULL)) != -1)
    {
        switch (c)
        {
        case OPT_DATASET:
            if (parse_enum("dataset", optarg, DATASET_NAMES, 3, &e) != 0)
                return 3;
            o->gen.kind = (km_gen_kind)e;
            break;
        case OPT_N:
            if (parse_uint("n", optarg, 100000000, &u) != 0)
                return 3;
            o->gen.n = (size_t)u;
            break;
        case 'k':
            if (parse_uint("k", optarg, 32, &u) != 0)
                return 3;
            o->k = (size_t)u;
            break;
        case OPT_CENTERS:
            if (parse_uint("centers", optarg, 1000, &u) != 0)
                return 3;
            o->gen.centers = (size_t)u;
            break;
        case OPT_SEED:
            if (parse_uint("seed", optarg, UINT64_MAX, &u) != 0)
                return 3;
            o->gen.seed = u;
            break;
        case OPT_IMPL:
            if (parse_enum("impl", optarg, IMPL_NAMES, 2, &e) != 0)
                return 3;
            o->impl = (km_impl)e;
            break;
        case 't':
            if (parse_uint("threads", optarg, 4096, &u) != 0)
                return 3;
            o->threads = (int)u;
            break;
        case 'i':
            o->input = optarg;
            break;
        case OPT_IMAGE:
            o->image = optarg;
            break;
        case OPT_SCREENSHOT:
            o->screenshot = optarg;
            break;
        case OPT_FRAME:
            if (parse_uint("frame", optarg, 1000000, &u) != 0)
                return 3;
            o->frame = (long)u;
            break;
        case OPT_NO_VORONOI:
            o->voronoi = false;
            break;
        case OPT_NO_TRAILS:
            o->trails = false;
            break;
        case OPT_SIZE: {
            int w, h;
            char tail;
            if (sscanf(optarg, "%dx%d%c", &w, &h, &tail) != 2 || w < 960 || h < 600)
            {
                fail("--size: invalid value '%s' (expected WxH, at least 960x600)", optarg);
                return 3;
            }
            o->width = w;
            o->height = h;
            break;
        }
        case 'h':
            usage(stdout);
            return 1;
        default:
            fail("unknown option or missing value");
            return 3;
        }
    }
    if (optind < argc)
    {
        fail("unexpected argument '%s'", argv[optind]);
        return 3;
    }
    if (o->input && o->image)
    {
        fail("--input and --image cannot be used together");
        return 3;
    }
    return 0;
}

static void app_message(App *app, const char *what, int err)
{
    snprintf(app->message, sizeof app->message, "%s: %s", what, km_strerror(err));
}

static void app_run(App *app, const float *given)
{
    km_config cfg = app->cfg;
    if (cfg.k > app->ds.n)
    {
        snprintf(app->message, sizeof app->message, "k must not exceed n");
        return;
    }
    if (given)
    {
        cfg.init = KM_INIT_GIVEN;
        cfg.initial_centroids = given;
    }
    framelist_clear(&app->frames);
    view_invalidate(&app->view);
    app->shown = 0;
    app->run_k = cfg.k;
    app->message[0] = '\0';
    int err = runner_start(&app->runner, &app->ds, &cfg);
    if (err != KM_OK)
        app_message(app, "run", err);
}

/* New data from the current source, then a run on it. */
static void app_generate(App *app)
{
    km_dataset ds = {0};
    int err = app->source == SRC_CSV ? km_dataset_load_csv(&ds, app->csv_path)
                                     : km_dataset_generate(&ds, &app->gen);
    if (err != KM_OK)
    {
        app_message(app, "generate", err);
        return;
    }
    runner_cancel(&app->runner);
    runner_join(&app->runner);
    km_dataset_free(&app->ds);
    app->ds = ds;
    err = view_set_dataset(&app->view, &app->ds, app->gen.seed);
    if (err != KM_OK)
    {
        app_message(app, "view", err);
        return;
    }
    app_run(app, NULL);
}

int main(int argc, char **argv)
{
    Options o = {0};
    o.gen = km_gen_params_default();
    o.k = 8;
    o.impl = KM_IMPL_OMP;
    o.frame = -1;
    o.voronoi = o.trails = true;
    o.width = 1280;
    o.height = 800;
    int rc = parse_args(argc, argv, &o);
    if (rc != 0)
        return rc - 1;

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(o.width, o.height, "kmeans-gui");
    if (!IsWindowReady())
    {
        fputs("kmeans-gui: could not open a window\n", stderr);
        return 1;
    }
    SetWindowMinSize(960, 600);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    panel_theme();
    int status = 0;

    App app = {0};
    app.gen = o.gen;
    app.cfg = km_config_default();
    app.cfg.k = o.k;
    app.cfg.impl = o.impl;
    app.cfg.threads = o.threads;
    app.cfg.seed = o.gen.seed;
    app.trails = o.trails;
    panel_init(&app.panel);
    runner_init(&app.runner);

    int err = KM_OK;
    if (o.input)
    {
        app.source = SRC_CSV;
        snprintf(app.csv_path, sizeof app.csv_path, "%s", o.input);
        err = km_dataset_load_csv(&app.ds, o.input);
    }
    else
        err = km_dataset_generate(&app.ds, &app.gen);
    if (err == KM_OK)
        err = view_set_dataset(&app.view, &app.ds, app.gen.seed);
    if (err != KM_OK)
    {
        fprintf(stderr, "kmeans-gui: %s\n", km_strerror(err));
        status = 1;
    }
    else
        app_run(&app, NULL);

    const Color bg = {24, 26, 30, 255};
    int settled = 0;
    bool quit = false;
    while (status == 0 && !quit)
    {
        runner_poll(&app.runner, &app.frames);
        app.info = runner_info(&app.runner);
        if (app.frames.count > 0)
            app.shown = app.frames.count - 1;
        view_layout(&app.view, (Rectangle){PANEL_W, 0, (float)(GetScreenWidth() - PANEL_W),
                                           (float)(GetScreenHeight() - TIMELINE_H - STATS_H)});

        char line[160];
        if (app.message[0])
            snprintf(line, sizeof line, "%s", app.message);
        else if (app.info.status == RUN_RUNNING)
            snprintf(line, sizeof line, "running... iter %u", app.info.iterations);
        else if (app.info.status == RUN_DONE)
            snprintf(line, sizeof line, "done in %u iters", app.info.iterations);
        else if (app.info.status == RUN_CANCELLED)
            snprintf(line, sizeof line, "cancelled after %u iters", app.info.iterations);
        else
            snprintf(line, sizeof line, "error: %s", km_strerror(app.info.err));

        BeginDrawing();
        ClearBackground(bg);
        DrawRectangle(0, 0, PANEL_W, GetScreenHeight(), (Color){32, 35, 40, 255});
        DrawText("ParallelKmeans", 16, 16, 20, RAYWHITE);
        DrawText(line, 16, 48, 16, LIGHTGRAY);
        view_draw(&app.view, &app.frames, app.shown, app.run_k, app.trails);
        PanelAction act = panel_draw(&app, (Rectangle){0, 0, PANEL_W, (float)GetScreenHeight()});
        EndDrawing();

        if (!app.panel.editing)
        {
            if (IsKeyPressed(KEY_G))
                act = PANEL_GENERATE;
            if (IsKeyPressed(KEY_R))
                act = PANEL_RUN;
            if (IsKeyPressed(KEY_T))
                app.trails = !app.trails;
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            if (app.info.status == RUN_RUNNING)
                runner_cancel(&app.runner);
            else
                quit = true;
        }
        if (WindowShouldClose())
            quit = true;
        if (act == PANEL_GENERATE)
            app_generate(&app);
        else if (act == PANEL_RUN)
            app_run(&app, NULL);

        if (o.screenshot && app.info.status != RUN_RUNNING && ++settled >= 5)
        {
            Image img = LoadImageFromScreen();
            if (!ExportImage(img, o.screenshot))
            {
                fprintf(stderr, "kmeans-gui: could not write %s\n", o.screenshot);
                status = 1;
            }
            UnloadImage(img);
            break;
        }
    }
    runner_free(&app.runner);
    view_free(&app.view);
    framelist_free(&app.frames);
    km_dataset_free(&app.ds);
    CloseWindow();
    return status;
}
