#include "image.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>

#define TAG "IMG "

typedef enum {
    HH2_RLE_COMPOSE = 0,
    HH2_RLE_SKIP,
    HH2_RLE_BLIT,
}
hh2_RleOp;

typedef uint16_t hh2_Rle;

// Make sure the build fails if hh2_Rle is not big enough for a hh2_RGB565
typedef char hh2_staticAssertRleMustBeBigEnoughForRGB565[sizeof(hh2_Rle) >= sizeof(hh2_RGB565) ? 1 : -1];

struct hh2_Image {
    unsigned width;
    unsigned height;
    size_t pixels_used;

#ifdef HH2_DEBUG
    char const* path;
#endif

    hh2_Rle const* rows[1];
};

static hh2_Rle hh2_rle(hh2_RleOp const op, uint16_t const length, uint8_t const inv_alpha) {
    return op | (length - 1) << 2 | (uint16_t)inv_alpha << 10;
}

static hh2_RleOp hh2_rleOp(hh2_Rle const rle) {
    return rle & 3;
}

static uint16_t hh2_rleLength(hh2_Rle const rle) {
    return ((rle >> 2) & (hh2_rleOp(rle) == 0 ? 255 : -1)) + 1;
}

static uint8_t hh2_rleInvAlpha(hh2_Rle const rle) {
    return rle >> 10;
}

static size_t hh2_rleRowDryRun(size_t* const pixels_used, hh2_PixelSource const source, int const y) {
    unsigned const width = hh2_pixelSourceWidth(source);

    size_t words = 0;
    *pixels_used = 0;

    for (unsigned x = 0; x < width;) {
        hh2_ARGB8888 const pixel = hh2_getPixel(source, x, y);
        uint8_t const alpha = HH2_ARGB8888_A(pixel);
        uint8_t const real_alpha = ((uint16_t)alpha + 4) / 8;
        unsigned xx = x + 1;

        for (; xx < width; xx++) {
            hh2_ARGB8888 const pixel2 = hh2_getPixel(source, xx, y);
            uint8_t const alpha2 = HH2_ARGB8888_A(pixel2);
            uint8_t const real_alpha2 = ((uint16_t)alpha2 + 4) / 8;

            if (real_alpha2 != real_alpha) {
                break;
            }
        }

        unsigned const length = xx - x;

        if (real_alpha == 0) {
            words += (length + 16383) / 16384;
        }
        else if (real_alpha == 32) {
            words += (length + 16383) / 16384;
            words += length;
            *pixels_used += length;
        }
        else {
            words += (length + 255) / 256;
            words += length;
            *pixels_used += length;
        }

        x = xx;
    }

    return words;
}

