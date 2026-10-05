#include "timeline.h"

#include "font.h"
#include "raygui.h"

#include <stdio.h>

void timeline_init(Timeline *t)
{
    t->playing = false;
    t->fps = 10;
    t->acc = 0.0;
}

static void toggle_play(Timeline *t, size_t *shown, size_t count)
{
    if (!t->playing && count > 0 && *shown + 1 >= count)
        *shown = 0; /* replay from the start */
    t->playing = !t->playing;
    t->acc = 0.0;
}

void timeline_update(Timeline *t, size_t *shown, size_t count, bool finished, double dt)
{
    if (count == 0)
    {
        *shown = 0;
        return;
    }
    if (*shown >= count)
        *shown = count - 1;
    if (!t->playing)
        return;
    t->acc += dt * t->fps;
    while (t->acc >= 1.0)
    {
        t->acc -= 1.0;
        if (*shown + 1 < count)
            (*shown)++;
        else
        {
            t->acc = 0.0;
            if (finished)
                t->playing = false;
            break;
        }
    }
}

void timeline_keys(Timeline *t, size_t *shown, size_t count)
{
    if (count == 0)
        return;
    if (IsKeyPressed(KEY_SPACE))
        toggle_play(t, shown, count);
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT))
    {
        t->playing = false;
        if (*shown > 0)
            (*shown)--;
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT))
    {
        t->playing = false;
        if (*shown + 1 < count)
            (*shown)++;
    }
    if (IsKeyPressed(KEY_HOME))
        *shown = 0;
    if (IsKeyPressed(KEY_END))
        *shown = count - 1;
}

void timeline_draw(Timeline *t, size_t *shown, size_t count, Rectangle area)
{
    DrawRectangleRec(area, (Color){32, 35, 40, 255});
    float y = area.y + 8, h = area.height - 16, x = area.x + 12, bw = 36;
    bool has = count > 0;
    if (!has)
        GuiDisable();
    if (GuiButton((Rectangle){x, y, bw, h}, "|<") && has)
        *shown = 0;
    if (GuiButton((Rectangle){x + bw + 4, y, bw, h}, "<") && has)
    {
        t->playing = false;
        if (*shown > 0)
            (*shown)--;
    }
    if (GuiButton((Rectangle){x + 2 * (bw + 4), y, 64, h}, t->playing ? "Pause" : "Play") && has)
        toggle_play(t, shown, count);
    if (GuiButton((Rectangle){x + 2 * (bw + 4) + 68, y, bw, h}, ">") && has)
    {
        t->playing = false;
        if (*shown + 1 < count)
            (*shown)++;
    }
    if (GuiButton((Rectangle){x + 3 * (bw + 4) + 68, y, bw, h}, ">|") && has)
        *shown = count - 1;

    float sx = x + 4 * (bw + 4) + 72, sw = area.x + area.width - 120 - sx;
    float f = (float)*shown;
    float old = f;
    GuiSlider((Rectangle){sx, y, sw, h}, NULL, NULL, &f, 0.0f,
              has && count > 1 ? (float)(count - 1) : 1.0f);
    if (has && f != old)
    {
        t->playing = false;
        *shown = (size_t)(f + 0.5f);
        if (*shown >= count)
            *shown = count - 1;
    }
    if (!has)
        GuiEnable();
    char buf[48];
    snprintf(buf, sizeof buf, "frame %zu / %zu", has ? *shown : 0, has ? count - 1 : 0);
    font_text(buf, (int)(sx + sw + 10), (int)y + 6, 14, LIGHTGRAY);
}
