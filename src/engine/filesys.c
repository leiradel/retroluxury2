#include "filesys.h"
#include "log.h"
#include "djb2.h"

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TAG "RIF "

typedef struct {
    char const* path;
    uint8_t const* data;
    long size;
    hh2_Djb2Hash hash;
}
hh2_Entry;

struct hh2_Filesys {
    uint8_t const* data;
    size_t size;
    unsigned num_entries;
    hh2_Entry entries[1];
};

struct hh2_File {
    uint8_t const* data;
    size_t size;
    size_t pos;
};

static void hh2_formatU8(uint8_t const u8, char string[static 5]) {
    // We don't want to use isprint to avoid issues with porting to baremetal
    // WARN(leiradel): ASCII only code
    int i = 0;

    if (u8 >= 32 && u8 < 127) {
        string[i++] = u8;
    }
    else {
        char const hex[17] = "0123456789abcedef";
        string[i++] = '0';
        string[i++] = 'x';
        string[i++] = hex[u8 / 16];
        string[i++] = hex[u8 % 16];
    }

    string[i] = 0;
}

static unsigned hh2_filesystemValidate(uint8_t const* const data, size_t const size) {
    uint8_t const* const end = data + size;
    uint8_t const* chunk = data + 12;
    unsigned count = 0;

    for (;;) {
        // Check if EOF reached
        if (chunk == end) {
            // RIFF must have at lest one entry
            if (count == 0) {
                HH2_LOG(HH2_LOG_ERROR, TAG "empty RIFF");
                return 0;
            }

            HH2_LOG(HH2_LOG_DEBUG, TAG "successfully reached the end of the RIFF buffer");
            return count;
        }

        // Check if we went past EOF
        if (chunk > end) {
            HH2_LOG(HH2_LOG_ERROR, TAG "detected chunk past end of the buffer");
            return 0;
        }

        // Check if there's enough space left for the chunk
        if (end - chunk < 8) {
            HH2_LOG(HH2_LOG_ERROR, TAG "not enough space left for another chunk");
            return 0;
        }

        // All chunks must be "FILE"
        if (chunk[0] != 'F' || chunk[1] != 'I' || chunk[2] != 'L' || chunk[3] != 'E') {
            char d0[5], d1[5], d2[5], d3[5];
            hh2_formatU8(chunk[0], d0);
            hh2_formatU8(chunk[1], d1);
            hh2_formatU8(chunk[2], d2);
            hh2_formatU8(chunk[3], d3);

            HH2_LOG(HH2_LOG_ERROR, TAG "invalid chunk %s %s %s %s", d0, d1, d2, d3);
            return 0;
        }

        HH2_LOG(HH2_LOG_DEBUG, TAG "validating chunk %u", count);

        uint32_t const chunk_size = chunk[4] | (uint32_t)chunk[5] << 8 | (uint32_t)chunk[6] << 16 | (uint32_t)chunk[7] << 24;

        uint16_t const path_len = chunk[8] | (uint16_t)chunk[9] << 8;
        uint32_t const total_path_len = (uint32_t)path_len + 2 + (path_len & 1); // including size and padding

        // Validate that the FILE chunk has a path and data (data could have 0 length though)
        if (total_path_len > chunk_size) {
            HH2_LOG(HH2_LOG_ERROR, TAG "invalid FILE chunk");
            return 0;
        }

        // Validate that the path is terminated with a nul
        if (chunk[10 + path_len - 1] != 0) {
            HH2_LOG(HH2_LOG_ERROR, TAG "path in FILE chunk is not nul terminated");
            return 0;
        }

        uint32_t const data_size = chunk_size - total_path_len;

        // Validate that the content size fits in a long
        if (data_size > UINT32_MAX) {
            HH2_LOG(HH2_LOG_ERROR, TAG "content size is too big: %" PRIu32, data_size);
            return 0;
        }

        uint32_t const total_chunk_size = chunk_size + 8 + (chunk_size & 1); // include id, size, and padding
        chunk += total_chunk_size; // total_chunk_size is at least 8 so we'll never be caught in an infinite loop
        count = count + 1;
    }
}

