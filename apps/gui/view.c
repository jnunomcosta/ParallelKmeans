#include "view.h"

#include "app.h"
#include "palette.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static const char *VORONOI_FS = "#version 330\n"
                                "out vec4 finalColor;\n"
                                "uniform vec2 centroids[32];\n"
                                "uniform vec3 colors[32];\n"
                                "uniform int count;\n"
                                "uniform float alpha;\n"
                                "void main()\n"
                                "{\n"
                                "    vec2 p = gl_FragCoord.xy;\n"
                                "    float d1 = 1e20, d2 = 1e20;\n"
                                "    int best = 0;\n"
                                "    for (int i = 0; i < count; i++)\n"
                                "    {\n"
                                "        float d = distance(p, centroids[i]);\n"
                                "        if (d < d1) { d2 = d1; d1 = d; best = i; }\n"
                                "        else if (d < d2) { d2 = d; }\n"
                                "    }\n"
                                "    if (d2 - d1 < 1.0)\n"
                                "        finalColor = vec4(colors[best] * 0.4, 0.6);\n"
                                "    else\n"
                                "        finalColor = vec4(colors[best], alpha);\n"
                                "}\n";

static size_t ycol(const km_dataset *ds)
{
    return ds->dim > 1 ? 1 : 0;
}

void view_init_gl(View *v)
{
    v->voronoi = LoadShaderFromMemory(NULL, VORONOI_FS);
    v->voronoi_ok = IsShaderValid(v->voronoi);
    v->loc_centroids = GetShaderLocation(v->voronoi, "centroids");
    v->loc_colors = GetShaderLocation(v->voronoi, "colors");
    v->loc_count = GetShaderLocation(v->voronoi, "count");
    v->loc_alpha = GetShaderLocation(v->voronoi, "alpha");
}

void view_free(View *v)
{
    if (v->voronoi_ok)
        UnloadShader(v->voronoi);
    free(v->idx);
    free(v->labels);
    *v = (View){0};
}

int view_set_dataset(View *v, const km_dataset *ds, uint64_t seed, bool keep_bounds)
{
    free(v->idx);
    free(v->labels);
    v->idx = NULL;
    v->labels = NULL;
    v->ds = ds;
    v->nd = 0;
    v->labels_valid = false;
    size_t n = ds->n;
    if (n == 0)
    {
        if (!v->bounds_set)
        {
            v->wmin[0] = v->wmin[1] = -5.0f;
            v->wmax[0] = v->wmax[1] = 5.0f;
            v->bounds_set = true;
        }
        return KM_OK;
    }

    size_t nd = n > DRAW_MAX ? DRAW_MAX : n;
    v->idx = malloc(nd * sizeof *v->idx);
    v->labels = malloc(nd * sizeof *v->labels);
    if (!v->idx || !v->labels)
    {
        free(v->idx);
        free(v->labels);
        v->idx = NULL;
        v->labels = NULL;
        return KM_ERR_NOMEM;
    }
    if (nd == n)
    {
        for (size_t i = 0; i < n; i++)
            v->idx[i] = i;
    }
    else
    {
        /* Selection sampling: each point is kept with probability needed/remaining. */
        uint64_t state = seed;
        size_t taken = 0;
        for (size_t i = 0; i < n && taken < nd; i++)
        {
            double u = (double)(splitmix64(&state) >> 11) * (1.0 / 9007199254740992.0);
            if (u * (double)(n - i) < (double)(nd - taken))
                v->idx[taken++] = i;
        }
    }
    v->nd = nd;

    if (keep_bounds && v->bounds_set)
        return KM_OK;
    v->bounds_set = true;
    size_t yd = ycol(ds);
    v->wmin[0] = v->wmax[0] = ds->points[v->idx[0] * ds->dim];
    v->wmin[1] = v->wmax[1] = ds->points[v->idx[0] * ds->dim + yd];
    for (size_t i = 1; i < nd; i++)
    {
        const float *p = ds->points + v->idx[i] * ds->dim;
        float xy[2] = {p[0], p[yd]};
        for (int a = 0; a < 2; a++)
        {
            if (xy[a] < v->wmin[a])
                v->wmin[a] = xy[a];
            if (xy[a] > v->wmax[a])
                v->wmax[a] = xy[a];
        }
    }
    return KM_OK;
}

void view_invalidate(View *v)
{
    v->labels_valid = false;
}

void view_layout(View *v, Rectangle canvas)
{
    v->canvas = canvas;
    float w = v->wmax[0] - v->wmin[0], h = v->wmax[1] - v->wmin[1];
    if (w < 1e-6f)
        w = 1.0f;
    if (h < 1e-6f)
        h = 1.0f;
    const float pad = 24.0f;
    float sx = (canvas.width - 2 * pad) / w, sy = (canvas.height - 2 * pad) / h;
    v->scale = sx < sy ? sx : sy;
    float cx = 0.5f * (v->wmin[0] + v->wmax[0]), cy = 0.5f * (v->wmin[1] + v->wmax[1]);
    v->origin.x = canvas.x + 0.5f * canvas.width - cx * v->scale;
    v->origin.y = canvas.y + 0.5f * canvas.height + cy * v->scale; /* y points up in the world */
}

Vector2 view_to_screen(const View *v, float x, float y)
{
    return (Vector2){v->origin.x + x * v->scale, v->origin.y - y * v->scale};
}

Vector2 view_to_world(const View *v, Vector2 s)
{
    return (Vector2){(s.x - v->origin.x) / v->scale, (v->origin.y - s.y) / v->scale};
}

