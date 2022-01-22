#include "rl2_pixelsrc.h"
#include "rl2_log.h"
#include "rl2_heap.h"

#include <png.h>
#include <jpeglib.h>

#include <stdlib.h>
#include <string.h>

#define TAG "PXS "

// Image read is coded for 32 bpp, make sure the build fails if hh2_ARGB8888 does not have 32 bits
typedef char rl2_staticAssertPixelMustHave32Bits[sizeof(rl2_ARGB8888) == 4 ? 1 : -1];

struct rl2_PixelSource {
    unsigned width;
    unsigned height;
    size_t pitch;
    rl2_PixelSource parent;

#ifdef RL2_BUILD_DEBUG
    char const* path;
#endif

    rl2_ARGB8888* abgr;
    rl2_ARGB8888 data[1];
};

typedef struct {
    rl2_File file;
    void const* data;
    size_t size;
    size_t pos;
}
rl2_Reader;

static size_t rl2_readFromReader(rl2_Reader* const reader, void* const buffer, size_t const size) {
    if (reader->file != NULL) {
        return rl2_read(reader->file, buffer, size);
    }
    else {
        size_t const available = reader->size - reader->pos;
        size_t const to_read = size <= available ? size : available;
        memcpy(buffer, (uint8_t const*)reader->data + reader->pos, to_read);
        reader->pos += to_read;
        return to_read;
    }
}


// ########  ##    ##  ######   
// ##     ## ###   ## ##    ##  
// ##     ## ####  ## ##        
// ########  ## ## ## ##   #### 
// ##        ##  #### ##    ##  
// ##        ##   ### ##    ##  
// ##        ##    ##  ######   

static void rl2_pngError(png_structp const png, png_const_charp const error) {
    (void)png;
    RL2_ERROR(TAG "error reading PNG: %s", error);
}

static void rl2_pngWarn(png_structp const png, png_const_charp const error) {
    (void)png;
    RL2_WARN(TAG "warning reading PNG: %s", error);
}

static void rl2_pngRead(png_structp const png, png_bytep const buffer, size_t const count) {
    rl2_Reader* const reader = png_get_io_ptr(png);
    rl2_readFromReader(reader, buffer, count);
}

static rl2_PixelSource rl2_readPng(rl2_Reader* const reader) {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, rl2_pngError, rl2_pngWarn);

    if (png == NULL) {
        return NULL;
    }

    png_infop info = png_create_info_struct(png);

    if (info == NULL) {
        png_destroy_read_struct(&png, NULL, NULL);
        return NULL;
    }

    rl2_PixelSource volatile volatile_source = NULL;

    if (setjmp(png_jmpbuf(png))) {
        if (volatile_source != NULL) {
            rl2_free(volatile_source);
        }

        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    png_set_read_fn(png, reader, rl2_pngRead);
    png_read_info(png, info);

    png_uint_32 width, height;
    int bit_depth, color_type;
    png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    if (width == 0 || height == 0) {
        RL2_ERROR(TAG "empty image reading");
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    size_t const num_pixels = width * height;
    rl2_PixelSource source = rl2_alloc(sizeof(*source) + sizeof(source->data[0]) * (num_pixels - 1));
    volatile_source = source;

    if (source == NULL) {
        RL2_ERROR(TAG "out of memory creating pixel source");
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    source->width = width;
    source->height = height;
    source->pitch = width;
    source->parent = NULL;
    source->abgr = source->data;

    // Make sure we always get RGBA pixels
    if (bit_depth == 16) {
#ifdef PNG_READ_SCALE_16_TO_8_SUPPORTED
        png_set_scale_16(png);
#else
        png_set_strip_16(png);
#endif
    }

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }

    // Transform transparent color to alpha
    if (png_get_valid(png, info, PNG_INFO_tRNS) != 0) {
        png_set_tRNS_to_alpha(png);
    }

    // Set alpha to opaque if non-existent
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png, 0xffff, PNG_FILLER_AFTER);
    }

    // Convert gray to RGB
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }

    // Turn on interlaced image support to read the PNG line by line
    int const num_passes = png_set_interlace_handling(png);

    png_read_update_info(png, info);

    for (int i = 0; i < num_passes; i++) {
        for (unsigned y = 0; y < height; y++) {
            png_read_row(png, (uint8_t*)(source->abgr + y * width), NULL);
        }
    }
    
    png_read_end(png, info);
    png_destroy_read_struct(&png, &info, NULL);
    return source;
}

//       ## ########  ########  ######   
//       ## ##     ## ##       ##    ##  
//       ## ##     ## ##       ##        
//       ## ########  ######   ##   #### 
// ##    ## ##        ##       ##    ##  
// ##    ## ##        ##       ##    ##  
//  ######  ##        ########  ######   

typedef struct {
    struct jpeg_source_mgr pub;
    rl2_Reader* reader;
    uint8_t buffer[4096];
}
rl2_jpegReader;

