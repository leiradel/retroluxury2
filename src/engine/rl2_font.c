#include "rl2_font.h"
#include "rl2_log.h"
#include "rl2_heap.h"

#define AL_BDF_CANVAS_TYPE rl2_PixelSource
#define AL_BDF_COLOR_TYPE rl2_ARGB8888
#define AL_BDF_PUT_PIXEL(source, x, y, color) do { rl2_putPixel(source, x, y, color); } while (0)
#define AL_BDF_MALLOC rl2_alloc
#define AL_BDF_REALLOC rl2_realloc
#define AL_BDF_FREE rl2_free
#define AL_BDF_IMPLEMENTATION
#include <al_bdf.h>

#define TAG "FNT "

struct rl2_Font {
    al_bdf_Font bdf;
};

static int rl2_bdfReader(void* const userdata, void* const buffer, size_t const count) {
    rl2_File const file = (rl2_File)userdata;
    size_t const num_read = rl2_read(file, buffer, count);
    return (int)num_read;
}

static void rl2_bdfError(al_bdf_Result const res) {
    switch (res) {
        case AL_BDF_OK: RL2_ERROR(TAG "no error"); break;
        case AL_BDF_READ_ERROR: RL2_ERROR(TAG "read error"); return;
        case AL_BDF_DIGIT_EXPECTED: RL2_ERROR(TAG "digit expected"); return;
        case AL_BDF_IDENTIFIER_EXPECTED: RL2_ERROR(TAG "identifier expected"); return;
        case AL_BDF_MALFORMED_VERSION: RL2_ERROR(TAG "malformed version"); return;
        case AL_BDF_INVALID_VERSION: RL2_ERROR(TAG "invalid version"); break;
        case AL_BDF_INVALID_DIRECTION: RL2_ERROR(TAG "invalid direction"); break;
        case AL_BDF_OUT_OF_MEMORY: RL2_ERROR(TAG "out of memory"); return;
        case AL_BDF_CHARACTER_NOT_ENDED: RL2_ERROR(TAG "unfinished characeter"); return;
        case AL_BDF_TOO_MANY_CHARACTERS: RL2_ERROR(TAG "too many characters"); return;
        case AL_BDF_CHARACTER_NOT_STARTED: RL2_ERROR(TAG "character not started"); return;
        case AL_BDF_XDIGIT_EXPECTED: RL2_ERROR(TAG "hexadecimal digit expected"); return;
    }

    RL2_ERROR(TAG "unknown error");
}

rl2_Font rl2_readFontWithFilter(rl2_File const file, rl2_GlyphFilter const filter) {
    rl2_Font font = (rl2_Font)rl2_alloc(sizeof(*font));

    if (font == NULL) {
        RL2_ERROR(TAG "out of memory");
        return NULL;
    }

    al_bdf_Result const res = al_bdf_load_filter(&font->bdf, rl2_bdfReader, filter, file);

    if (res != AL_BDF_OK) {
        rl2_bdfError(res);
        rl2_free(font);
        return NULL;
    }

    return font;
}

static int rl2_passAll(void* const userdata, int const encoding, int const non_standard) {
    (void)userdata;
    (void)encoding;
    (void)non_standard;

    /* TODO: validate this assumption. */
    return encoding != -1 ? encoding : non_standard;
}

rl2_Font rl2_readFont(rl2_File const file) {
    return rl2_readFontWithFilter(file, rl2_passAll);
}

void rl2_destroyFont(rl2_Font const font) {
    al_bdf_unload(&font->bdf);
    rl2_free(font);
}

void rl2_textSize(rl2_Font const font, int* const x0, int* const y0, int* const width, int* const height, char const* const text) {
    al_bdf_size(&font->bdf, x0, y0, width, height, text);
}

rl2_PixelSource rl2_renderText(
    rl2_Font const font, int* const x0, int* const y0, char const* const text,
    rl2_ARGB8888 const bg_color, rl2_ARGB8888 const fg_color) {

    int width = 0, height = 0;
    al_bdf_size(&font->bdf, x0, y0, &width, &height, text);

    if (width == 0 || height == 0) {
        RL2_WARN(TAG "nothing to render");
        return NULL;
    }

    rl2_PixelSource source = rl2_newPixelSource(width, height);

    if (source == NULL) {
        // Error already logged
        return NULL;
    }

    rl2_fillPixelSource(source, bg_color);
    al_bdf_render(&font->bdf, text, source, fg_color);
    return source;
}
