#ifndef RL2_FILESYS_H__
#define RL2_FILESYS_H__

#include <stddef.h>
#include <stdbool.h>

typedef struct rl2_Filesys* rl2_Filesys;
typedef struct rl2_File* rl2_File;

// rl2_createFilesystem does **not** take ownership of buffer, keep it around until rl2_destroyFilesystem is called
rl2_Filesys rl2_createFilesystem(void const* const buffer, size_t const size);
void rl2_destroyFilesystem(rl2_Filesys const filesys);

bool rl2_fileExists(rl2_Filesys const filesys, char const* const path);
long rl2_fileSize(rl2_Filesys const filesys, char const* const path);
rl2_File rl2_openFile(rl2_Filesys const filesys, char const* const path);
int rl2_seek(rl2_File const file, long const offset, int const whence);
long rl2_tell(rl2_File const file);
size_t rl2_read(rl2_File const file, void* const buffer, size_t const size);
void rl2_close(rl2_File const file);

#endif // RL2_FILESYS_H__
