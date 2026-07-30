/*
----------------------------------------------------------------
Contents:
This file implements virtual filesystem, allowing application to manage files in flexible and portable way.
Also provides portable cross platform mmap wrapper.

----------------------------------------------------------------
Code info:
- dvfs prefix
- DEMIURG_VIRTUAL_FILESYSTEM_IMPL macro to build
- User must pick target OS system by using of the macros below:
    - DEMIURG_VIRTUAL_FILESYSTEM_LINUX
    - DEMIURG_VIRTUAL_FILESYSTEM_WINDOWS

----------------------------------------------------------------
Usage:
- Create virtual filesystem with dvfs_create_filesystem. Provide info mappings, virtual mounting slot to system path.
- Check filesystem files status via dvfs_filesystem_check
- Translate filesystem paths to system path via dvfs_filesystem_system, do file operations with standard stdio
- Pool file changes with dvfs_filesystem_update

----------------------------------------------------------------
Virtual Paths:
- Shall use right slashes '/' to mark a directory
- Shall include file extensions
- Shall begin with mount slot
- Example valid path: "assets/fruit_textures/apple/red_apple.png"

----------------------------------------------------------------
Depedencies:
- each OS have own compilation requirements:
    - DEMIURG_VIRTUAL_FILESYSTEM_LINUX
        - none beyond libc (inotify, mmap, dirent are all libc/syscall wrappers, no extra -l needed)
    - DEMIURG_VIRTUAL_FILESYSTEM_WINDOWS
        - kernel32 (linked by default on Windows, no extra libs to add)
*/

#ifndef DEMIURG_VIRTUAL_FILESYSTEM_H
#define DEMIURG_VIRTUAL_FILESYSTEM_H

// ===========================
// Header Depedency

#include <stdint.h>
#include <stddef.h>

// ===========================
// Typedefs

typedef enum dvfs_status {          // Errors returned in declaration order
    dvfs_status_okay = 0,           // File can be read
    dvfs_status_bad_filepath,       // Given filepath is incorrect (bad mount, bad syntax, etc)
    dvfs_status_not_accessible,     // The file is outside mounted filesystem (for safety shall not be read)
    dvfs_status_not_existent,       // File does not exists
    dvfs_status_directory,          // Given filepath points to directory and not file
} dvfs_status;

typedef enum dvfs_change {
    dvfs_change_created,            // File was just created
    dvfs_change_removed,            // File was just removed
    dvfs_change_modified            // File was modified
} dvfs_change;

typedef struct dvfs_mount_info {
    int         watch_changes;      // Whethet to watch over changes in directory
    const char* mount_name;         // Shall be single word like "assets" or "temp"
    const char* system_path;        // System path like "/home/program/assets" or "C:/temp"
} dvfs_mount_info;

typedef struct dvfs_change_info {
    dvfs_change change;             // Change type
    const char* filepath;           // Filepath inside virtual filesystem
} dvfs_change_info;

// ===========================
// Filesystem

typedef struct dvfs_filesystem_create_info {
    uint32_t            mounted_directories_count;
    dvfs_mount_info*    mounted_directories;
} dvfs_filesystem_create_info;

typedef struct dvfs_filesystem dvfs_filesystem;
dvfs_filesystem* dvfs_create_filesystem(const dvfs_filesystem_create_info*);
void dvfs_free_filesystem(dvfs_filesystem*);

dvfs_status dvfs_filesystem_check(  // Check file status in filesystem
    dvfs_filesystem*, const char* filepath
);

int dvfs_filesystem_system( // Translates virtual to system path, saves to given buffer, returns non-zero if path fitted (with terminating zero)
    dvfs_filesystem*, const char* filepath, size_t system_path_buffer_bytes, char* system_path_buffer
);

void dvfs_filesystem_update( // Pools changes on watched mount directories, out_infos are filesystem owned, valid till next update
    dvfs_filesystem*, uint32_t* out_count, const dvfs_change_info** out_infos
);
 
// ===========================
// File Map
 
typedef struct dvfs_file_mmap_registry dvfs_file_mmap_registry;
dvfs_file_mmap_registry* dvfs_create_file_mmap_registry();
void dvfs_free_file_mmap_registry();
 
int dvfs_file_mmap_registry_map( // Maps file to addressing space, allowing zero-copy read from file, non-zero at success
    dvfs_file_mmap_registry*, const char* system_filepath, uint64_t* out_bytes, const unsigned char** out_mapped
);
 
void dvfs_file_mmap_registry_unmap( // Unmaps mapped file from addresing space, must be given out_mapped from dvfs_file_mmap
    dvfs_file_mmap_registry*, const char* mapped
);
 
#endif // DEMIURG_VIRTUAL_FILESYSTEM_H

#ifdef DEMIURG_VIRTUAL_FILESYSTEM_IMPL


#endif // DEMIURG_VIRTUAL_FILESYSTEM_IMPL
