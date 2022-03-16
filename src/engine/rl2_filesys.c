#include "rl2_filesys.h"
#include "rl2_log.h"
#include "rl2_djb2.h"
#include "rl2_heap.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define TAG "FST "

typedef union {
  struct {
    uint8_t name[100];
    uint8_t mode[8];
    uint8_t owner[8];
    uint8_t group[8];
    uint8_t size[12];
    uint8_t modification[12];
    uint8_t checksum[8];
    uint8_t type;
    uint8_t linked[100];
  }
  header;
  
  uint8_t fill[512];
}
rl2_TarEntryV7;

typedef char rl2_staticAssertTarEntryV7Has512Bytes[sizeof(rl2_TarEntryV7) == 512 ? 1 : -1];

typedef struct {
    rl2_TarEntryV7 const* tar_entry;
    long size;
    rl2_Djb2Hash hash;
}
rl2_Entry;

struct rl2_Filesys {
    unsigned num_entries;
    rl2_Entry entries[1];
};

struct rl2_File {
    rl2_Entry const* entry;
    long pos;
};

static int rl2_compareEntries(void const* const ptr1, void const* const ptr2) {
    rl2_Entry const* const entry1 = ptr1;
    rl2_Entry const* const entry2 = ptr2;

    rl2_Djb2Hash const hash1 = entry1->hash;
    rl2_Djb2Hash const hash2 = entry2->hash;

    if (hash1 < hash2) {
        return -1;
    }
    else if (hash1 > hash2) {
        return 1;
    }

    char const* path1 = (char const*)entry1->tar_entry->header.name;
    char const* path2 = (char const*)entry2->tar_entry->header.name;

    return strcmp(path1, path2);
}

rl2_Filesys rl2_createFilesystem(void const* const buffer, size_t const size) {
    RL2_INFO(TAG "creating filesystem from buffer %p with size %zu", buffer, size);

    if ((size % 512) != 0) {
        RL2_ERROR("file system data must have a size multiple of 512 (%zu)", size);
        return NULL;
    }

    rl2_TarEntryV7 const* entry = buffer;
    rl2_TarEntryV7 const* const end = (rl2_TarEntryV7*)((uint8_t*)buffer + size);
    unsigned num_entries = 0;

    for (; entry < end && entry->header.name[0] != 0;) {
        RL2_DEBUG(TAG "processing entry %p", entry);

        uint8_t const* const name = entry->header.name;

        if (name[sizeof(entry->header.name) - 1] != 0) {
            int const length = (int)sizeof(entry->header.name);
            RL2_ERROR(TAG "entry name doesn't end with a nul character: \"%.*s\"", entry->header.name, length);
            return NULL;
        }

        char* endptr = NULL;
        long const entry_size = strtol((char const*)entry->header.size, &endptr, 8);

        if (entry->header.size[0] == 0 || *endptr != 0 || entry_size < 0 || errno == ERANGE) {
            RL2_ERROR(TAG "invalid size in file system entry \"%s\"", entry->header.name);
            return NULL;
        }

        entry += (entry_size + 511) / 512 + 1;
        num_entries++;
    }

    if (entry >= end) {
        RL2_ERROR(TAG "file system does not end with an empty entry");
        return NULL;
    }

    for (; entry < end; entry++) {
        for (size_t i = 0; i < sizeof(entry->fill); i++) {
            if (entry->fill[i] != 0) {
                RL2_ERROR(TAG "non-empty entry found at end of file system");
                return NULL;
            }
        }
    }

    RL2_INFO(TAG "file system has %u entries", num_entries);

    if (num_entries == 0) {
        RL2_WARN(TAG "empty file system");
    }

    size_t const entries_size = num_entries * sizeof(rl2_Entry);
    rl2_Filesys filesys = (rl2_Filesys)rl2_alloc(sizeof(*filesys) + entries_size - sizeof(filesys->entries[0]));

    if (filesys == NULL) {
        RL2_ERROR(TAG "out of memory creating file system");
        return NULL;
    }

    filesys->num_entries = num_entries;
    num_entries = 0;

    for (rl2_TarEntryV7 const* entry = buffer; num_entries < filesys->num_entries; num_entries++) {
        long const entry_size = strtol((char const*)entry->header.size, NULL, 8);
        rl2_Djb2Hash const hash = rl2_djb2((char const*)entry->header.name);

        filesys->entries[num_entries].tar_entry = entry;
        filesys->entries[num_entries].size = entry_size;
        filesys->entries[num_entries].hash = hash;

        RL2_DEBUG(
            TAG "file system entry %3u: size %8ld, hash " RL2_PRI_DJB2HASH ", path \"%s\"",
            num_entries + 1, entry_size, hash, entry->header.name
        );

        entry += (entry_size + 511) / 512 + 1;
    }

    qsort(filesys->entries, filesys->num_entries, sizeof(filesys->entries[0]), rl2_compareEntries);
    RL2_DEBUG(TAG "created file system from %p", buffer);
    return filesys;
}

