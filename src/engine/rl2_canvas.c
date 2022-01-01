#include "rl2_canvas.h"
#include "rl2_heap.h"
#include "rl2_log.h"

#include <stdlib.h>

#define TAG "CNV "

// Image blit is coded for 16 bpp, make sure the build fails if hh2_RGB565 does not have 16 bits
typedef char rl2_staticAssertColorMustHave16Bits[sizeof(rl2_RGB565) == 2 ? 1 : -1];

struct rl2_Canvas {
    unsigned width;
    unsigned height;
    size_t pitch; // in bytes

    rl2_RGB565 pixels[1];
};

rl2_Canvas rl2_createCanvas(unsigned const width, unsigned const height) {
    size_t const pitch = ((width + 3) & ~3) * sizeof(rl2_RGB565);
    size_t const size = pitch * height;

    RL2_DEBUG(TAG "canvas pitch is %zu", pitch);
    RL2_DEBUG(TAG "allocating %zu bytes for the canvas", size);

    rl2_Canvas const canvas = (rl2_Canvas)rl2_alloc(sizeof(*canvas) + size - sizeof(canvas->pixels[0]));

    if (canvas == NULL) {
        RL2_ERROR(TAG "out of memory creating canvas");
        return NULL;
    }

    canvas->width = width;
    canvas->height = height;
    canvas->pitch = pitch;

    RL2_INFO(TAG "canvas created @%p, width=%u, height=%u, pitch=%zu", canvas, width, height, pitch);
    return canvas;
}

void rl2_destroyCanvas(rl2_Canvas const canvas) {
    RL2_INFO(TAG "freeing canvas @%p", canvas);
    rl2_free(canvas);
}

unsigned rl2_canvasWidth(rl2_Canvas const canvas) {
    return canvas->width;
}

unsigned rl2_canvasHeight(rl2_Canvas const canvas) {
    return canvas->height;
}

size_t rl2_canvasPitch(rl2_Canvas const canvas) {
    return canvas->pitch;
}

void rl2_clearCanvas(rl2_Canvas const canvas, rl2_RGB565 const color) {
    unsigned const width = canvas->width;
    unsigned const height = canvas->height;
    size_t const pitch = canvas->pitch;
    rl2_RGB565* pixel = canvas->pixels;

    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
            pixel[x] = color;
        }

        pixel = (rl2_RGB565*)((uint8_t*)pixel + pitch);
    }
}

rl2_RGB565* rl2_canvasPixel(rl2_Canvas const canvas, unsigned const x, unsigned const y) {
    return (rl2_RGB565*)((uint8_t*)canvas->pixels + y * canvas->pitch) + x;
}
