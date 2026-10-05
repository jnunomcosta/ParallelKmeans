#define _POSIX_C_SOURCE 200809L

#include "app.h"
#include "image_mode.h"
#include "kmeans/kmeans.h"
#include "panel.h"
#include "stats.h"
#include "view.h"

#include <raylib.h>

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <raymath.h>
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
          "      --seed S          generator and init seed (default: 1342756)\n"
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
        cfg.k = app->run_k; /* the given centroids belong to the displayed run */
        cfg.init = KM_INIT_GIVEN;
        cfg.initial_centroids = given;
    }
    framelist_clear(&app->frames);
    view_invalidate(&app->view);
    image_mode_invalidate(&app->img);
    app->shown = 0;
    app->timeline.playing = app->autoplay;
    app->timeline.acc = 0.0;
    app->run_k = cfg.k;
    app->message[0] = '\0';
    int err = runner_start(&app->runner, &app->ds, &cfg);
    if (err != KM_OK)
        app_message(app, "run", err);
}

static void app_compare(App *app)
{
    if (app->cfg.k > app->ds.n)
    {
        snprintf(app->message, sizeof app->message, "k must not exceed n");
        return;
    }
    app->message[0] = '\0';
    /* Finish a run in flight first, so that its numbers and frames are not lost. */
    runner_cancel(&app->runner);
    runner_join(&app->runner);
    runner_poll(&app->runner, &app->frames);
    RunInfo prev = runner_info(&app->runner);
    if (!prev.is_compare && prev.status != RUN_IDLE)
        app->last_run = prev;
    int err = runner_compare(&app->runner, &app->ds, &app->cfg);
    if (err != KM_OK)
        app_message(app, "compare", err);
}

/* Builds a dataset from the current source. Image mode follows the source. */
static int app_load_dataset(App *app, km_dataset *ds)
{
    int err;
    switch (app->source)
    {
    case SRC_CSV:
        err = km_dataset_load_csv(ds, app->source_path);
#ifdef PLATFORM_WEB
        if (err == KM_OK && ds->n > WEB_MAX_N)
            ds->n = WEB_MAX_N; /* keep the first points only */
#endif
        break;
    case SRC_IMAGE:
        return image_mode_load(&app->img, app->source_path, ds);
    default:
        err = km_dataset_generate(ds, &app->gen);
    }
    if (err == KM_OK)
        image_mode_free(&app->img);
    return err;
}

/* New data from the current source, then a run on it. */
static void app_generate(App *app)
{
    km_dataset ds = {0};
    int err = app_load_dataset(app, &ds);
    if (err != KM_OK)
    {
        app_message(app, "generate", err);
        return;
    }
    runner_cancel(&app->runner);
    runner_join(&app->runner);
    km_dataset_free(&app->ds);
    app->ds = ds;
    err = view_set_dataset(&app->view, &app->ds, app->gen.seed, false);
    if (err != KM_OK)
    {
        app_message(app, "view", err);
        return;
    }
    app_run(app, NULL);
}

static void app_stop(App *app)
{
    runner_cancel(&app->runner);
    runner_join(&app->runner);
    framelist_clear(&app->frames);
    view_invalidate(&app->view);
    image_mode_invalidate(&app->img);
    app->shown = 0;
}

static void app_clear(App *app)
{
    app_stop(app);
    km_dataset_free(&app->ds);
    app->ds.dim = 2;
    view_set_dataset(&app->view, &app->ds, app->gen.seed, true);
    snprintf(app->message, sizeof app->message, "no points: paint some with Brush");
}

static void app_load_file(App *app, const char *path, DataSource source)
{
    snprintf(app->source_path, sizeof app->source_path, "%s", path);
    app->source = source;
    app_generate(app);
}

static uint64_t splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static float gaussian(uint64_t *state)
{
    uint64_t a = splitmix64(state), b = splitmix64(state);
    double u1 = 1.0 - (double)(a >> 11) * (1.0 / 9007199254740992.0);
    double u2 = (double)(b >> 11) * (1.0 / 9007199254740992.0);
    return (float)(sqrt(-2.0 * log(u1)) * cos(6.283185307179586 * u2));
}

#define BRUSH_POINTS 20
#define BRUSH_SIGMA 0.15f