static size_t hh2_rleRow(hh2_Rle* rle, hh2_PixelSource const source, int const y) {
    unsigned const width = hh2_pixelSourceWidth(source);

    size_t words = 0;

    for (unsigned x = 0; x < width;) {
        hh2_ARGB8888 const pixel = hh2_getPixel(source, x, y);
        uint8_t const alpha = HH2_ARGB8888_A(pixel);
        uint8_t const real_alpha = ((uint16_t)alpha + 4) / 8;
        unsigned xx = x + 1;

        for (; xx < width; xx++) {
            hh2_ARGB8888 const pixel2 = hh2_getPixel(source, xx, y);
            uint8_t const alpha2 = HH2_ARGB8888_A(pixel2);
            uint8_t const real_alpha2 = ((uint16_t)alpha2 + 4) / 8;

            if (real_alpha2 != real_alpha) {
                break;
            }
        }

        unsigned length = xx - x;

        if (real_alpha == 0) {
            words += (length + 16383) / 16384;

            while (length != 0) {
                unsigned const count = length < 16384 ? length : 16384;
                *rle++ = hh2_rle(HH2_RLE_SKIP, count, 0);
                length -= count;
            }
        }
        else if (real_alpha == 32) {
            words += (length + 16383) / 16384;
            words += length;

            while (length != 0) {
                unsigned const count = length < 16384 ? length : 16384;
                *rle++ = hh2_rle(HH2_RLE_BLIT, count, 0);

                for (unsigned i = 0; i < count; i++) {
                    hh2_ARGB8888 const pixel2 = hh2_getPixel(source, x + i, y);
                    uint8_t const r = HH2_ARGB8888_R(pixel2);
                    uint8_t const g = HH2_ARGB8888_G(pixel2);
                    uint8_t const b = HH2_ARGB8888_B(pixel2);
                    *rle++ = HH2_COLOR_RGB565(r, g, b);
                }

                length -= count;
            }
        }
        else {
            uint8_t const inv_alpha = 32 - real_alpha;
            words += (length + 255) / 256;
            words += length;

            while (length != 0) {
                unsigned const count = length < 256 ? length : 256;
                *rle++ = hh2_rle(HH2_RLE_COMPOSE, count, inv_alpha);

                for (unsigned i = 0; i < count; i++) {
                    hh2_ARGB8888 const pixel2 = hh2_getPixel(source, x + i, y);
                    uint8_t const r = HH2_ARGB8888_R(pixel2) * alpha / 255;
                    uint8_t const g = HH2_ARGB8888_G(pixel2) * alpha / 255;
                    uint8_t const b = HH2_ARGB8888_B(pixel2) * alpha / 255;
                    *rle++ = HH2_COLOR_RGB565(r, g, b);
                }

                length -= count;
            }
        }

        x = xx;
    }

    return words;
}

static hh2_RGB565 hh2_compose(hh2_RGB565 const src, hh2_RGB565 const dst, uint8_t const inv_alpha) {
    uint32_t const src32 = (src & 0xf81fU) | (uint32_t)(src & 0x07e0U) << 16;
    uint32_t const dst32 = (dst & 0xf81fU) | (uint32_t)(dst & 0x07e0U) << 16;
    uint32_t const composed = src32 + (dst32 * inv_alpha) / 32;
    return (composed & 0xf81fU) | ((composed >> 16) & 0x07e0U);
}

hh2_Image hh2_createImage(hh2_PixelSource const source) {
    size_t total_words = 0;
    size_t total_pixels_used = 0;

    unsigned const height = hh2_pixelSourceHeight(source);

    for (unsigned y = 0; y < height; y++) {
        size_t pixels_used;
        size_t const words = hh2_rleRowDryRun(&pixels_used, source, y);

        total_words += words;
        total_pixels_used += pixels_used;
    }

    hh2_Image const image = (hh2_Image)malloc(sizeof(*image) + sizeof(image->rows[0]) * (height - 1) + total_words * 2);

    if (image == NULL) {
        HH2_LOG(HH2_LOG_ERROR, TAG "out of memory");
        return NULL;
    }

    image->width = hh2_pixelSourceWidth(source);
    image->height = height;
    image->pixels_used = total_pixels_used;

    hh2_Rle* rle = (hh2_Rle*)((uint8_t*)image + sizeof(*image) + sizeof(image->rows[0]) * (height - 1));

    for (unsigned y = 0; y < height; y++) {
        image->rows[y] = rle;
        size_t const words = hh2_rleRow(rle, source, y);
        rle += words;
    }

#ifdef HH2_DEBUG
    {
        char const* const path = hh2_getPixelSourcePath(source);

        if (path != NULL) {
            size_t const path_len = strlen(path);
            char* const path_dup = (char*)malloc(path_len + 1);
            image->path = path_dup;

            if (path_dup != NULL) {
                memcpy(path_dup, path, path_len + 1);
            }
        }
        else {
            image->path = NULL;
        }
    }
#endif

    return image;
}

void hh2_destroyImage(hh2_Image const image) {
#ifdef HH2_DEBUG
    free((void*)image->path);
#endif

    free(image);
}

unsigned hh2_imageWidth(hh2_Image const image) {
    return image->width;
}

unsigned hh2_imageHeight(hh2_Image const image) {
    return image->height;
}

