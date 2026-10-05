#include "stats.h"

#include "app.h"

#include <stdio.h>

static void text(const char *s, float x, float y, Color c)
{
    font_text(s, (int)x, (int)y, 14, c);
}

static void draw_plot(const App *app, Rectangle r)
{
    const FrameList *fl = &app->frames;
    text("inertia vs iteration", r.x, r.y, LIGHTGRAY);
    Rectangle p = {r.x + 4, r.y + 24, r.width - 8, r.height - 32};
    DrawRectangleLinesEx(p, 1.0f, (Color){70, 74, 82, 255});
    if (fl->count < 2)
        return;
    double lo = fl->items[0].inertia, hi = lo;
    for (size_t i = 1; i < fl->count; i++)
    {
        double v = fl->items[i].inertia;
        if (v < lo)
            lo = v;
        if (v > hi)
            hi = v;
    }
    double span = hi > lo ? hi - lo : 1.0;
    const float m = 6.0f; /* margin inside the frame */
    float w = p.width - 2 * m, h = p.height - 2 * m;
    Vector2 prev = {0, 0};
    Vector2 cur = {0, 0};
    for (size_t i = 0; i < fl->count; i++)
    {
        float x = p.x + m + w * (float)i / (float)(fl->count - 1);
        float y = p.y + m + h * (1.0f - (float)((fl->items[i].inertia - lo) / span));
        Vector2 pt = {x, y};
        if (i > 0)
            DrawLineEx(prev, pt, 1.5f, (Color){110, 170, 240, 255});
        if (i == app->shown)
            cur = pt;
        prev = pt;
    }
    DrawLineEx((Vector2){cur.x, p.y + 1}, (Vector2){cur.x, p.y + p.height - 1}, 1.0f,
               (Color){255, 255, 255, 60});
    DrawCircleV(cur, 4.0f, WHITE);
}

static void draw_text(const App *app, Rectangle r)
{
    char buf[128];
    float y = r.y;
    text("last run", r.x, y, LIGHTGRAY);
    y += 22;
    const RunInfo *run = &app->last_run;
    if (run->status == RUN_IDLE)
    {
        text("none yet", r.x, y, GRAY);
        y += 20;
    }
    else if (run->status == RUN_ERROR)
    {
        snprintf(buf, sizeof buf, "error: %s", km_strerror(run->err));
        text(buf, r.x, y, GRAY);
        y += 20;
    }
    else
    {
        snprintf(buf, sizeof buf, "%s, %d thread%s", km_impl_name(run->impl), run->threads,
                 run->threads == 1 ? "" : "s");
        text(buf, r.x, y, WHITE);
        y += 20;
        const char *state = run->status == RUN_RUNNING     ? "running"
                            : run->status == RUN_CANCELLED ? "cancelled"
                            : run->converged               ? "converged"
                                                           : "not converged";
        snprintf(buf, sizeof buf, "%u iters, %s", run->iterations, state);
        text(buf, r.x, y, WHITE);
        y += 20;
        if (run->status != RUN_RUNNING && run->iterations > 0)
        {
            snprintf(buf, sizeof buf, "%.2f ms total, %.3f ms/iter", run->seconds * 1e3,
                     run->seconds * 1e3 / run->iterations);
            text(buf, r.x, y, WHITE);
            y += 20;
        }
    }
    y += 6;
    text("current frame", r.x, y, LIGHTGRAY);
    y += 22;
    if (app->shown < app->frames.count)
    {
        const Frame *f = &app->frames.items[app->shown];
        snprintf(buf, sizeof buf, "inertia %.6g", f->inertia);
        text(buf, r.x, y, WHITE);
        y += 20;
        snprintf(buf, sizeof buf, "shift %.3e", f->shift);
        text(buf, r.x, y, WHITE);
    }
    else
        text("-", r.x, y, GRAY);
}

#ifdef PLATFORM_WEB
static void draw_compare(const App *app, Rectangle r)
{
    (void)app;
    text("web build: single-threaded", r.x, r.y, LIGHTGRAY);
}
#else
static void draw_bar(Rectangle r, const char *label, double ms, double max_ms, Color c)
{
    char buf[48];
    text(label, r.x, r.y + 3, LIGHTGRAY);
    float bx = r.x + 40, bw = r.width - 40 - 74;
    float w = max_ms > 0 ? (float)(ms / max_ms) * bw : 0;
    DrawRectangle((int)bx, (int)r.y, (int)w, (int)r.height, c);
    snprintf(buf, sizeof buf, "%.1f ms", ms);
    text(buf, bx + w + 6, r.y + 3, WHITE);
}

static void draw_compare(const App *app, Rectangle r)
{
    text("seq vs omp (20 iters)", r.x, r.y, LIGHTGRAY);
    float y = r.y + 28;
    if (app->info.is_compare && app->info.status == RUN_RUNNING)
    {
        text("comparing...", r.x, y, GRAY);
        return;
    }
    if (!app->has_compare)
    {
        text("press Compare", r.x, y, GRAY);
        return;
    }
    const CompareResult *c = &app->compare;
    double mx = c->seq_seconds > c->omp_seconds ? c->seq_seconds : c->omp_seconds;
    draw_bar((Rectangle){r.x, y, r.width, 18}, "seq", c->seq_seconds * 1e3, mx * 1e3,
             (Color){230, 140, 90, 255});
    draw_bar((Rectangle){r.x, y + 28, r.width, 18}, "omp", c->omp_seconds * 1e3, mx * 1e3,
             (Color){110, 190, 130, 255});
    char buf[64];
    double sp = c->omp_seconds > 0 ? c->seq_seconds / c->omp_seconds : 0.0;
    snprintf(buf, sizeof buf, "speedup x%.1f on %d threads", sp, c->threads);
    text(buf, r.x, y + 60, WHITE);
}
#endif

void stats_draw(const App *app, Rectangle area)
{
    DrawRectangleRec(area, (Color){28, 30, 35, 255});
    const float pad = 12.0f, gap = 20.0f;
    float inner = area.width - 2 * pad - 2 * gap;
    float wp = inner * 0.40f, wt = inner * 0.28f, wc = inner * 0.32f;
    float y = area.y + 10, h = area.height - 20;
    draw_plot(app, (Rectangle){area.x + pad, y, wp, h});
    draw_text(app, (Rectangle){area.x + pad + wp + gap, y, wt, h});
    draw_compare(app, (Rectangle){area.x + pad + wp + wt + 2 * gap, y, wc, h});
}
