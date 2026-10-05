#ifndef KMEANS_GUI_FONT_H
#define KMEANS_GUI_FONT_H

#include <raylib.h>

/* Loads the UI font and makes raygui use it. Falls back to raylib's default font.
 * Call once after InitWindow. */
void font_init(void);
void font_free(void);
/* Draws text with the UI font (replacement for DrawText). */
void font_text(const char *s, int x, int y, int size, Color c);

#endif
