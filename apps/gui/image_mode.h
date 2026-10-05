#ifndef KMEANS_GUI_IMAGE_MODE_H
#define KMEANS_GUI_IMAGE_MODE_H

#include "kmeans/kmeans.h"
#include "runner.h"

#include <raylib.h>

/* Colour quantization: the pixels of an image are a dim-3 dataset (values 0..255). */
typedef struct ImageMode
{
    bool active;
    int w, h;
    Texture2D original, quantized;
    unsigned char *pixels; /* w*h*3 bytes behind the quantized texture */
    bool valid;            /* quantized texture matches quant_frame */
    size_t quant_frame;
} ImageMode;

/* Loads the file and fills ds with one point per pixel. Replaces any previous image. */
int image_mode_load(ImageMode *im, const char *path, km_dataset *ds);
void image_mode_free(ImageMode *im);
void image_mode_invalidate(ImageMode *im);
/* Original on the left, quantized frame on the right, palette strip below. */
void image_mode_draw(ImageMode *im, const km_dataset *ds, const FrameList *frames, size_t shown,
                     size_t k, Rectangle canvas);

#endif
