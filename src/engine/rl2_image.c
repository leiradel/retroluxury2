#include "rl2_image.h"
#include "rl2_log.h"
#include "rl2_heap.h"

#include <stdlib.h>
#include <string.h>

#define TAG "IMG "

typedef uint16_t rl2_Rle;

// Make sure the build fails if rl2_Rle is not big enough for a rl2_RGB565
typedef char rl2_staticAssertRleMustBeBigEnoughForRGB565[sizeof(rl2_Rle) >= sizeof(rl2_RGB565) ? 1 : -1];

typedef enum {
    // RLE is 6 bits for inverse alpha || 8 bits for length - 1 || 2 bits for operation followed by the colors
    RL2_RLE_COMPOSE = 0,

    // RLE is 14 bits for length - 1 || 2 bits for operation
    RL2_RLE_SKIP,

    // RLW is 14 bits for length - 1 || 2 bits for operation followed by the colors
    RL2_RLE_BLIT,
}
rl2_RleOp;

struct rl2_Image {
    unsigned width;
    unsigned height;
    size_t pixels_used;

#ifdef RL2_BUILD_DEBUG
    char const* path;
#endif

    rl2_Rle const* rows[1];
};

static rl2_Rle rl2_rle(rl2_RleOp const op, uint16_t const length, uint8_t const inv_alpha) {
    return op | (length - 1) << 2 | (uint16_t)inv_alpha << 10;
}

static rl2_RleOp rl2_rleOp(rl2_Rle const rle) {
    return rle & 3;
}

static uint16_t rl2_rleLength(rl2_Rle const rle) {
    return ((rle >> 2) & (rl2_rleOp(rle) == RL2_RLE_COMPOSE ? 255 : -1)) + 1;
}

static uint8_t rl2_rleInvAlpha(rl2_Rle const rle) {
    return rle >> 10;
}

static void rl2_rleRowDryRun(size_t* const words_used, size_t* const pixels_used, rl2_PixelSource const source, int const y) {
    unsigned const width = rl2_pixelSourceWidth(source);

    *words_used = 0;
    *pixels_used = 0;

    for (unsigned x = 0; x < width;) {
        rl2_ARGB8888 const pixel = rl2_getPixel(source, x, y);
        uint8_t const alpha = RL2_ARGB8888_A(pixel);
        uint8_t const real_alpha = ((uint16_t)alpha + 4) / 8;
        unsigned xx = x + 1;

        for (; xx < width; xx++) {
            rl2_ARGB8888 const pixel2 = rl2_getPixel(source, xx, y);
            uint8_t const alpha2 = RL2_ARGB8888_A(pixel2);
            uint8_t const real_alpha2 = ((uint16_t)alpha2 + 4) / 8;

            if (real_alpha2 != real_alpha) {
                break;
            }
        }

        unsigned const length = xx - x;

        if (real_alpha == 0) {
            // RL2_RLE_SKIP
            *words_used += (length + 16383) / 16384; // one RLE op for each set of 16384 skipped pixels
        }
        else if (real_alpha == 32) {
            // RL2_RLE_BLIT
            *words_used += (length + 16383) / 16384; // one RLE op for each set of 16384 skipped pixels
            *words_used += length;                   // + length colors
            *pixels_used += length;                 // overwrites length pixels of the canvas
        }
        else {
            // RL2_RLE_COMPOSE
            *words_used += (length + 255) / 256; // one RLE op for each set of 256 composed pixels
            *words_used += length;               // + length colors
            *pixels_used += length;             // overwrites length pixels of the canvas
        }

        x = xx;
    }
}