static void brush_paint(App *app, Vector2 world)
{
    if (app->ds.n > 0 && app->ds.dim != 2)
        return;
    size_t n = app->ds.n;
    float *pts = realloc(app->ds.points, (n + BRUSH_POINTS) * 2 * sizeof(float));
    if (!pts)
        return;
    app_stop(app); /* the points changed, so the frames no longer describe them */
    app->ds.points = pts;
    app->ds.dim = 2;
    for (size_t i = 0; i < BRUSH_POINTS; i++)
    {
        pts[(n + i) * 2] = world.x + BRUSH_SIGMA * gaussian(&app->brush_rng);
        pts[(n + i) * 2 + 1] = world.y + BRUSH_SIGMA * gaussian(&app->brush_rng);
    }
    app->ds.n = n + BRUSH_POINTS;
    view_set_dataset(&app->view, &app->ds, app->gen.seed, true);
    snprintf(app->message, sizeof app->message, "%zu points", app->ds.n);
}

/* Brush painting and centroid dragging on the canvas. */
static void canvas_input(App *app)
{
    if (app->img.active)
        return;
    View *v = &app->view;
    Vector2 m = GetMousePosition();
    bool inside = CheckCollisionPointRec(m, v->canvas);
    if (app->brush)
    {
        if (inside && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            brush_paint(app, view_to_world(v, m));
        return;
    }
    bool have = app->shown < app->frames.count && app->ds.dim == 2;
    if (inside && have && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        const float *c = app->frames.items[app->shown].centroids;
        for (size_t j = 0; j < app->run_k; j++)
        {
            Vector2 s = view_to_screen(v, c[2 * j], c[2 * j + 1]);
            if (Vector2Distance(s, m) <= 10.0f)
            {
                app->dragging = (int)j;
                app->timeline.playing = false;
                break;
            }
        }
    }
    if (app->dragging >= 0)
    {
        Vector2 w = view_to_world(v, m);
        v->drag_active = true;
        v->drag_idx = (size_t)app->dragging;
        v->drag_xy[0] = w.x;
        v->drag_xy[1] = w.y;
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
            float init[K_MAX * 2];
            memcpy(init, app->frames.items[app->shown].centroids, app->run_k * 2 * sizeof(float));
            init[2 * app->dragging] = w.x;
            init[2 * app->dragging + 1] = w.y;
            v->drag_active = false;
            app->dragging = -1;
            app_run(app, init);
        }
    }
}

static Options o;
static App app;
static int status;
static int settled;
static bool quit;

/* One iteration of the main loop. */
static void frame(void)
{
    const Color bg = {24, 26, 30, 255};
    runner_tick(&app.runner);
    runner_poll(&app.runner, &app.frames);
    app.info = runner_info(&app.runner);
    if (!app.info.is_compare && app.info.status != RUN_IDLE)
        app.last_run = app.info;
    if (app.info.is_compare && app.info.compare_ready)
    {
        app.compare = app.info.compare;
        app.has_compare = true;
    }
    if (o.screenshot)
    {
        /* Screenshots show --frame N (clamped), or the last frame. */
        size_t last = app.frames.count ? app.frames.count - 1 : 0;
        app.shown = (o.frame < 0 || (size_t)o.frame > last) ? last : (size_t)o.frame;
    }
    else
        timeline_update(&app.timeline, &app.shown, app.frames.count, app.info.status != RUN_RUNNING,
                        (double)GetFrameTime());
    float cw = (float)(GetScreenWidth() - PANEL_W);
    float ch = (float)(GetScreenHeight() - TIMELINE_H - STATS_H);
    view_layout(&app.view, (Rectangle){PANEL_W, 0, cw, ch});

    canvas_input(&app);

    char line[160];
    if (app.message[0])
        snprintf(line, sizeof line, "%s", app.message);
    else if (app.info.is_compare && app.info.status == RUN_RUNNING)
        snprintf(line, sizeof line, "comparing seq vs omp...");
    else if (app.info.is_compare && app.info.status == RUN_DONE)
        snprintf(line, sizeof line, "comparison done");
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
    font_text("ParallelKmeans", 16, 16, 20, RAYWHITE);
    font_text(line, 16, 48, 16, LIGHTGRAY);
    if (app.img.active)
        image_mode_draw(&app.img, &app.ds, &app.frames, app.shown, app.run_k,
                        (Rectangle){PANEL_W, 0, cw, ch});
    else
        view_draw(&app.view, &app.frames, app.shown, app.run_k, app.trails, app.voronoi);
    timeline_draw(&app.timeline, &app.shown, app.frames.count,
                  (Rectangle){PANEL_W, ch, cw, TIMELINE_H});
    stats_draw(&app, (Rectangle){PANEL_W, ch + TIMELINE_H, cw, STATS_H});
    PanelAction act = panel_draw(&app, (Rectangle){0, 0, PANEL_W, (float)GetScreenHeight()});
    EndDrawing();

    if (!app.panel.editing)
    {
        if (IsKeyPressed(KEY_G))
            act = PANEL_GENERATE;
        if (IsKeyPressed(KEY_R))
            act = PANEL_RUN;
        if (IsKeyPressed(KEY_V) && !app.img.active)
            app.voronoi = !app.voronoi;
        if (IsKeyPressed(KEY_T) && !app.img.active)
            app.trails = !app.trails;
        timeline_keys(&app.timeline, &app.shown, app.frames.count);
    }
    if (IsKeyPressed(KEY_ESCAPE))
    {
        if (app.info.status == RUN_RUNNING)
            runner_cancel(&app.runner);
#ifndef PLATFORM_WEB
        else
            quit = true;
#endif
    }
#ifndef PLATFORM_WEB
    if (WindowShouldClose())
        quit = true;
#endif
    if (act == PANEL_GENERATE)
        app_generate(&app);
    else if (act == PANEL_RUN)
        app_run(&app, NULL);
    else if (act == PANEL_COMPARE)
        app_compare(&app);
    else if (act == PANEL_CLEAR)
        app_clear(&app);

    if (IsFileDropped())
    {
        FilePathList files = LoadDroppedFiles();
        if (files.count > 0 && IsFileExtension(files.paths[0], ".csv"))
            app_load_file(&app, files.paths[0], SRC_CSV);
        else if (files.count > 0 && (IsFileExtension(files.paths[0], ".png") ||
                                     IsFileExtension(files.paths[0], ".jpg") ||
                                     IsFileExtension(files.paths[0], ".jpeg")))
            app_load_file(&app, files.paths[0], SRC_IMAGE);
        UnloadDroppedFiles(files);
    }

    if (o.screenshot && app.info.status != RUN_RUNNING && ++settled >= 5)
    {
        Image img = LoadImageFromScreen();
        if (!ExportImage(img, o.screenshot))
        {
            fprintf(stderr, "kmeans-gui: could not write %s\n", o.screenshot);
            status = 1;
        }
        UnloadImage(img);
        quit = true;
    }
#ifdef PLATFORM_WEB
    if (quit)
        emscripten_cancel_main_loop();
#endif
}

