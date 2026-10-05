#ifndef KMEANS_GUI_APP_H
#define KMEANS_GUI_APP_H

#include "kmeans/kmeans.h"
#include "runner.h"

#define K_MAX 32
#define DRAW_MAX 100000

/* All UI state. */
typedef struct App
{
    km_dataset ds;
    km_gen_params gen;
    km_config cfg;
    Runner runner;
    FrameList frames;
    RunInfo info; /* last snapshot of the runner */
} App;

#endif