static void hh2_collectEntries(hh2_Filesys filesys) {
    uint8_t const* chunk = filesys->data + 12;
    unsigned const num_entries = filesys->num_entries;

    for (unsigned i = 0; i < num_entries; i++) {
        filesys->entries[i].path = (char const*)(chunk + 10);
        filesys->entries[i].hash = hh2_djb2(filesys->entries[i].path);

        HH2_LOG(
            HH2_LOG_DEBUG,
            TAG "entry %u with hash " HH2_PRI_DJB2HASH ": \"%s\"",
            i, filesys->entries[i].hash, filesys->entries[i].path
        );

        uint16_t const path_len = chunk[8] | (uint16_t)chunk[9] << 8;
        uint32_t const total_path_len = (uint32_t)path_len + 2 + (path_len & 1);

        filesys->entries[i].data = chunk + 8 + total_path_len;

        uint32_t const chunk_size = chunk[4] | (uint32_t)chunk[5] << 8 | (uint32_t)chunk[6] << 16 | (uint32_t)chunk[7] << 24;

        filesys->entries[i].size = chunk_size - total_path_len;

        if (filesys->entries[i].size == 0) {
            HH2_LOG(HH2_LOG_WARN, TAG "entry %u has no data", i);
        }

        uint32_t const total_chunk_size = chunk_size + 8 + (chunk_size & 1);
        chunk += total_chunk_size;
    }
}

static int hh2_compareEntries(void const* ptr1, void const* ptr2) {
    hh2_Entry const* const entry1 = ptr1;
    hh2_Entry const* const entry2 = ptr2;

    hh2_Djb2Hash const hash1 = entry1->hash;
    hh2_Djb2Hash const hash2 = entry2->hash;

    if (hash1 < hash2) {
        return -1;
    }
    else if (hash1 > hash2) {
        return 1;
    }

    char const* path1 = entry1->path;
    char const* path2 = entry2->path;

    // TODO leiradel: remove strcmp and use a NIH implementation
    return strcmp(path1, path2);
}

static hh2_Entry* hh2_fileFind(hh2_Filesys filesys, char const* path) {
    hh2_Entry key;
    key.path = path;
    key.hash = hh2_djb2(path);

    // TODO leiradel: remove bsearch and use a NIH implementation
    hh2_Entry* found = bsearch(&key, filesys->entries, filesys->num_entries, sizeof(filesys->entries[0]), hh2_compareEntries);

    if (found == NULL) {
        HH2_LOG(HH2_LOG_INFO, TAG "could not find \"%s\" in file system %p", path, filesys);
    }
    else {
        HH2_LOG(
            HH2_LOG_INFO, TAG "found \"%s\" in file system %p, data=%p, size=%ld, hash=" HH2_PRI_DJB2HASH,
            path, filesys, found->data, found->size, found->hash
        );
    }

    return found;
}

