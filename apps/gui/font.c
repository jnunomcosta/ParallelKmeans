#include "font.h"

#include "raygui.h"

#include <stddef.h>

#ifndef KMEANS_GUI_FONT_FILE
#define KMEANS_GUI_FONT_FILE "assets/DejaVuSans.ttf"
#endif

#define FONT_SIZE 16

static Font ui_font;
static bool loaded;

void font_init(void)
{
    /* ASCII only: every string in the GUI is ASCII. */
    ui_font = LoadFontEx(KMEANS_GUI_FONT_FILE, FONT_SIZE, NULL, 0);
    loaded = IsFontValid(ui_font) && ui_font.glyphCount > 0;
    if (!loaded)
    {
        TraceLog(LOG_WARNING, "kmeans-gui: could not load %s, using the default font",
                 KMEANS_GUI_FONT_FILE);
        ui_font = GetFontDefault();
        return;
    }
    SetTextureFilter(ui_font.texture, TEXTURE_FILTER_BILINEAR);
    GuiSetFont(ui_font);
}

void font_free(void)
{
    if (loaded)
        UnloadFont(ui_font);
    loaded = false;
}

void font_text(const char *s, int x, int y, int size, Color c)
{
    DrawTextEx(ui_font, s, (Vector2){(float)x, (float)y}, (float)size, 0.0f, c);
}
