#ifndef KMEANS_GUI_STATS_H
#define KMEANS_GUI_STATS_H

#include <raylib.h>

typedef struct App App;

/* Inertia plot, last-run numbers and the seq-vs-omp comparison. */
void stats_draw(const App *app, Rectangle area);

#endif
