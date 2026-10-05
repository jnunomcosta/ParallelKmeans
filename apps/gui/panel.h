#ifndef KMEANS_GUI_PANEL_H
#define KMEANS_GUI_PANEL_H

#include <raylib.h>

typedef struct App App;

typedef enum
{
    PANEL_NONE,
    PANEL_GENERATE,
    PANEL_RUN
} PanelAction;

/* Widget state that raygui needs between frames. */
typedef struct PanelState
{
    bool dd_dataset, dd_init, dd_tol;
    bool ed_n, ed_centers, ed_iter, ed_seed;
    int init_idx, tol_idx;
    bool editing; /* a text box has keyboard focus; shortcuts must be ignored */
} PanelState;

void panel_init(PanelState *ps);
/* Applies the dark theme. Call once after InitWindow. */
void panel_theme(void);
PanelAction panel_draw(App *app, Rectangle area);

#endif