static size_t rl2_rleRow(rl2_Rle* rle, rl2_PixelSource const source, int const y) {
    unsigned const width = rl2_pixelSourceWidth(source);

    size_t words_used = 0;

    for (unsigned x = 0; x < width;) {
        rl2_ARGB8888 const pixel = rl2_getPixel(source, x, y);
        uint8_t const alpha = RL2_ARGB8888_A(pixel);
        uint8_t const real_alpha = ((uint16_t)alpha + 4) / 8;
        unsigned xx = x + 1;

        for (; xx < width; xx++) {
            rl2_ARGB8888 const pixel2 = rl2_getPixel(source, xx, y);
            uint8_t const alpha2 = RL2_ARGB8888_A(pixel2);
            uint8_t const real_alpha2 = ((uint16_t)alpha2 + 4) / 8;

            if (real_alpha2 != real_alpha) {
                break;
            }
        }

        unsigned length = xx - x;

        if (real_alpha == 0) {
            // RL2_RLE_SKIP
            words_used += (length + 16383) / 16384;

            while (length != 0) {
                unsigned const count = length < 16384 ? length : 16384;
                *rle++ = rl2_rle(RL2_RLE_SKIP, count, 0);
                length -= count;
            }
        }
        else if (real_alpha == 32) {
            // RL2_RLE_BLIT
            words_used += (length + 16383) / 16384;
            words_used += length;

            while (length != 0) {
                unsigned const count = length < 16384 ? length : 16384;
                *rle++ = rl2_rle(RL2_RLE_BLIT, count, 0);

                for (unsigned i = 0; i < count; i++) {
                    rl2_ARGB8888 const pixel2 = rl2_getPixel(source, x + i, y);
                    uint8_t const r = RL2_ARGB8888_R(pixel2);
                    uint8_t const g = RL2_ARGB8888_G(pixel2);
                    uint8_t const b = RL2_ARGB8888_B(pixel2);
                    *rle++ = RL2_COLOR_RGB565(r, g, b);
                }

                length -= count;
            }
        }
        else {
            // RL2_RLE_COMPOSE
            uint8_t const inv_alpha = 32 - real_alpha;
            words_used += (length + 255) / 256;
            words_used += length;

            while (length != 0) {
                unsigned const count = length < 256 ? length : 256;
                *rle++ = rl2_rle(RL2_RLE_COMPOSE, count, inv_alpha);

                for (unsigned i = 0; i < count; i++) {
                    rl2_ARGB8888 const pixel2 = rl2_getPixel(source, x + i, y);
                    uint8_t const r = RL2_ARGB8888_R(pixel2) * alpha / 255;
                    uint8_t const g = RL2_ARGB8888_G(pixel2) * alpha / 255;
                    uint8_t const b = RL2_ARGB8888_B(pixel2) * alpha / 255;
                    *rle++ = RL2_COLOR_RGB565(r, g, b);
                }

                length -= count;
            }
        }

        x = xx;
    }

    return words_used;
}

static rl2_RGB565 rl2_compose(rl2_RGB565 const src, rl2_RGB565 const dst, uint8_t const inv_alpha) {
    uint32_t const src32 = (src & 0xf81fU) | (uint32_t)(src & 0x07e0U) << 16;
    uint32_t const dst32 = (dst & 0xf81fU) | (uint32_t)(dst & 0x07e0U) << 16;
    uint32_t const composed = src32 + (dst32 * inv_alpha) / 32;
    return (composed & 0xf81fU) | ((composed >> 16) & 0x07e0U);
}

rl2_Image rl2_createImage(rl2_PixelSource const source) {
    size_t total_words_used = 0;
    size_t total_pixels_used = 0;

    unsigned const height = rl2_pixelSourceHeight(source);

    for (unsigned y = 0; y < height; y++) {
        size_t words_used = 0, pixels_used = 0;
        rl2_rleRowDryRun(&words_used, &pixels_used, source, y);

        total_words_used += words_used;
        total_pixels_used += pixels_used;
    }

    rl2_Image const image = (rl2_Image)rl2_alloc(sizeof(*image) + sizeof(image->rows[0]) * (height - 1) + total_words_used * 2);

    if (image == NULL) {
        RL2_ERROR(TAG "out of memory");
        return NULL;
    }

    image->width = rl2_pixelSourceWidth(source);
    image->height = height;
    image->pixels_used = total_pixels_used;

    rl2_Rle* rle = (rl2_Rle*)((uint8_t*)image + sizeof(*image) + sizeof(image->rows[0]) * (height - 1));

    for (unsigned y = 0; y < height; y++) {
        image->rows[y] = rle;
        size_t const words_used = rl2_rleRow(rle, source, y);
        rle += words_used;
    }

#ifdef RL2_BUILD_DEBUG
    char const* const path = rl2_getPixelSourcePath(source);

    if (path != NULL) {
        size_t const path_len = strlen(path);
        char* const path_dup = (char*)rl2_alloc(path_len + 1);
        image->path = path_dup;

        if (path_dup != NULL) {
            memcpy(path_dup, path, path_len + 1);
        }
    }
    else {
        image->path = NULL;
    }
#endif

    return image;
}

void rl2_destroyImage(rl2_Image const image) {
#ifdef RL2_BUILD_DEBUG
    rl2_free((void*)image->path);
#endif

    rl2_free(image);
}

