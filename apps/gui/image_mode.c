#include "image_mode.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void image_mode_free(ImageMode *im)
{
    if (im->active)
    {
        UnloadTexture(im->original);
        UnloadTexture(im->quantized);
    }
    free(im->pixels);
    *im = (ImageMode){0};
}

void image_mode_invalidate(ImageMode *im)
{
    im->valid = false;
}

int image_mode_load(ImageMode *im, const char *path, km_dataset *ds)
{
    Image img = LoadImage(path);
    if (!img.data)
        return KM_ERR_IO;
#ifdef PLATFORM_WEB
    /* The web build caps images at 512 px on the longest side. */
    int longest = img.width > img.height ? img.width : img.height;
    if (longest > 512)
    {
        int w = img.width * 512 / longest, h = img.height * 512 / longest;
        ImageResize(&img, w > 0 ? w : 1, h > 0 ? h : 1);
    }
#endif
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
    if (img.width <= 0 || img.height <= 0)
    {
        UnloadImage(img);
        return KM_ERR_IO;
    }
    size_t n = (size_t)img.width * (size_t)img.height;
    int err = km_dataset_alloc(ds, n, 3);
    unsigned char *pixels = err == KM_OK ? malloc(n * 3) : NULL;
    if (err == KM_OK && !pixels)
    {
        km_dataset_free(ds);
        err = KM_ERR_NOMEM;
    }
    if (err != KM_OK)
    {
        UnloadImage(img);
        return err;
    }
    const unsigned char *src = img.data;
    for (size_t i = 0; i < n * 3; i++)
        ds->points[i] = (float)src[i];
    memcpy(pixels, src, n * 3);

    image_mode_free(im);
    im->w = img.width;
    im->h = img.height;
    im->original = LoadTextureFromImage(img);
    Image q = {pixels, img.width, img.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8};
    im->quantized = LoadTextureFromImage(q);
    im->pixels = pixels;
    im->active = true;
    UnloadImage(img);
    return KM_OK;
}

static unsigned char to_byte(float v)
{
    return (unsigned char)(v < 0.0f ? 0.0f : v > 255.0f ? 255.0f : v + 0.5f);
}

/* Largest rectangle with the image's aspect ratio centred in box. */
static Rectangle fit(int w, int h, Rectangle box)
{
    float s = fminf(box.width / (float)w, box.height / (float)h);
    float dw = (float)w * s, dh = (float)h * s;
    return (Rectangle){box.x + 0.5f * (box.width - dw), box.y + 0.5f * (box.height - dh), dw, dh};
}

static void quantize(ImageMode *im, const km_dataset *ds, const float *cent, size_t k)
{
    unsigned char colors[32 * 3];
    for (size_t j = 0; j < k * 3; j++)
        colors[j] = to_byte(cent[j]);
    long n = (long)ds->n;
#pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++)
    {
        const float *p = ds->points + (size_t)i * 3;
        size_t best = 0;
        float bd = 0.0f;
        for (size_t j = 0; j < k; j++)
        {
            float d2 = 0.0f;
            for (size_t d = 0; d < 3; d++)
            {
                float diff = p[d] - cent[j * 3 + d];
                d2 += diff * diff;
            }
            if (j == 0 || d2 < bd)
            {
                bd = d2;
                best = j;
            }
        }
        memcpy(im->pixels + (size_t)i * 3, colors + best * 3, 3);
    }
    UpdateTexture(im->quantized, im->pixels);
}

void image_mode_draw(ImageMode *im, const km_dataset *ds, const FrameList *frames, size_t shown,
                     size_t k, Rectangle canvas)
{
    if (!im->active)
        return;
    const float pad = 12.0f, strip_h = 44.0f, label_h = 22.0f;
    bool have = shown < frames->count && k >= 1 && k <= 32;
    const float *cent = have ? frames->items[shown].centroids : NULL;
    if (have && (!im->valid || im->quant_frame != shown))
    {
        quantize(im, ds, cent, k);
        im->valid = true;
        im->quant_frame = shown;
    }

    float half = (canvas.width - 3 * pad) / 2;
    float ah = canvas.height - strip_h - 3 * pad - label_h;
    Rectangle left = {canvas.x + pad, canvas.y + pad + label_h, half, ah};
    Rectangle right = {canvas.x + 2 * pad + half, left.y, half, ah};
    Rectangle l = fit(im->w, im->h, left), r = fit(im->w, im->h, right);
    Rectangle src = {0, 0, (float)im->w, (float)im->h};
    DrawText("original", (int)left.x, (int)canvas.y + 12, 14, LIGHTGRAY);
    DrawTexturePro(im->original, src, l, (Vector2){0, 0}, 0.0f, WHITE);
    DrawText(have ? TextFormat("%zu colours, frame %zu", k, shown) : "quantized", (int)right.x,
             (int)canvas.y + 12, 14, LIGHTGRAY);
    if (have)
        DrawTexturePro(im->quantized, src, r, (Vector2){0, 0}, 0.0f, WHITE);
    else
        DrawRectangleLinesEx(r, 1.0f, GRAY);

    if (have)
    {
        float sw = (canvas.width - 2 * pad) / (float)k;
        float sy = canvas.y + canvas.height - strip_h - pad;
        for (size_t j = 0; j < k; j++)
        {
            Color c = {to_byte(cent[j * 3]), to_byte(cent[j * 3 + 1]), to_byte(cent[j * 3 + 2]),
                       255};
            DrawRectangle((int)(canvas.x + pad + sw * (float)j), (int)sy, (int)ceilf(sw),
                          (int)strip_h, c);
        }
    }
}
