#ifndef RL2_FILESYS_H__
#define RL2_FILESYS_H__

#include <stddef.h>
#include <stdbool.h>
#include <limits.h>

#define RL2_MAX_FSYS_HEIGHT UINT_MAX

typedef struct rl2_File* rl2_File;

// rl2_addFilesystem does **not** take ownership of buffer, keep it around until rl2_destroyFilesystem is called
bool rl2_addFilesystem(void const* const buffer, size_t const size);
void rl2_destroyFilesystem(void);

bool rl2_fileExists(char const* const path, unsigned const max_height);
long rl2_fileSize(char const* const path, unsigned const max_height);
rl2_File rl2_openFile(char const* const path, unsigned const max_height);
int rl2_seek(rl2_File const file, long const offset, int const whence);
long rl2_tell(rl2_File const file);
size_t rl2_read(rl2_File const file, void* const buffer, size_t const size);
void rl2_close(rl2_File const file);

#endif // RL2_FILESYS_H__
