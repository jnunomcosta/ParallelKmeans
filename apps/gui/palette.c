#include "palette.h"

/* Golden-angle hue steps with alternating lightness so neighbours stay apart. */
Color palette_color(int i)
{
    float hue = (float)((i * 137.508) - 360.0 * (int)((i * 137.508) / 360.0));
    float val = (i % 2 == 0) ? 1.0f : 0.88f;
    float sat = (i % 3 == 0) ? 0.65f : 0.80f;
    return ColorFromHSV(hue, sat, val);
}