void rl22_destroyFilesystem(rl2_Filesys const filesys) {
    RL2_INFO(TAG "destroying file system %p", filesys);
    rl2_free(filesys);
}

static rl2_Entry* rl2_fileFind(rl2_Filesys const filesys, char const* const path) {
    rl2_Entry key;
    key.tar_entry = (rl2_TarEntryV7*)path;
    key.hash = rl2_djb2(path);

    rl2_Entry* const found = bsearch(&key, filesys->entries, filesys->num_entries, sizeof(filesys->entries[0]), rl2_compareEntries);

    if (found == NULL) {
        RL2_WARN(TAG "could not find \"%s\" in file system %p", path, filesys);
    }
    else {
        RL2_DEBUG(
            TAG "found \"%s\" in file system %p, size %ld, hash " RL2_PRI_DJB2HASH,
            path, filesys, found->size, found->hash
        );
    }

    return found;
}

bool rl2_fileExists(rl2_Filesys const filesys, char const* const path) {
    rl2_Entry const* const found = rl2_fileFind(filesys, path);
    return found != NULL;
}

long rl2_fileSize(rl2_Filesys const filesys, char const* const path) {
    rl2_Entry const* const found = rl2_fileFind(filesys, path);

    if (found == NULL) {
        return -1;
    }

    return found->size;
}

rl2_File rl2_openFile(rl2_Filesys const filesys, char const* const path) {
    rl2_Entry const* const found = rl2_fileFind(filesys, path);

    if (found == NULL) {
        return NULL;
    }

    rl2_File file = rl2_alloc(sizeof(*file));

    if (file == NULL) {
        RL2_ERROR(TAG "out of memory opening file \"%s\"", path);
        return NULL;
    }

    file->entry = found;
    file->pos = 0;

    return file;
}

int rl2_seek(rl2_File const file, long const offset, int const whence) {
    long const size = file->entry->size;
    long pos = 0;

    switch (whence) {
        case SEEK_SET: pos = offset; break;
        case SEEK_CUR: pos = file->pos + offset; break;
        case SEEK_END: pos = size - offset; break;

        default: {
            RL2_ERROR(TAG "invalid base for seek: %d", whence);
            return -1;
        }
    }

    if (pos < 0 || pos > size) {
        RL2_ERROR(TAG "invalid position to seek to: %ld", pos);
        return -1;
    }

    file->pos = pos;
    return 0;
}

long rl2_tell(rl2_File const file) {
    return file->pos;
}

size_t rl2_read(rl2_File const file, void* const buffer, size_t const size) {
    rl2_Entry const* const entry = file->entry;
    size_t const available = entry->size - file->pos;
    size_t const to_read = size < available ? size : available;

    uint8_t const* const data = (uint8_t const*)entry->tar_entry;
    memcpy(buffer, data + 512 + file->pos, to_read);
    file->pos += to_read;

    return to_read;
}

void rl2_close(rl2_File const file) {
    rl2_free(file);
}