unsigned rl2_imageWidth(rl2_Image const image) {
    return image->width;
}

unsigned rl2_imageHeight(rl2_Image const image) {
    return image->height;
}

size_t rl2_changedPixels(rl2_Image const image) {
    return image->pixels_used;
}

static bool rl2_clip(
    rl2_Image const image, rl2_Canvas const canvas, int* const x0, int* const y0, unsigned* const width, unsigned* const height) {

    unsigned const image_width = rl2_imageWidth(image);
    unsigned const image_height = rl2_imageHeight(image);

    unsigned const canvas_width = rl2_canvasWidth(canvas);
    unsigned const canvas_height = rl2_canvasHeight(canvas);

    if (*x0 < 0) {
        if ((unsigned)(-*x0) > image_width) {
            return false;
        }
    }
    else if ((unsigned)(*x0) >= canvas_width) {
        return false;
    }

    if (*y0 < 0) {
        if ((unsigned)(-*y0) > image_height) {
            return false;
        }
    }
    else if ((unsigned)(*y0) >= canvas_height) {
        return false;
    }

    *width = image_width;
    *height = image_height;

    if (*x0 < 0) {
        // Left clip, decrease total width and start at x0 = 0
        *width += *x0;
        *x0 = 0;
    }

    if (*x0 + *width > canvas_width) {
        // Right clip, decrease total width
        *width = canvas_width - *x0;
    }

    if (*y0 < 0) {
        // Top clip, decrease total height and start at y0 = 0
        *height += *y0;
        *y0 = 0;
    }

    if (*y0 + *height > canvas_height) {
        // Bottom clip, decrease total height
        *height = canvas_height - *y0;
    }

    return true;
}

rl2_RGB565* rl2_blit(rl2_Image const image, rl2_Canvas const canvas, int const x0, int const y0, rl2_RGB565* bg) {
    int new_x0 = x0, new_y0 = y0;
    unsigned width, height;
    
    // Clip the image to the canvas
    if (!rl2_clip(image, canvas, &new_x0, &new_y0, &width, &height)) {
        // Image is not visible
        return bg;
    }

    // Evaluate the pixel on the canvas to blit to
    rl2_RGB565* pixel = rl2_canvasPixel(canvas, new_x0, new_y0);
    size_t const pitch = rl2_canvasPitch(canvas);

    for (unsigned y = 0; y < height; y++) {
        rl2_RGB565* const saved_pixel = pixel;
        rl2_Rle const* rle = image->rows[new_y0 - y0 + y];

        rl2_RleOp op = rl2_rleOp(*rle);
        unsigned length = rl2_rleLength(*rle);
        uint8_t inv_alpha = rl2_rleInvAlpha(*rle);
        rle++;

        // Skip pixels to the left
        for (unsigned skip = new_x0 - x0; skip != 0;) {
            unsigned const count = length <= skip ? length : skip;

            if (op != RL2_RLE_SKIP) {
                // Also skip colors
                rle += count;
            }

            length -= count;
            skip -= count;

            if (length == 0) {
                // End of this RLE operation, fetch the next one
                op = rl2_rleOp(*rle);
                length = rl2_rleLength(*rle);
                inv_alpha = rl2_rleInvAlpha(*rle);
                rle++;
            }
        }

        // Write the remaining pixels
        for (unsigned remaining = width; remaining != 0;) {
            unsigned const count = length <= remaining ? length : remaining;

            if (op == RL2_RLE_BLIT) {
                // Save the overwritten pixels
                memcpy(bg, pixel, count * sizeof(*bg));
                bg += count;

                // Blit the image pixels
                memcpy(pixel, rle, count * sizeof(*pixel));
                rle += count;
            }
            else if (op == RL2_RLE_COMPOSE) {
                // Save the overwritten pixels
                memcpy(bg, pixel, count * sizeof(*bg));
                bg += count;

                // Compose the image pixels
                for (unsigned i = 0; i < count; i++) {
                    pixel[i] = rl2_compose(*rle, pixel[i], inv_alpha);
                    rle++;
                }
            }

            length -= count;
            remaining -= count;
            pixel += count;

            if (length == 0) {
                // End of this RLE operation, fetch the next one
                op = rl2_rleOp(*rle);
                length = rl2_rleLength(*rle);
                inv_alpha = rl2_rleInvAlpha(*rle);
                rle++;
            }
        }

        pixel = (rl2_RGB565*)((uint8_t*)saved_pixel + pitch);
    }

    return bg;
}

