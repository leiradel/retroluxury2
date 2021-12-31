#include "pixelsrc.h"
#include "log.h"

#include <png.h>
#include <jpeglib.h>

#include <stdlib.h>
#include <string.h>

#define TAG "PXS "

// Image read is coded for 32 bpp, make sure the build fails if hh2_ARGB8888 does not have 32 bits
typedef char hh2_staticAssertPixelMustHave32Bits[sizeof(hh2_ARGB8888) == 4 ? 1 : -1];

struct hh2_PixelSource {
    unsigned width;
    unsigned height;
    size_t pitch;
    hh2_PixelSource parent;

#ifdef HH2_DEBUG
    char const* path;
#endif

    hh2_ARGB8888* abgr;
    hh2_ARGB8888 data[1];
};

typedef struct {
    hh2_File file;
    void const* data;
    size_t size;
    size_t pos;
}
hh2_Reader;

static size_t hh2_readFromReader(hh2_Reader* const reader, void* buffer, size_t size) {
    if (reader->file != NULL) {
        return hh2_read(reader->file, buffer, size);
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

static void hh2_pngError(png_structp const png, png_const_charp const error) {
    (void)png;
    HH2_LOG(HH2_LOG_ERROR, TAG "error reading PNG: %s", error);
}

static void hh2_pngWarn(png_structp const png, png_const_charp const error) {
    (void)png;
    HH2_LOG(HH2_LOG_WARN, TAG "warning reading PNG: %s", error);
}

static void hh2_pngRead(png_structp const png, png_bytep const buffer, size_t const count) {
    hh2_Reader* const reader = png_get_io_ptr(png);
    hh2_readFromReader(reader, buffer, count);
}

static hh2_PixelSource hh2_readPng(hh2_Reader* const reader) {
    hh2_PixelSource source = NULL;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, hh2_pngError, hh2_pngWarn);

    if (png == NULL) {
        return NULL;
    }

    png_infop info = png_create_info_struct(png);

    if (info == NULL) {
        png_destroy_read_struct(&png, NULL, NULL);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png))) {
        if (source != NULL) {
            free(source);
        }

        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    png_set_read_fn(png, reader, hh2_pngRead);
    png_read_info(png, info);

    png_uint_32 width, height;
    int bit_depth, color_type;
    png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    size_t const num_pixels = width * height;
    source = malloc(sizeof(*source) + sizeof(source->data[0]) * (num_pixels - 1));

    if (source == NULL) {
        HH2_LOG(HH2_LOG_ERROR, TAG "out of memory");
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
    hh2_Reader* reader;
    uint8_t buffer[4096];
}
hh2_jpegReader;

typedef struct {
  struct jpeg_error_mgr pub;
  jmp_buf rollback;
}
hh2_jpegError;

static void hh2_jpegDummy(j_decompress_ptr cinfo) {}

static boolean hh2_jpegFill(j_decompress_ptr cinfo) {
    hh2_jpegReader* reader = (hh2_jpegReader*)cinfo->src;
    reader->pub.bytes_in_buffer = hh2_readFromReader(reader->reader, reader->buffer, sizeof(reader->buffer));
    reader->pub.next_input_byte = reader->buffer;
    return TRUE;
}

static void hh2_jpegSkip(j_decompress_ptr cinfo, long num_bytes) {
    hh2_jpegReader* reader = (hh2_jpegReader*)cinfo->src;

    if (num_bytes <= reader->pub.bytes_in_buffer) {
        reader->pub.bytes_in_buffer -= num_bytes;
        reader->pub.next_input_byte += num_bytes;
        return;
    }

    num_bytes -= reader->pub.bytes_in_buffer;

    while (num_bytes != 0) {
        size_t const to_read = num_bytes <= sizeof(reader->buffer) ? num_bytes : sizeof(reader->buffer);
        size_t const num_read = hh2_readFromReader(reader->reader, reader->buffer, to_read);
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

static void hh2_jpegExit(j_common_ptr cinfo) {
    hh2_jpegError* error = (hh2_jpegError*)cinfo->err;
    error->pub.output_message(cinfo);
    longjmp(error->rollback, 1);
}

static void hh2_jpegErr(j_common_ptr cinfo) {
    char buffer[JMSG_LENGTH_MAX];

    cinfo->err->format_message(cinfo, buffer);
    HH2_LOG(HH2_LOG_ERROR, TAG "error reading JPEG: %s", buffer);
}

static hh2_PixelSource hh2_readJpeg(hh2_Reader* const the_reader) {
    struct jpeg_decompress_struct cinfo;
    memset(&cinfo, 0, sizeof(cinfo));

    hh2_jpegError error;
    memset(&error, 0, sizeof(error));

    cinfo.err = jpeg_std_error(&error.pub);
    error.pub.error_exit = hh2_jpegExit;
    error.pub.output_message = hh2_jpegErr;

    if (setjmp(error.rollback)) {
        jpeg_destroy_decompress(&cinfo);
        return 0;
    }

    hh2_jpegReader reader;
    memset(&reader, 0, sizeof(reader));

    reader.reader = the_reader;
    reader.pub.init_source = hh2_jpegDummy;
    reader.pub.fill_input_buffer = hh2_jpegFill;
    reader.pub.skip_input_data = hh2_jpegSkip;
    reader.pub.resync_to_restart = jpeg_resync_to_restart; // default
    reader.pub.term_source = hh2_jpegDummy;
    reader.pub.bytes_in_buffer = 0;
    reader.pub.next_input_byte = NULL;

    jpeg_create_decompress(&cinfo);
    cinfo.src = (struct jpeg_source_mgr*)&reader;

    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_EXT_RGBX;

    jpeg_start_decompress(&cinfo);

    JDIMENSION width = cinfo.output_width;
    JDIMENSION height = cinfo.output_height;
    size_t const num_pixels = width * height;
    hh2_PixelSource const source = malloc(sizeof(*source) + sizeof(source->data[0]) * (num_pixels - 1));

    if (source == NULL) {
        HH2_LOG(HH2_LOG_ERROR, TAG "out of memory");
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

static bool hh2_isPng(void const* const header) {
    static uint8_t const png_header[8] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    return memcmp(header, png_header, 8) == 0;
}

hh2_PixelSource hh2_initPixelSource(void const* data, size_t size) {

    hh2_Reader reader;
    reader.file = NULL;
    reader.data = data;
    reader.size = size;
    reader.pos = 0;

    hh2_PixelSource const source = hh2_isPng(data) ? hh2_readPng(&reader) : hh2_readJpeg(&reader);

#ifdef HH2_DEBUG
    source->path = NULL;
#endif

    return source;
}

hh2_PixelSource hh2_readPixelSource(hh2_Filesys const filesys, char const* const path) {
    hh2_File const file = hh2_openFile(filesys, path);

    if (file == NULL) {
        // Error already logged
        return NULL;
    }

    uint8_t header[8];

    if (hh2_read(file, header, 8) != 8) {
        HH2_LOG(HH2_LOG_ERROR, TAG "error reading from image \"%s\"", path);
        hh2_close(file);
        return NULL;
    }

    hh2_seek(file, 0, SEEK_SET);

    hh2_Reader reader;
    reader.file = file;
    reader.data = NULL;
    reader.size = 0;
    reader.pos = 0;

    hh2_PixelSource const source = hh2_isPng(header) ? hh2_readPng(&reader) : hh2_readJpeg(&reader);
    hh2_close(file);

#ifdef HH2_DEBUG
    if (source != NULL) {
        size_t const path_len = strlen(path);
        char* const path_dup = (char*)malloc(path_len + 1);
        source->path = path_dup;

        if (path_dup != NULL) {
            memcpy(path_dup, path, path_len + 1);
        }
    }
#endif

    return source;
}

hh2_PixelSource hh2_subPixelSource(hh2_PixelSource const parent, unsigned const x0, unsigned const y0, unsigned const width, unsigned const height) {
    if ((x0 + width) > parent->width) {
        HH2_LOG(HH2_LOG_ERROR, TAG "empty sub pixel source");
        return NULL;
    }

    if ((y0 + height) > parent->height) {
        HH2_LOG(HH2_LOG_ERROR, TAG "empty sub pixel source");
        return NULL;
    }

    if (width == 0 || height == 0) {
        HH2_LOG(HH2_LOG_ERROR, TAG "empty sub pixel source");
        return NULL;
    }

    hh2_PixelSource const source = malloc(sizeof(*source));

    if (source == NULL) {
        HH2_LOG(HH2_LOG_ERROR, TAG "out of memory");
        return NULL;
    }

    source->width = width;
    source->height = height;
    source->pitch = parent->pitch;
    source->abgr = parent->abgr + y0 * parent->pitch + x0;
    source->parent = parent;

#ifdef HH2_DEBUG
    char path[64];
    snprintf(path, sizeof(path), "Child of %p", (void*)parent);

    size_t const path_len = strlen(path);
    char* const path_dup = (char*)malloc(path_len + 1);
    source->path = path_dup;

    if (path_dup != NULL) {
        memcpy(path_dup, path, path_len + 1);
    }
#endif

    return source;
}

void hh2_destroyPixelSource(hh2_PixelSource const source) {
#ifdef HH2_DEBUG
    free((void*)source->path);
#endif

    free(source);
}

unsigned hh2_pixelSourceWidth(hh2_PixelSource const source) {
    return source->width;
}

unsigned hh2_pixelSourceHeight(hh2_PixelSource const source) {
    return source->height;
}

hh2_ARGB8888 hh2_getPixel(hh2_PixelSource const source, unsigned const x, unsigned const y) {
    if (x < source->width && y < source->height) {
        return source->abgr[y * source->pitch + x];
    }

    return 0;
}

#ifdef HH2_DEBUG
char const* hh2_getPixelSourcePath(hh2_PixelSource source) {
    return source->path;
}
#endif
