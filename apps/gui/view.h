#ifndef KMEANS_GUI_VIEW_H
#define KMEANS_GUI_VIEW_H

#include "kmeans/kmeans.h"
#include "runner.h"

#include <raylib.h>

typedef struct View
{
    Rectangle canvas;
    float scale;    /* pixels per world unit */
    Vector2 origin; /* screen position of world (0, 0) */
    float wmin[2], wmax[2];
    const km_dataset *ds;
    size_t *idx; /* indices of the drawn points */
    size_t nd;
    int32_t *labels; /* nd labels for labels_frame */
    bool labels_valid;
    size_t labels_frame;
    Shader voronoi;
    bool voronoi_ok;
    int loc_centroids, loc_colors, loc_count, loc_alpha;
} View;

/* Compiles the Voronoi shader. Needs the window to exist; without it Voronoi is skipped. */
void view_init_gl(View *v);
void view_free(View *v);
/* Recomputes bounds and the drawn subset (a fixed random subset when n > DRAW_MAX). */
int view_set_dataset(View *v, const km_dataset *ds, uint64_t seed);
/* Forces the labels to be recomputed on the next draw. */
void view_invalidate(View *v);
void view_layout(View *v, Rectangle canvas);
Vector2 view_to_screen(const View *v, float x, float y);
void view_draw(View *v, const FrameList *frames, size_t shown, size_t k, bool trails, bool voronoi);

#endif