size_t hh2_changedPixels(hh2_Image const image) {
    return image->pixels_used;
}

static bool hh2_clip(
    hh2_Image const image, hh2_Canvas const canvas, int* const x0, int* const y0, unsigned* const width, unsigned* const height) {

    unsigned const image_width = hh2_imageWidth(image);
    unsigned const image_height = hh2_imageHeight(image);

    unsigned const canvas_width = hh2_canvasWidth(canvas);
    unsigned const canvas_height = hh2_canvasHeight(canvas);

    if (*x0 < 0) {
        if ((unsigned)-*x0 > image_width) {
            return false;
        }
    }
    else if ((unsigned)*x0 >= canvas_width) {
        return false;
    }

    if (*y0 < 0) {
        if ((unsigned)-*y0 > image_height) {
            return false;
        }
    }
    else if ((unsigned)*y0 >= canvas_height) {
        return false;
    }

    *width = image_width;
    *height = image_height;

    if (*x0 < 0) {
        *width += *x0;
        *x0 = 0;
    }

    if (*x0 + *width > canvas_width) {
        *width = canvas_width - *x0;
    }

    if (*y0 < 0) {
        *height += *y0;
        *y0 = 0;
    }

    if (*y0 + *height > canvas_height) {
        *height = canvas_height - *y0;
    }

    return true;
}

hh2_RGB565* hh2_blit(hh2_Image const image, hh2_Canvas const canvas, int const x0, int const y0, hh2_RGB565* bg) {
    int new_x0 = x0, new_y0 = y0;
    unsigned width, height;
    
    // Clip the image to the canvas
    if (!hh2_clip(image, canvas, &new_x0, &new_y0, &width, &height)) {
        return bg;
    }

    // Evaluate the pixel on the canvas to blit to
    hh2_RGB565* pixel = hh2_canvasPixel(canvas, new_x0, new_y0);
    size_t const pitch = hh2_canvasPitch(canvas);

    for (unsigned y = 0; y < height; y++) {
        hh2_RGB565* const saved_pixel = pixel;
        hh2_Rle const* rle = image->rows[new_y0 - y0 + y];

        hh2_RleOp op = hh2_rleOp(*rle);
        unsigned length = hh2_rleLength(*rle);
        uint8_t inv_alpha = hh2_rleInvAlpha(*rle);
        rle++;

        for (unsigned skip = new_x0 - x0; skip != 0;) {
            unsigned const count = length <= skip ? length : skip;

            if (op != HH2_RLE_SKIP) {
                rle += count;
            }

            length -= count;

            if (length == 0) {
                op = hh2_rleOp(*rle);
                length = hh2_rleLength(*rle);
                inv_alpha = hh2_rleInvAlpha(*rle);
                rle++;
            }

            skip -= count;
        }

        for (unsigned remaining = width; remaining != 0;) {
            unsigned const count = length <= remaining ? length : remaining;

            if (op == HH2_RLE_BLIT) {
                memcpy(bg, pixel, count * sizeof(*bg));
                bg += count;
                memcpy(pixel, rle, count * sizeof(*pixel));
                rle += count;
            }
            else if (op == HH2_RLE_COMPOSE) {
                memcpy(bg, pixel, count * sizeof(*bg));
                bg += count;

                for (unsigned i = 0; i < count; i++) {
                    pixel[i] = hh2_compose(*rle, pixel[i], inv_alpha);
                    rle++;
                }
            }

            pixel += count;
            length -= count;

            if (length == 0) {
                op = hh2_rleOp(*rle);
                length = hh2_rleLength(*rle);
                inv_alpha = hh2_rleInvAlpha(*rle);
                rle++;
            }

            remaining -= count;
        }

        pixel = (hh2_RGB565*)((uint8_t*)saved_pixel + pitch);
    }

    return bg;
}