void rl2_unblit(rl2_Image const image, rl2_Canvas const canvas, int const x0, int const y0, rl2_RGB565 const* bg) {
    int new_x0 = x0, new_y0 = y0;
    unsigned width, height;
    
    // Clip the image to the canvas
    if (!rl2_clip(image, canvas, &new_x0, &new_y0, &width, &height)) {
        // Image is not visible
        return;
    }

    // Evaluate the pixel on the canvas to blit to
    rl2_RGB565* pixel = rl2_canvasPixel(canvas, new_x0, new_y0);
    size_t const pitch = rl2_canvasPitch(canvas);

    for (unsigned y = 0; y < height; y++) {
        rl2_RGB565* const saved_pixel = pixel;
        rl2_Rle const* rle = image->rows[new_y0 - y0 + y];

        rl2_RleOp op = rl2_rleOp(*rle);
        unsigned length = rl2_rleLength(*rle);
        rle++;

        // Skip pixels to the left
        for (unsigned skip = new_x0 - x0; skip != 0;) {
            unsigned const count = length <= skip ? length : skip;

            if (op != RL2_RLE_SKIP) {
                // Also skip colors
                rle += count;
            }

            length -= count;
            skip -= count;

            if (length == 0) {
                // End of this RLE operation, fetch the next one
                op = rl2_rleOp(*rle);
                length = rl2_rleLength(*rle);
                rle++;
            }
        }

        // Restore the remaining pixels
        for (unsigned remaining = width; remaining != 0;) {
            unsigned const count = length <= remaining ? length : remaining;

            if (op != RL2_RLE_SKIP) {
                // Restore the overwritten pixels
                memcpy(pixel, bg, count * sizeof(*bg));
                bg += count;
                rle += count;
            }

            length -= count;
            remaining -= count;
            pixel += count;

            if (length == 0) {
                // End of this RLE operation, fetch the next one
                op = rl2_rleOp(*rle);
                length = rl2_rleLength(*rle);
                rle++;
            }
        }

        pixel = (rl2_RGB565*)((uint8_t*)saved_pixel + pitch);
    }
}

void rl2_stamp(rl2_Image const image, rl2_Canvas const canvas, int const x0, int const y0) {
    // This is identical to rl2_blit, except overwritten pixels aren't saved
    int new_x0 = x0, new_y0 = y0;
    unsigned width, height;
    
    if (!rl2_clip(image, canvas, &new_x0, &new_y0, &width, &height)) {
        return;
    }

    rl2_RGB565* pixel = rl2_canvasPixel(canvas, new_x0, new_y0);
    size_t const pitch = rl2_canvasPitch(canvas);

    for (unsigned y = 0; y < height; y++) {
        rl2_RGB565* const saved_pixel = pixel;
        rl2_Rle const* rle = image->rows[new_y0 - y0 + y];

        rl2_RleOp op = rl2_rleOp(*rle);
        unsigned length = rl2_rleLength(*rle);
        uint8_t inv_alpha = rl2_rleInvAlpha(*rle);
        rle++;

        for (unsigned skip = new_x0 - x0; skip != 0;) {
            unsigned const count = length <= skip ? length : skip;

            if (op != RL2_RLE_SKIP) {
                rle += count;
            }

            length -= count;
            skip -= count;

            if (length == 0) {
                op = rl2_rleOp(*rle);
                length = rl2_rleLength(*rle);
                inv_alpha = rl2_rleInvAlpha(*rle);
                rle++;
            }
        }

        for (unsigned remaining = width; remaining != 0;) {
            unsigned const count = length <= remaining ? length : remaining;

            if (op == RL2_RLE_BLIT) {
                memcpy(pixel, rle, count * sizeof(*pixel));
                rle += count;
            }
            else if (op == RL2_RLE_COMPOSE) {
                for (unsigned i = 0; i < count; i++) {
                    pixel[i] = rl2_compose(*rle, pixel[i], inv_alpha);
                    rle++;
                }
            }

            length -= count;
            remaining -= count;
            pixel += count;

            if (length == 0) {
                op = rl2_rleOp(*rle);
                length = rl2_rleLength(*rle);
                inv_alpha = rl2_rleInvAlpha(*rle);
                rle++;
            }
        }

        pixel = (rl2_RGB565*)((uint8_t*)saved_pixel + pitch);
    }
}

#ifdef RL2_BUILD_DEBUG
char const* rl2_getImagePath(rl2_Image const image) {
    return image->path;
}
#endif
