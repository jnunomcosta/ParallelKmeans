#include "panel.h"

#include "app.h"

#include <math.h>
#include <stdio.h>

#include "raygui.h"

static const double TOL_VALUES[] = {1e-2, 1e-3, 1e-4, 1e-6};
#define TOL_COUNT 4

void panel_init(PanelState *ps)
{
    *ps = (PanelState){0};
    ps->init_idx = 1;
    ps->tol_idx = 2;
}

void panel_theme(void)
{
    const Color base = {52, 56, 64, 255}, hot = {70, 76, 88, 255}, press = {60, 110, 170, 255};
    const Color border = {88, 94, 106, 255}, text = {225, 228, 232, 255},
                dim = {120, 124, 132, 255};
    GuiSetStyle(DEFAULT, TEXT_SIZE, 14);
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt((Color){32, 35, 40, 255}));
    GuiSetStyle(DEFAULT, LINE_COLOR, ColorToInt(border));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, ColorToInt(base));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, ColorToInt(hot));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, ColorToInt(press));
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED, ColorToInt((Color){40, 43, 48, 255}));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, ColorToInt(border));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, ColorToInt((Color){130, 170, 220, 255}));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, ColorToInt((Color){130, 170, 220, 255}));
    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, ColorToInt((Color){60, 64, 70, 255}));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(text));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, ColorToInt(WHITE));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, ColorToInt(WHITE));
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED, ColorToInt(dim));
}

#define ROW_H 24.0f
#define ROW_GAP 4.0f
#define LABEL_W 78.0f
#define PAD 12.0f

typedef struct Cursor
{
    Rectangle area;
    float y;
} Cursor;

static Rectangle next_row(Cursor *c)
{
    Rectangle r = {c->area.x + PAD, c->y, c->area.width - 2 * PAD, ROW_H};
    c->y += ROW_H + ROW_GAP;
    return r;
}

/* Label on the left, control rectangle on the right. */
static Rectangle labelled(Cursor *c, const char *label)
{
    Rectangle r = next_row(c);
    GuiLabel((Rectangle){r.x, r.y, LABEL_W, r.height}, label);
    return (Rectangle){r.x + LABEL_W, r.y, r.width - LABEL_W, r.height};
}

static bool any_open(const PanelState *ps)
{
    return ps->dd_dataset || ps->dd_init || ps->dd_tol;
}

