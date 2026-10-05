#ifndef KMEANS_GUI_TIMELINE_H
#define KMEANS_GUI_TIMELINE_H

#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct Timeline
{
    bool playing;
    int fps; /* playback speed in frames per second, 1..30 */
    double acc;
} Timeline;

#define TIMELINE_FPS_MIN 1
#define TIMELINE_FPS_MAX 30

void timeline_init(Timeline *t);
/* Advances the shown frame while playing. count is the number of frames received so far;
 * playback pauses at the last one and stops when `finished` is set. */
void timeline_update(Timeline *t, size_t *shown, size_t count, bool finished, double dt);
/* Space, arrows, Home and End. */
void timeline_keys(Timeline *t, size_t *shown, size_t count);
void timeline_draw(Timeline *t, size_t *shown, size_t count, Rectangle area);

#endif