typedef struct {
  struct jpeg_error_mgr pub;
  jmp_buf rollback;
}
rl2_jpegError;

static void rl2_jpegDummy(j_decompress_ptr const cinfo) {}

static boolean rl2_jpegFill(j_decompress_ptr const cinfo) {
    rl2_jpegReader* const reader = (rl2_jpegReader*)cinfo->src;
    reader->pub.bytes_in_buffer = rl2_readFromReader(reader->reader, reader->buffer, sizeof(reader->buffer));
    reader->pub.next_input_byte = reader->buffer;
    return TRUE;
}

static void rl2_jpegSkip(j_decompress_ptr const cinfo, long num_bytes) {
    rl2_jpegReader* const reader = (rl2_jpegReader*)cinfo->src;

    if (num_bytes <= reader->pub.bytes_in_buffer) {
        reader->pub.bytes_in_buffer -= num_bytes;
        reader->pub.next_input_byte += num_bytes;
        return;
    }

    num_bytes -= reader->pub.bytes_in_buffer;

    while (num_bytes != 0) {
        size_t const to_read = num_bytes <= sizeof(reader->buffer) ? num_bytes : sizeof(reader->buffer);
        size_t const num_read = rl2_readFromReader(reader->reader, reader->buffer, to_read);
        reader->pub.bytes_in_buffer = num_read;
        reader->pub.next_input_byte = reader->buffer;

        if (num_read == 0) {
            // EOF reached
            reader->buffer[0] = JPEG_EOI;
            reader->pub.bytes_in_buffer = 1;
            return;
        }

        num_bytes -= num_read;
        reader->pub.bytes_in_buffer -= num_read;
        reader->pub.next_input_byte += num_read;
    }
}

static void rl2_jpegExit(j_common_ptr const cinfo) {
    rl2_jpegError* const error = (rl2_jpegError*)cinfo->err;
    error->pub.output_message(cinfo);
    longjmp(error->rollback, 1);
}

static void rl2_jpegErr(j_common_ptr const cinfo) {
    char buffer[JMSG_LENGTH_MAX];

    cinfo->err->format_message(cinfo, buffer);
    RL2_ERROR(TAG "error reading JPEG: %s", buffer);
}

static rl2_PixelSource rl2_readJpeg(rl2_Reader* const the_reader) {
    struct jpeg_decompress_struct cinfo;
    memset(&cinfo, 0, sizeof(cinfo));

    rl2_jpegError error;
    memset(&error, 0, sizeof(error));

    cinfo.err = jpeg_std_error(&error.pub);
    error.pub.error_exit = rl2_jpegExit;
    error.pub.output_message = rl2_jpegErr;

    if (setjmp(error.rollback)) {
        jpeg_destroy_decompress(&cinfo);
        return 0;
    }

    rl2_jpegReader reader;
    memset(&reader, 0, sizeof(reader));

    reader.reader = the_reader;
    reader.pub.init_source = rl2_jpegDummy;
    reader.pub.fill_input_buffer = rl2_jpegFill;
    reader.pub.skip_input_data = rl2_jpegSkip;
    reader.pub.resync_to_restart = jpeg_resync_to_restart; // default
    reader.pub.term_source = rl2_jpegDummy;
    reader.pub.bytes_in_buffer = 0;
    reader.pub.next_input_byte = NULL;

    jpeg_create_decompress(&cinfo);
    cinfo.src = (struct jpeg_source_mgr*)&reader;

    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_EXT_RGBX;

    jpeg_start_decompress(&cinfo);

    JDIMENSION const width = cinfo.output_width;
    JDIMENSION const height = cinfo.output_height;

    if (width == 0 || height == 0) {
        RL2_ERROR(TAG "empty image reading");
        jpeg_destroy_decompress(&cinfo);
        return NULL;
    }

    size_t const num_pixels = width * height;
    rl2_PixelSource const source = malloc(sizeof(*source) + sizeof(source->data[0]) * (num_pixels - 1));

    if (source == NULL) {
        RL2_ERROR(TAG "out of memory creating pixel source");
        jpeg_destroy_decompress(&cinfo);
        return NULL;
    }

    source->width = width;
    source->height = height;
    source->pitch = width;
    source->parent = NULL;
    source->abgr = source->data;

    unsigned y = 0;

    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW row = (uint8_t*)(source->abgr + y * width);
        JSAMPARRAY const array = &row;

        jpeg_read_scanlines(&cinfo, array, 1);
        y++;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return source;
}

static bool rl2_isPng(void const* const header) {
    static uint8_t const png_header[8] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    return memcmp(header, png_header, 8) == 0;
}

rl2_PixelSource rl2_newPixelSource(unsigned const width, unsigned const height) {
    size_t const num_pixels = width * height;
    rl2_PixelSource source = rl2_alloc(sizeof(*source) + sizeof(source->data[0]) * (num_pixels - 1));

    if (source == NULL) {
        RL2_ERROR(TAG "out of memory");
        return NULL;
    }

    return source;
}