void hh2_unblit(hh2_Image const image, hh2_Canvas const canvas, int const x0, int const y0, hh2_RGB565 const* bg) {
    int new_x0 = x0, new_y0 = y0;
    unsigned width, height;
    
    // Clip the image to the canvas
    if (!hh2_clip(image, canvas, &new_x0, &new_y0, &width, &height)) {
        return;
    }

    // Evaluate the pixel on the canvas to blit to
    hh2_RGB565* pixel = hh2_canvasPixel(canvas, new_x0, new_y0);
    size_t const pitch = hh2_canvasPitch(canvas);

    for (unsigned y = 0; y < height; y++) {
        hh2_RGB565* const saved_pixel = pixel;
        hh2_Rle const* rle = image->rows[new_y0 - y0 + y];

        hh2_RleOp op = hh2_rleOp(*rle);
        unsigned length = hh2_rleLength(*rle);
        rle++;

        for (unsigned skip = new_x0 - x0; skip != 0;) {
            unsigned const count = length <= skip ? length : skip;

            if (op != HH2_RLE_SKIP) {
                rle += count;
            }

            length -= count;

            if (length == 0) {
                op = hh2_rleOp(*rle);
                length = hh2_rleLength(*rle);
                rle++;
            }

            skip -= count;
        }

        for (unsigned remaining = width; remaining != 0;) {
            unsigned const count = length <= remaining ? length : remaining;

            if (op != HH2_RLE_SKIP) {
                memcpy(pixel, bg, count * sizeof(*bg));
                bg += count;
                rle += count;
            }

            pixel += count;
            length -= count;

            if (length == 0) {
                op = hh2_rleOp(*rle);
                length = hh2_rleLength(*rle);
                rle++;
            }

            remaining -= count;
        }

        pixel = (hh2_RGB565*)((uint8_t*)saved_pixel + pitch);
    }
}

void hh2_stamp(hh2_Image const image, hh2_Canvas const canvas, int const x0, int const y0) {
    int new_x0 = x0, new_y0 = y0;
    unsigned width, height;
    
    // Clip the image to the canvas
    if (!hh2_clip(image, canvas, &new_x0, &new_y0, &width, &height)) {
        return;
    }

    // Evaluate the pixel on the canvas to blit to
    hh2_RGB565* pixel = hh2_canvasPixel(canvas, new_x0, new_y0);
    size_t const pitch = hh2_canvasPitch(canvas);

    for (unsigned y = 0; y < height; y++) {
        hh2_RGB565* const saved_pixel = pixel;
        hh2_Rle const* rle = image->rows[new_y0 - y0 + y];

        hh2_RleOp op = hh2_rleOp(*rle);
        unsigned length = hh2_rleLength(*rle);
        uint8_t inv_alpha = hh2_rleInvAlpha(*rle);
        rle++;

        for (unsigned skip = new_x0 - x0; skip != 0;) {
            unsigned const count = length <= skip ? length : skip;

            if (op != HH2_RLE_SKIP) {
                rle += count;
            }

            length -= count;

            if (length == 0) {
                op = hh2_rleOp(*rle);
                length = hh2_rleLength(*rle);
                inv_alpha = hh2_rleInvAlpha(*rle);
                rle++;
            }

            skip -= count;
        }

        for (unsigned remaining = width; remaining != 0;) {
            unsigned const count = length <= remaining ? length : remaining;

            if (op == HH2_RLE_BLIT) {
                memcpy(pixel, rle, count * sizeof(*pixel));
                rle += count;
            }
            else if (op == HH2_RLE_COMPOSE) {
                for (unsigned i = 0; i < count; i++) {
                    pixel[i] = hh2_compose(*rle, pixel[i], inv_alpha);
                    rle++;
                }
            }

            pixel += count;
            length -= count;

            if (length == 0) {
                op = hh2_rleOp(*rle);
                length = hh2_rleLength(*rle);
                inv_alpha = hh2_rleInvAlpha(*rle);
                rle++;
            }

            remaining -= count;
        }

        pixel = (hh2_RGB565*)((uint8_t*)saved_pixel + pitch);
    }
}

#ifdef HH2_DEBUG
char const* hh2_getImagePath(hh2_Image image) {
    return image->path;
}
#endif
