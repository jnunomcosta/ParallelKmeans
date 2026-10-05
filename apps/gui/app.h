#ifndef KMEANS_GUI_APP_H
#define KMEANS_GUI_APP_H

#include "kmeans/kmeans.h"
#include "panel.h"
#include "runner.h"
#include "timeline.h"
#include "view.h"

#define K_MAX 32
#define DRAW_MAX 100000
#define PANEL_W 280
#define TIMELINE_H 44
#define STATS_H 170

typedef enum
{
    SRC_GEN,
    SRC_CSV
} DataSource;

/* All UI state. */
typedef struct App
{
    km_dataset ds;
    km_gen_params gen;
    km_config cfg;
    Runner runner;
    FrameList frames;
    RunInfo info;     /* last snapshot of the runner */
    RunInfo last_run; /* last snapshot of a clustering run (not a comparison) */
    CompareResult compare;
    bool has_compare;
    View view;
    size_t shown; /* index of the displayed frame */
    Timeline timeline;
    bool autoplay; /* start playing from frame 0 on every run */
    bool trails, voronoi, brush;
    int dragging; /* index of the centroid being dragged, or -1 */
    uint64_t brush_rng;
    PanelState panel;
    DataSource source;
    char csv_path[1024];
    char message[128]; /* last error, shown in the panel */
    size_t run_k;      /* k of the run whose frames are shown */
} App;

#endif