int main(int argc, char **argv)
{
    o = (Options){0};
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
#ifdef PLATFORM_WEB
    /* Fixed-size canvas, scaled by CSS: raylib's resizable web window skews the mouse. */
    SetConfigFlags(FLAG_VSYNC_HINT);
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
#endif
    InitWindow(o.width, o.height, "kmeans-gui");
    if (!IsWindowReady())
    {
        fputs("kmeans-gui: could not open a window\n", stderr);
        return 1;
    }
    SetWindowMinSize(960, 600);
    SetExitKey(KEY_NULL);
#ifndef PLATFORM_WEB
    SetTargetFPS(60);
#endif
    font_init();
    panel_theme();
    status = 0;

    app = (App){0};
    app.gen = o.gen;
    app.cfg = km_config_default();
    app.cfg.k = o.k;
    app.cfg.impl = o.impl;
    app.cfg.threads = o.threads;
    app.cfg.seed = o.gen.seed;
    app.trails = o.trails;
    app.voronoi = o.voronoi;
    app.dragging = -1;
    app.brush_rng = o.gen.seed;
    view_init_gl(&app.view);
    panel_init(&app.panel);
    timeline_init(&app.timeline);
    app.autoplay = !o.screenshot;
    runner_init(&app.runner);

    if (o.input || o.image)
    {
        app.source = o.input ? SRC_CSV : SRC_IMAGE;
        snprintf(app.source_path, sizeof app.source_path, "%s", o.input ? o.input : o.image);
    }
    int err = app_load_dataset(&app, &app.ds);
    if (err == KM_OK)
        err = view_set_dataset(&app.view, &app.ds, app.gen.seed, false);
    if (err != KM_OK)
    {
        fprintf(stderr, "kmeans-gui: %s\n", km_strerror(err));
        status = 1;
    }
    else
        app_run(&app, NULL);

#ifdef PLATFORM_WEB
    if (status == 0)
        emscripten_set_main_loop(frame, 0, 1);
#else
    while (status == 0 && !quit)
        frame();
#endif
    runner_free(&app.runner);
    view_free(&app.view);
    image_mode_free(&app.img);
    framelist_free(&app.frames);
    km_dataset_free(&app.ds);
    font_free();
    CloseWindow();
    return status;
}