rl2_PixelSource rl2_initPixelSource(void const* const data, size_t const size) {

    rl2_Reader reader;
    reader.file = NULL;
    reader.data = data;
    reader.size = size;
    reader.pos = 0;

    rl2_PixelSource const source = rl2_isPng(data) ? rl2_readPng(&reader) : rl2_readJpeg(&reader);

#ifdef RL2_BUILD_DEBUG
    source->path = NULL;
#endif

    return source;
}

rl2_PixelSource rl2_readPixelSource(rl2_Filesys const filesys, char const* const path) {
    rl2_File const file = rl2_openFile(filesys, path);

    if (file == NULL) {
        // Error already logged
        return NULL;
    }

    uint8_t header[8];

    if (rl2_read(file, header, 8) != 8) {
        RL2_ERROR(TAG "error reading from image \"%s\"", path);
        rl2_close(file);
        return NULL;
    }

    rl2_seek(file, 0, SEEK_SET);

    rl2_Reader reader;
    reader.file = file;
    reader.data = NULL;
    reader.size = 0;
    reader.pos = 0;

    rl2_PixelSource const source = rl2_isPng(header) ? rl2_readPng(&reader) : rl2_readJpeg(&reader);
    rl2_close(file);

#ifdef RL2_BUILD_DEBUG
    if (source != NULL) {
        size_t const path_len = strlen(path);
        char* const path_dup = (char*)rl2_alloc(path_len + 1);
        source->path = path_dup;

        if (path_dup != NULL) {
            memcpy(path_dup, path, path_len + 1);
        }
    }
#endif

    return source;
}

rl2_PixelSource rl2_subPixelSource(
    rl2_PixelSource const parent, unsigned const x0, unsigned const y0, unsigned const width, unsigned const height) {

    if ((x0 + width) > parent->width) {
        RL2_ERROR(TAG "empty sub pixel source");
        return NULL;
    }

    if ((y0 + height) > parent->height) {
        RL2_ERROR(TAG "empty sub pixel source");
        return NULL;
    }

    if (width == 0 || height == 0) {
        RL2_ERROR(TAG "empty sub pixel source");
        return NULL;
    }

    rl2_PixelSource const source = malloc(sizeof(*source));

    if (source == NULL) {
        RL2_ERROR(TAG "out of memory");
        return NULL;
    }

    source->width = width;
    source->height = height;
    source->pitch = parent->pitch;
    source->abgr = parent->abgr + y0 * parent->pitch + x0;
    source->parent = parent;

#ifdef RL2_BUILD_DEBUG
    char path[64];
    snprintf(path, sizeof(path), "Child of %p", (void*)parent);

    size_t const path_len = strlen(path);
    char* const path_dup = (char*)rl2_alloc(path_len + 1);
    source->path = path_dup;

    if (path_dup != NULL) {
        memcpy(path_dup, path, path_len + 1);
    }
#endif

    return source;
}

void rl2_destroyPixelSource(rl2_PixelSource const source) {
#ifdef RL2_BUILD_DEBUG
    rl2_free((void*)source->path);
#endif

    rl2_free(source);
}

unsigned rl2_pixelSourceWidth(rl2_PixelSource const source) {
    return source->width;
}

unsigned rl2_pixelSourceHeight(rl2_PixelSource const source) {
    return source->height;
}

rl2_ARGB8888 rl2_getPixel(rl2_PixelSource const source, unsigned const x, unsigned const y) {
    unsigned const width = source->width;
    unsigned const height = source->height;

    if (x < width && y < height) {
        return source->abgr[y * source->pitch + x];
    }

    RL2_WARN(TAG "pixel coordinates outside bounds: %u, %u", x, y);
    return 0;
}

void rl2_fillPixelSource(rl2_PixelSource const source, rl2_ARGB8888 const color) {
    unsigned const width = source->width;
    unsigned const height = source->height;
    size_t const pitch = source->pitch;
    rl2_ARGB8888* pixel = source->abgr;

    for (unsigned y = 0; y < height; y++) {
        rl2_ARGB8888 const* const saved_pixel = pixel;

        for (unsigned x = 0; x < width; x++) {
            *pixel++ = color;
        }

        pixel = (rl2_ARGB8888*)((uint8_t*)saved_pixel + pitch);
    }
}

void rl2_putPixel(rl2_PixelSource const source, unsigned const x, unsigned const y, rl2_ARGB8888 const color) {
    unsigned const width = source->width;
    unsigned const height = source->height;

    if (x < width && y < height) {
        source->abgr[y * source->pitch + x] = color;
        return;
    }

    RL2_WARN(TAG "pixel coordinates outside bounds: %u, %u", x, y);
}

#ifdef RL2_BUILD_DEBUG
char const* rl2_getPixelSourcePath(rl2_PixelSource const source) {
    return source->path;
}
#endif