PanelAction panel_draw(App *app, Rectangle area)
{
    PanelState *ps = &app->panel;
    PanelAction action = PANEL_NONE;
    Cursor c = {area, area.y + 72};
    char buf[32];

    /* Rows are laid out in order; the dropdown rectangles are remembered and drawn last so
     * that their open lists overlay the rows below. */
    Rectangle r_dataset = labelled(&c, "Dataset");
    Rectangle r_n = labelled(&c, "n");
    Rectangle r_nslider = next_row(&c);
    Rectangle r_centers = labelled(&c, "Centers");
    Rectangle r_spread = labelled(&c, "Spread");
    Rectangle r_k = labelled(&c, "k");
    Rectangle r_init = labelled(&c, "Init");
    Rectangle r_impl = labelled(&c, "Impl");
    Rectangle r_threads = labelled(&c, "Threads");
    Rectangle r_seed = labelled(&c, "Seed");
    Rectangle r_iter = labelled(&c, "Max iter");
    Rectangle r_tol = labelled(&c, "Tol");
    Rectangle r_speed = labelled(&c, "Speed");

    if (any_open(ps))
        GuiLock();

    /* n: spinner plus a log-scale slider */
    int n = (int)app->gen.n;
    if (GuiSpinner(r_n, NULL, &n, 100, 2000000, ps->ed_n))
        ps->ed_n = !ps->ed_n;
    float logn = log10f((float)n);
    float old_logn = logn;
    GuiSlider(r_nslider, NULL, NULL, &logn, 2.0f, log10f(2000000.0f));
    if (logn != old_logn)
    {
        float v = powf(10.0f, logn);
        float mag = powf(10.0f, floorf(log10f(v)) - 1.0f); /* keep two significant digits */
        n = (int)(roundf(v / mag) * mag);
    }
    app->gen.n = (size_t)n;

    int centers = (int)app->gen.centers;
    if (GuiSpinner(r_centers, NULL, &centers, 1, 64, ps->ed_centers))
        ps->ed_centers = !ps->ed_centers;
    app->gen.centers = (size_t)centers;

    float spread = app->gen.spread;
    GuiSlider(r_spread, NULL, NULL, &spread, 0.05f, 2.0f);
    app->gen.spread = spread;

    float kf = (float)app->cfg.k;
    GuiSlider(r_k, NULL, NULL, &kf, 2.0f, (float)K_MAX);
    app->cfg.k = (size_t)lroundf(kf);
    snprintf(buf, sizeof buf, "%zu", app->cfg.k);
    DrawText(buf, (int)(r_k.x + r_k.width - 20), (int)r_k.y + 5, 14, WHITE);

    int impl = (int)app->cfg.impl;
    GuiToggleGroup((Rectangle){r_impl.x, r_impl.y, r_impl.width / 2 - 1, r_impl.height}, "seq;omp",
                   &impl);
    app->cfg.impl = (km_impl)impl;

    int maxthreads = km_max_threads();
    float tf = (float)(app->cfg.threads > 0 ? app->cfg.threads : maxthreads);
    if (app->cfg.impl == KM_IMPL_SEQ)
        GuiDisable();
    GuiSlider(r_threads, NULL, NULL, &tf, 1.0f, (float)(maxthreads > 1 ? maxthreads : 2));
    if (app->cfg.impl == KM_IMPL_SEQ)
        GuiEnable();
    app->cfg.threads = (int)lroundf(tf);
    snprintf(buf, sizeof buf, "%d", app->cfg.threads);
    DrawText(buf, (int)(r_threads.x + r_threads.width - 20), (int)r_threads.y + 5, 14, WHITE);

    int seed = (int)(app->gen.seed & 0x7fffffff);
    if (GuiValueBox(r_seed, NULL, &seed, 0, 2147483647, ps->ed_seed))
        ps->ed_seed = !ps->ed_seed;
    app->gen.seed = app->cfg.seed = (uint64_t)seed;

    int iter = (int)app->cfg.max_iter;
    if (GuiSpinner(r_iter, NULL, &iter, 1, 10000, ps->ed_iter))
        ps->ed_iter = !ps->ed_iter;
    app->cfg.max_iter = (unsigned)iter;

    float fps = (float)app->timeline.fps;
    GuiSlider(r_speed, NULL, NULL, &fps, (float)TIMELINE_FPS_MIN, (float)TIMELINE_FPS_MAX);
    app->timeline.fps = (int)lroundf(fps);
    snprintf(buf, sizeof buf, "%d fps", app->timeline.fps);
    DrawText(buf, (int)(r_speed.x + r_speed.width - 50), (int)r_speed.y + 5, 14, WHITE);

    Rectangle b = next_row(&c);
    float bw = b.width / 2 - 2;
    if (GuiButton((Rectangle){b.x, b.y, bw, b.height}, "Generate (G)"))
        action = PANEL_GENERATE;
    if (GuiButton((Rectangle){b.x + bw + 4, b.y, bw, b.height}, "Run (R)"))
        action = PANEL_RUN;

    Rectangle t = next_row(&c);
    GuiCheckBox((Rectangle){t.x, t.y + 4, 16, 16}, "Trails (T)", &app->trails);

    GuiUnlock();

    /* Dropdowns last, bottom to top, so open lists are not covered. */
    if (GuiDropdownBox(r_tol, "1e-2;1e-3;1e-4;1e-6", &ps->tol_idx, ps->dd_tol))
        ps->dd_tol = !ps->dd_tol;
    if (ps->tol_idx >= 0 && ps->tol_idx < TOL_COUNT)
        app->cfg.tol = TOL_VALUES[ps->tol_idx];
    if (GuiDropdownBox(r_init, "random;kmeans++", &ps->init_idx, ps->dd_init))
        ps->dd_init = !ps->dd_init;
    app->cfg.init = ps->init_idx == 0 ? KM_INIT_RANDOM : KM_INIT_PLUSPLUS;

    int ds = app->source == SRC_CSV ? 3 : (int)app->gen.kind;
    const char *items = app->source == SRC_CSV ? "uniform;blobs;rings;csv" : "uniform;blobs;rings";
    if (GuiDropdownBox(r_dataset, items, &ds, ps->dd_dataset))
        ps->dd_dataset = !ps->dd_dataset;
    if (ds < 3) /* the csv entry only exists while a CSV is loaded, so this is a user pick */
    {
        app->source = SRC_GEN;
        app->gen.kind = (km_gen_kind)ds;
    }

    ps->editing = ps->ed_n || ps->ed_centers || ps->ed_iter || ps->ed_seed;
    return action;
}