/* Same arithmetic as the library's nearest-centroid search, so frame labels match exactly. */
static int32_t nearest(const float *p, const float *c, size_t k, size_t dim)
{
    int32_t best = 0;
    float bd = 0.0f;
    for (size_t j = 0; j < k; j++)
    {
        float d2 = 0.0f;
        for (size_t d = 0; d < dim; d++)
        {
            float diff = p[d] - c[j * dim + d];
            d2 += diff * diff;
        }
        if (j == 0 || d2 < bd)
        {
            bd = d2;
            best = (int32_t)j;
        }
    }
    return best;
}

static void fmt_count(char *out, size_t cap, size_t n)
{
    char raw[32];
    int len = snprintf(raw, sizeof raw, "%zu", n);
    size_t o = 0;
    for (int i = 0; i < len && o + 2 < cap; i++)
    {
        if (i > 0 && (len - i) % 3 == 0)
            out[o++] = ',';
        out[o++] = raw[i];
    }
    out[o] = '\0';
}

static int point_size(size_t nd)
{
    return nd <= 5000 ? 3 : nd <= 50000 ? 2 : 1;
}

void view_draw(View *v, const FrameList *frames, size_t shown, size_t k, bool trails, bool voronoi)
{
    const km_dataset *ds = v->ds;
    if (!ds || ds->n == 0)
        return;
    size_t dim = ds->dim, yd = ycol(ds);
    bool have = frames && shown < frames->count;
    const float *cent = have ? frames->items[shown].centroids : NULL;

    if (have && (!v->labels_valid || v->labels_frame != shown))
    {
        for (size_t i = 0; i < v->nd; i++)
            v->labels[i] = nearest(ds->points + v->idx[i] * dim, cent, k, dim);
        v->labels_valid = true;
        v->labels_frame = shown;
    }

    BeginScissorMode((int)v->canvas.x, (int)v->canvas.y, (int)v->canvas.width,
                     (int)v->canvas.height);
    if (voronoi && have && v->voronoi_ok && dim == 2 && k <= K_MAX)
    {
        float cs[K_MAX * 2], col[K_MAX * 3];
        float height = (float)GetRenderHeight();
        for (size_t j = 0; j < k; j++)
        {
            Vector2 s = view_to_screen(v, cent[j * dim], cent[j * dim + yd]);
            cs[2 * j] = s.x;
            cs[2 * j + 1] = height - s.y; /* gl_FragCoord has its origin at the bottom */
            Color pc = palette_color((int)j);
            col[3 * j] = pc.r / 255.0f;
            col[3 * j + 1] = pc.g / 255.0f;
            col[3 * j + 2] = pc.b / 255.0f;
        }
        int count = (int)k;
        float alpha = 0.15f;
        SetShaderValueV(v->voronoi, v->loc_centroids, cs, SHADER_UNIFORM_VEC2, count);
        SetShaderValueV(v->voronoi, v->loc_colors, col, SHADER_UNIFORM_VEC3, count);
        SetShaderValue(v->voronoi, v->loc_count, &count, SHADER_UNIFORM_INT);
        SetShaderValue(v->voronoi, v->loc_alpha, &alpha, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(v->voronoi);
        DrawRectangleRec(v->canvas, WHITE);
        EndShaderMode();
    }
    int ps = point_size(v->nd);
    for (size_t i = 0; i < v->nd; i++)
    {
        const float *p = ds->points + v->idx[i] * dim;
        Vector2 s = view_to_screen(v, p[0], p[yd]);
        Color c =
            (have && v->labels_valid) ? palette_color(v->labels[i]) : (Color){150, 150, 150, 255};
        c.a = 200;
        DrawRectangle((int)s.x - ps / 2, (int)s.y - ps / 2, ps, ps, c);
    }

    if (have)
    {
        if (trails)
        {
            for (size_t j = 0; j < k; j++)
            {
                Color c = Fade(palette_color((int)j), 0.6f);
                for (size_t f = 0; f < shown; f++)
                {
                    const float *a = frames->items[f].centroids + j * dim;
                    const float *b = frames->items[f + 1].centroids + j * dim;
                    DrawLineEx(view_to_screen(v, a[0], a[yd]), view_to_screen(v, b[0], b[yd]), 1.5f,
                               c);
                }
            }
        }
        for (size_t j = 0; j < k; j++)
        {
            Vector2 s = view_to_screen(v, cent[j * dim], cent[j * dim + yd]);
            if (v->drag_active && v->drag_idx == j)
                s = view_to_screen(v, v->drag_xy[0], v->drag_xy[1]);
            DrawCircleV(s, 6.5f, WHITE);
            DrawCircleV(s, 4.5f, palette_color((int)j));
        }
    }
    EndScissorMode();

    char a[32], b[32], line[96];
    float y = v->canvas.y + v->canvas.height - 22;
    if (ds->n > DRAW_MAX)
    {
        fmt_count(a, sizeof a, v->nd);
        fmt_count(b, sizeof b, ds->n);
        snprintf(line, sizeof line, "drawing %s of %s points", a, b);
        DrawText(line, (int)v->canvas.x + 12, (int)y, 14, LIGHTGRAY);
        y -= 18;
    }
    if (dim > 2)
    {
        snprintf(line, sizeof line, "showing dims 0,1 of %zu", dim);
        DrawText(line, (int)v->canvas.x + 12, (int)y, 14, LIGHTGRAY);
    }
}