hh2_Filesys hh2_createFilesystem(void const* const buffer, size_t const size) {
    HH2_LOG(HH2_LOG_INFO, TAG "creating filesystem from buffer %p with size %zu", buffer, size);

    uint8_t const* data = buffer;

    // Validate buffer size
    if (size < 20) {
        // Sizes less than 20 can't contain the main chunk id (4 bytes), its size (4 bytes), the file id (4 bytes), and one empty
        // subchunk (id + size = 8 bytes)
        HH2_LOG(HH2_LOG_ERROR, TAG "buffer to small for a RIFF file: %zu", size);
        return NULL;
    }

    // Validate top-level chunk id
    if (data[0] != 'R' || data[1] != 'I' || data[2] != 'F' || data[3] != 'F') {
        // RIFF files must begin with "RIFF"
        char d0[5], d1[5], d2[5], d3[5];
        hh2_formatU8(data[0], d0);
        hh2_formatU8(data[1], d1);
        hh2_formatU8(data[2], d2);
        hh2_formatU8(data[3], d3);

        HH2_LOG(HH2_LOG_ERROR, TAG "buffer doesn't start with \"RIFF\": %s %s %s %s", d0, d1, d2, d3);
        return NULL;
    }

    // Validate main chunk size
    uint32_t const main_size = data[4] | (uint32_t)data[5] << 8 | (uint32_t)data[6] << 16 | (uint32_t)data[7] << 24;

    if (main_size + 8 != size) {
        // The main chunk size doesn't count for the id (4 bytes) and size (4 bytes)
        HH2_LOG(HH2_LOG_ERROR, TAG "main chunk size plus 8 must be equal to the buffer size");
        return NULL;
    }

    // Validate descriptor id
    if (data[8] != 'H' || data[9] != 'H' || data[10] != '2' || data[11] != ' ') {
        // Our file type is "HH2 "
        char d8[5], d9[5], d10[5], d11[5];
        hh2_formatU8(data[8], d8);
        hh2_formatU8(data[9], d9);
        hh2_formatU8(data[10], d10);
        hh2_formatU8(data[11], d11);

        HH2_LOG(HH2_LOG_ERROR, TAG "buffer id is not \"HH2 \": %s %s %s %s", d8, d9, d10, d11);
        return NULL;
    }

    // Validate structure
    unsigned const num_entries = hh2_filesystemValidate(data, size);
    HH2_LOG(HH2_LOG_DEBUG, TAG "RIFF file has %u entries", num_entries);

    if (num_entries == 0) {
        // Error already logged
        return NULL;
    }

    hh2_Filesys const filesys = malloc(sizeof(*filesys) + sizeof(filesys->entries[0]) * (num_entries - 1));

    if (filesys == NULL) {
        HH2_LOG(HH2_LOG_ERROR, TAG "out of memory");
        return NULL;
    }

    filesys->data = buffer;
    filesys->size = size;
    filesys->num_entries = num_entries;

    hh2_collectEntries(filesys);

    // TODO leiradel: remove qsort and use a NIH implementation
    qsort(filesys->entries, filesys->num_entries, sizeof(filesys->entries[0]), hh2_compareEntries);
    HH2_LOG(HH2_LOG_DEBUG, TAG "created file system %p", filesys);
    return filesys;
}

void hh2_destroyFilesystem(hh2_Filesys filesys) {
    HH2_LOG(HH2_LOG_INFO, TAG "destroying file system %p", filesys);
    free(filesys);
}

bool hh2_fileExists(hh2_Filesys filesys, char const* path) {
    hh2_Entry const* const found = hh2_fileFind(filesys, path);
    return found != NULL;
}

long hh2_fileSize(hh2_Filesys filesys, char const* path) {
    hh2_Entry const* const found = hh2_fileFind(filesys, path);

    if (found == NULL) {
        return -1;
    }

    return found->size;
}

hh2_File hh2_openFile(hh2_Filesys filesys, char const* path) {
    hh2_Entry const* const found = hh2_fileFind(filesys, path);

    if (found == NULL) {
        return NULL;
    }

    hh2_File file = malloc(sizeof(*file));

    if (file == NULL) {
        HH2_LOG(HH2_LOG_ERROR, TAG "out of memory");
        return NULL;
    }

    file->data = found->data;
    file->size = found->size;
    file->pos = 0;

    return file;
}

int hh2_seek(hh2_File file, long offset, int whence) {
    long pos = 0;

    switch (whence) {
        case SEEK_SET: pos = offset; break;
        case SEEK_CUR: pos = file->pos + offset; break;
        case SEEK_END: pos = file->size - offset; break;

        default: {
            HH2_LOG(HH2_LOG_ERROR, TAG "invalid base for seek: %d", whence);
            return -1;
        }
    }

    if (pos < 0 || pos > file->size) {
        HH2_LOG(HH2_LOG_ERROR, TAG "invalid position to seek to: %ld", pos);
        return -1;
    }

    file->pos = pos;
    return 0;
}

long hh2_tell(hh2_File file) {
    return file->pos;
}

size_t hh2_read(hh2_File file, void* buffer, size_t size) {
    size_t const available = file->size - file->pos;
    size_t const toread = size < available ? size : available;

    memcpy(buffer, file->data + file->pos, toread);
    file->pos += toread;

    return toread;
}

void hh2_close(hh2_File file) {
    free(file);
}
