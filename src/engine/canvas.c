#include "canvas.h"
#include "log.h"

#include <stdlib.h>

#define TAG "CNV "

// Image blit is coded for 16 bpp, make sure the build fails if hh2_RGB565 does not have 16 bits
typedef char hh2_staticAssertColorMustHave16Bits[sizeof(hh2_RGB565) == 2 ? 1 : -1];

struct hh2_Canvas {
    unsigned width;
    unsigned height;
    size_t pitch; // in bytes

    hh2_RGB565 pixels[1];
};

hh2_Canvas hh2_createCanvas(unsigned const width, unsigned const height) {
    size_t const pitch = ((width + 3) & ~3) * sizeof(hh2_RGB565);
    size_t const size = pitch * height;

    hh2_Canvas const canvas = (hh2_Canvas)malloc(sizeof(*canvas) + size - sizeof(canvas->pixels[0]));

    if (canvas == NULL) {
        HH2_LOG(HH2_LOG_ERROR, TAG "out of memory");
        return NULL;
    }

    canvas->width = width;
    canvas->height = height;
    canvas->pitch = pitch;

    return canvas;
}

void hh2_destroyCanvas(hh2_Canvas const canvas) {
    free(canvas);
}

unsigned hh2_canvasWidth(hh2_Canvas const canvas) {
    return canvas->width;
}

unsigned hh2_canvasHeight(hh2_Canvas const canvas) {
    return canvas->height;
}

size_t hh2_canvasPitch(hh2_Canvas const canvas) {
    return canvas->pitch;
}

void hh2_clearCanvas(hh2_Canvas const canvas, hh2_RGB565 const color) {
    unsigned const width = canvas->width;
    unsigned const height = canvas->height;
    size_t const pitch = canvas->pitch;
    hh2_RGB565* pixel = canvas->pixels;

    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
            pixel[x] = color;
        }

        pixel = (hh2_RGB565*)((uint8_t*)pixel + pitch);
    }
}

hh2_RGB565* hh2_canvasPixel(hh2_Canvas canvas, unsigned x, unsigned y) {
    return (hh2_RGB565*)((uint8_t*)canvas->pixels + y * canvas->pitch) + x;
}
