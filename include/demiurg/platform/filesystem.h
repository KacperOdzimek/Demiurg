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
void dvfs_free_file_mmap_registry(dvfs_file_mmap_registry*);

int dvfs_file_mmap_registry_map( // Maps file to addressing space, allowing zero-copy read from file, non-zero at success
    dvfs_file_mmap_registry*, const char* system_filepath, uint64_t* out_bytes, const unsigned char** out_mapped
);

void dvfs_file_mmap_registry_unmap( // Unmaps mapped file from addresing space, must be given out_mapped from dvfs_file_mmap
    dvfs_file_mmap_registry*, const char* mapped
);

#endif // DEMIURG_VIRTUAL_FILESYSTEM_H
#ifdef DEMIURG_VIRTUAL_FILESYSTEM_IMPL

/*
Implementation notes:
- Recursive directory watching:
    - Linux: inotify only watches a single directory non-recursively,
      so the implementation walks the directory tree at watch-setup time
      and installs one watch per directory, and installs additional
      watches on the fly whenever a new subdirectory is created.
    - Windows: ReadDirectoryChangesW supports recursive subtree watching
      natively via the bWatchSubtree flag, so a single watch (running on
      its own worker thread per mount) is enough.
*/

#ifdef DEMIURG_VIRTUAL_FILESYSTEM_LINUX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// ===========================
// Internal types

typedef struct mount {
    char mount_name[64];
    char system_path[PATH_MAX];
    int  watch_changes;
} mount;

typedef struct watch_node {
    int                 wd;
    uint32_t            mount_index;
    char                rel_path[PATH_MAX];     // virtual path relative to mount root, "" for mount root, no leading/trailing slash
    struct watch_node*  next;
} watch_node;

struct dvfs_filesystem {
    mount*              mounts;
    uint32_t            mount_count;

    int                 inotify_fd;
    watch_node*         watches;

    dvfs_change_info*   change_buffer;
    char**              change_buffer_strings;
    uint32_t            change_buffer_count;
    uint32_t            change_buffer_cap;
};

// ===========================
// Path helpers

static mount* find_mount(dvfs_filesystem* fs, const char* filepath, const char** out_rest) {
    const char* slash = strchr(filepath, '/');
    size_t name_len = slash ? (size_t)(slash - filepath) : strlen(filepath);

    for (uint32_t i = 0; i < fs->mount_count; ++i) {
        size_t mlen = strlen(fs->mounts[i].mount_name);
        if (mlen == name_len && strncmp(fs->mounts[i].mount_name, filepath, name_len) == 0) {
            *out_rest = slash ? slash + 1 : "";
            return &fs->mounts[i];
        }
    }
    return NULL;
}

// Rejects any ".." path component, so paths can never escape the mount root.
static int path_is_safe(const char* rest) {
    const char* p = rest;
    while (*p) {
        if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\0') && (p == rest || p[-1] == '/')) {
            return 0;
        } p++;
    }
    return 1;
}

static int build_system_path(mount* m, const char* rest, char* buf, size_t buf_size) {
    if (rest[0] == '\0') return snprintf(buf, buf_size, "%s", m->system_path);
    return snprintf(buf, buf_size, "%s/%s", m->system_path, rest);
}

// ===========================
// Recursive watch setup

static void add_watch_recursive(dvfs_filesystem* fs, uint32_t mount_index, const char* rel_path) {
    if (fs->inotify_fd < 0) return;

    mount* m = &fs->mounts[mount_index];
    char sys_path[PATH_MAX];
    if (rel_path[0] == '\0') snprintf(sys_path, sizeof(sys_path), "%s", m->system_path);
    else                     snprintf(sys_path, sizeof(sys_path), "%s/%s", m->system_path, rel_path);

    int wd = inotify_add_watch(fs->inotify_fd, sys_path,
        IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE | IN_DELETE_SELF);
    if (wd < 0) return; // directory may have vanished, or be unreadable; skip watching it

    watch_node* node = (watch_node*)malloc(sizeof(watch_node));
    node->wd = wd;
    node->mount_index = mount_index;
    snprintf(node->rel_path, sizeof(node->rel_path), "%s", rel_path);
    node->next = fs->watches;
    fs->watches = node;

    DIR* dir = opendir(sys_path);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char child_sys[PATH_MAX];
        snprintf(child_sys, sizeof(child_sys), "%s/%s", sys_path, entry->d_name);

        struct stat st;
        if (stat(child_sys, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            char child_rel[PATH_MAX];
            if (rel_path[0] == '\0') snprintf(child_rel, sizeof(child_rel), "%s", entry->d_name);
            else                     snprintf(child_rel, sizeof(child_rel), "%s/%s", rel_path, entry->d_name);
            add_watch_recursive(fs, mount_index, child_rel);
        }
    }
    closedir(dir);
}

static watch_node* find_watch(dvfs_filesystem* fs, int wd) {
    for (watch_node* n = fs->watches; n; n = n->next) {
        if (n->wd == wd) return n;
    }
    return NULL;
}

static void remove_watch_node(dvfs_filesystem* fs, int wd) {
    watch_node** pp = &fs->watches;
    while (*pp) {
        if ((*pp)->wd == wd) {
            watch_node* dead = *pp;
            *pp = dead->next;
            free(dead);
            return;
        }
        pp = &(*pp)->next;
    }
}

static void push_change(dvfs_filesystem* fs, dvfs_change change, const char* filepath) {
    if (fs->change_buffer_count == fs->change_buffer_cap) {
        uint32_t new_cap = fs->change_buffer_cap ? fs->change_buffer_cap * 2 : 16;
        fs->change_buffer = (dvfs_change_info*)realloc(fs->change_buffer, new_cap * sizeof(dvfs_change_info));
        fs->change_buffer_strings = (char**)realloc(fs->change_buffer_strings, new_cap * sizeof(char*));
        fs->change_buffer_cap = new_cap;
    }

    char* copy = strdup(filepath);
    fs->change_buffer_strings[fs->change_buffer_count] = copy;
    fs->change_buffer[fs->change_buffer_count].change = change;
    fs->change_buffer[fs->change_buffer_count].filepath = copy;
    fs->change_buffer_count++;
}

// ===========================
// Filesystem API

dvfs_filesystem* dvfs_create_filesystem(const dvfs_filesystem_create_info* info) {
    dvfs_filesystem* fs = (dvfs_filesystem*)calloc(1, sizeof(dvfs_filesystem));

    fs->mount_count = info->mounted_directories_count;
    fs->mounts = (mount*)calloc(fs->mount_count, sizeof(mount));
    for (uint32_t i = 0; i < fs->mount_count; ++i) {
        snprintf(fs->mounts[i].mount_name, sizeof(fs->mounts[i].mount_name), "%s", info->mounted_directories[i].mount_name);
        snprintf(fs->mounts[i].system_path, sizeof(fs->mounts[i].system_path), "%s", info->mounted_directories[i].system_path);
        fs->mounts[i].watch_changes = info->mounted_directories[i].watch_changes;
    }

    fs->inotify_fd = inotify_init1(IN_NONBLOCK);

    if (fs->inotify_fd >= 0) {
        for (uint32_t i = 0; i < fs->mount_count; ++i) {
            if (fs->mounts[i].watch_changes) add_watch_recursive(fs, i, "");
        }
    }

    return fs;
}

void dvfs_free_filesystem(dvfs_filesystem* fs) {
    if (!fs) return;

    watch_node* node = fs->watches;
    while (node) {
        watch_node* next = node->next;
        if (fs->inotify_fd >= 0) inotify_rm_watch(fs->inotify_fd, node->wd);
        free(node);
        node = next;
    }
    if (fs->inotify_fd >= 0) close(fs->inotify_fd);

    for (uint32_t i = 0; i < fs->change_buffer_count; ++i) free(fs->change_buffer_strings[i]);
    free(fs->change_buffer_strings);
    free(fs->change_buffer);

    free(fs->mounts);
    free(fs);
}

dvfs_status dvfs_filesystem_check(dvfs_filesystem* fs, const char* filepath) {
    const char* rest;
    mount* m = find_mount(fs, filepath, &rest);
    if (!m) return dvfs_status_bad_filepath;
    if (!path_is_safe(rest)) return dvfs_status_not_accessible;

    char sys_path[PATH_MAX];
    int n = build_system_path(m, rest, sys_path, sizeof(sys_path));
    if (n < 0 || (size_t)n >= sizeof(sys_path)) return dvfs_status_bad_filepath;

    struct stat st;
    if (stat(sys_path, &st) != 0) {
        if (errno == ENOENT || errno == ENOTDIR) return dvfs_status_not_existent;
        return dvfs_status_bad_filepath;
    }
    if (S_ISDIR(st.st_mode)) return dvfs_status_directory;
    return dvfs_status_okay;
}

int dvfs_filesystem_system(dvfs_filesystem* fs, const char* filepath, size_t system_path_buffer_bytes, char* system_path_buffer) {
    const char* rest;
    mount* m = find_mount(fs, filepath, &rest);
    if (!m || !path_is_safe(rest)) return 0;

    int n = build_system_path(m, rest, system_path_buffer, system_path_buffer_bytes);
    return (n >= 0 && (size_t)n < system_path_buffer_bytes);
}

void dvfs_filesystem_update(dvfs_filesystem* fs, uint32_t* out_count, const dvfs_change_info** out_infos) {
    for (uint32_t i = 0; i < fs->change_buffer_count; ++i) free(fs->change_buffer_strings[i]);
    fs->change_buffer_count = 0;

    if (fs->inotify_fd < 0) {
        *out_count = 0;
        *out_infos = fs->change_buffer;
        return;
    }

    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));

    for (;;) {
        ssize_t len = read(fs->inotify_fd, buf, sizeof(buf));
        if (len <= 0) break; // EAGAIN/EWOULDBLOCK (non-blocking fd) or nothing more to read

        ssize_t off = 0;
        while (off < len) {
            struct inotify_event* ev = (struct inotify_event*)(buf + off);
            watch_node* node = find_watch(fs, ev->wd);

            if (node) {
                mount* m = &fs->mounts[node->mount_index];
                char virtual_path[PATH_MAX];

                if (ev->len > 0) {
                    if (node->rel_path[0] == '\0') snprintf(virtual_path, sizeof(virtual_path), "%s/%s", m->mount_name, ev->name);
                    else snprintf(virtual_path, sizeof(virtual_path), "%s/%s/%s", m->mount_name, node->rel_path, ev->name);
                } else {
                    if (node->rel_path[0] == '\0') snprintf(virtual_path, sizeof(virtual_path), "%s", m->mount_name);
                    else snprintf(virtual_path, sizeof(virtual_path), "%s/%s", m->mount_name, node->rel_path);
                }

                if (ev->mask & (IN_CREATE | IN_MOVED_TO)) {
                    push_change(fs, dvfs_change_created, virtual_path);

                    if ((ev->mask & IN_ISDIR) && ev->len > 0) {
                        char child_rel[PATH_MAX];
                        if (node->rel_path[0] == '\0') snprintf(child_rel, sizeof(child_rel), "%s", ev->name);
                        else snprintf(child_rel, sizeof(child_rel), "%s/%s", node->rel_path, ev->name);
                        add_watch_recursive(fs, node->mount_index, child_rel); // start watching the new subdirectory too
                    }
                } else if (ev->mask & (IN_DELETE | IN_MOVED_FROM)) {
                    push_change(fs, dvfs_change_removed, virtual_path);
                } else if (ev->mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
                    push_change(fs, dvfs_change_modified, virtual_path);
                }

                if (ev->mask & (IN_DELETE_SELF | IN_IGNORED)) {
                    remove_watch_node(fs, ev->wd);
                }
            }

            off += (ssize_t)(sizeof(struct inotify_event) + ev->len);
        }
    }

    *out_count = fs->change_buffer_count;
    *out_infos = fs->change_buffer;
}

// ===========================
// File Map

typedef struct mmap_node {
    const unsigned char* ptr;
    uint64_t size;
    int fd;
    struct mmap_node* next;
} mmap_node;

struct dvfs_file_mmap_registry {
    mmap_node** buckets;
    size_t bucket_count;
};

static size_t hash_ptr(const void* p, size_t bucket_count) {
    uintptr_t v = (uintptr_t)p;
    v = (v >> 4) ^ (v >> 12) ^ (v >> 20); // mmap results are page aligned; spread the low bits out
    return (size_t)(v % bucket_count);
}

dvfs_file_mmap_registry* dvfs_create_file_mmap_registry() {
    dvfs_file_mmap_registry* reg = (dvfs_file_mmap_registry*)malloc(sizeof(dvfs_file_mmap_registry));
    reg->bucket_count = 61;
    reg->buckets = (mmap_node**)calloc(reg->bucket_count, sizeof(mmap_node*));
    return reg;
}

void dvfs_free_file_mmap_registry(dvfs_file_mmap_registry* reg) {
    if (!reg) return;

    for (size_t i = 0; i < reg->bucket_count; ++i) {
        mmap_node* n = reg->buckets[i];
        while (n) {
            mmap_node* next = n->next;
            munmap((void*)n->ptr, (size_t)n->size);
            close(n->fd);
            free(n);
            n = next;
        }
    }

    free(reg->buckets);
    free(reg);
}

int dvfs_file_mmap_registry_map(dvfs_file_mmap_registry* reg, const char* system_filepath, uint64_t* out_bytes, const unsigned char** out_mapped) {
    if (!reg) return 0;

    int fd = open(system_filepath, O_RDONLY);
    if (fd < 0) return 0;

    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size <= 0) { close(fd); return 0; }

    void* mapped = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) { close(fd); return 0; }

    mmap_node* node = (mmap_node*)malloc(sizeof(mmap_node));
    node->ptr = (const unsigned char*)mapped;
    node->size = (uint64_t)st.st_size;
    node->fd = fd;

    size_t bucket = hash_ptr(mapped, reg->bucket_count);
    node->next = reg->buckets[bucket];
    reg->buckets[bucket] = node;

    *out_bytes = node->size;
    *out_mapped = node->ptr;
    return 1;
}

void dvfs_file_mmap_registry_unmap(dvfs_file_mmap_registry* reg, const char* mapped) {
    if (!reg) return;

    size_t bucket = hash_ptr(mapped, reg->bucket_count);
    mmap_node** pp = &reg->buckets[bucket];

    while (*pp) {
        if ((const char*)(*pp)->ptr == mapped) {
            mmap_node* dead = *pp;
            *pp = dead->next;
            munmap((void*)dead->ptr, (size_t)dead->size);
            close(dead->fd);
            free(dead);
            return;
        }
        pp = &(*pp)->next;
    }
}

#endif // DEMIURG_VIRTUAL_FILESYSTEM_LINUX

#ifdef DEMIURG_VIRTUAL_FILESYSTEM_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================
// Internal types

typedef struct mount {
    char mount_name[64];
    char system_path[MAX_PATH];
    int  watch_changes;
} mount;

typedef struct change_node {
    dvfs_change change;
    char* filepath;
    struct change_node* next;
} change_node;

typedef struct watcher {
    HANDLE dir_handle;
    HANDLE stop_event;
    HANDLE thread_handle;
    struct dvfs_filesystem* fs;
    uint32_t mount_index;
} watcher;

struct dvfs_filesystem {
    mount* mounts;
    uint32_t     mount_count;

    watcher* watchers;
    uint32_t       watcher_count;

    CRITICAL_SECTION  queue_lock;   // guards queue_head/queue_tail, shared with watcher threads
    change_node* queue_head;
    change_node* queue_tail;

    dvfs_change_info* out_buffer;
    char**            out_strings;
    uint32_t          out_count;
    uint32_t          out_cap;
};

// ===========================
// Path helpers

static mount* find_mount(dvfs_filesystem* fs, const char* filepath, const char** out_rest) {
    const char* slash = strchr(filepath, '/');
    size_t name_len = slash ? (size_t)(slash - filepath) : strlen(filepath);

    for (uint32_t i = 0; i < fs->mount_count; ++i) {
        size_t mlen = strlen(fs->mounts[i].mount_name);
        if (mlen == name_len && strncmp(fs->mounts[i].mount_name, filepath, name_len) == 0) {
            *out_rest = slash ? slash + 1 : "";
            return &fs->mounts[i];
        }
    }
    return NULL;
}

// Rejects any ".." path component, so paths can never escape the mount root.
static int path_is_safe(const char* rest) {
    const char* p = rest;
    while (*p) {
        if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\0') && (p == rest || p[-1] == '/')) {
            return 0;
        }
        p++;
    }
    return 1;
}

static int build_system_path(mount* m, const char* rest, char* buf, size_t buf_size) {
    if (rest[0] == '\0') return snprintf(buf, buf_size, "%s", m->system_path);
    return snprintf(buf, buf_size, "%s/%s", m->system_path, rest);
}

// ===========================
// Recursive watching

static DWORD WINAPI watch_thread_proc(LPVOID param) {
    watcher* w = (watcher*)param;
    dvfs_filesystem* fs = w->fs;
    mount* m = &fs->mounts[w->mount_index];

    BYTE buffer[64 * 1024];

    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);

    for (;;) {
        DWORD bytes_returned = 0;
        BOOL ok = ReadDirectoryChangesW(
            w->dir_handle, buffer, sizeof(buffer), TRUE, // TRUE = watch subtree recursively
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytes_returned, &ov, NULL);

        if (!ok && GetLastError() != ERROR_IO_PENDING) break;

        HANDLE wait_handles[2];
        wait_handles[0] = ov.hEvent;
        wait_handles[1] = w->stop_event;
        DWORD wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);

        if (wait_result == WAIT_OBJECT_0 + 1) {
            CancelIoEx(w->dir_handle, &ov);
            break;
        }

        DWORD transferred = 0;
        if (!GetOverlappedResult(w->dir_handle, &ov, &transferred, TRUE) || transferred == 0) {
            ResetEvent(ov.hEvent);
            continue;
        }

        BYTE* p = buffer;
        for (;;) {
            FILE_NOTIFY_INFORMATION* info = (FILE_NOTIFY_INFORMATION*)p;

            // FileName here is already relative to the watched (mount) root, subdirectories included,
            // because bWatchSubtree is TRUE.
            char narrow_name[MAX_PATH * 2];
            int wlen = (int)(info->FileNameLength / sizeof(WCHAR));
            int nlen = WideCharToMultiByte(CP_UTF8, 0, info->FileName, wlen, narrow_name, (int)sizeof(narrow_name) - 1, NULL, NULL);
            if (nlen < 0) nlen = 0;
            narrow_name[nlen] = '\0';
            for (int i = 0; i < nlen; ++i) if (narrow_name[i] == '\\') narrow_name[i] = '/';

            char virtual_path[MAX_PATH * 2];
            snprintf(virtual_path, sizeof(virtual_path), "%s/%s", m->mount_name, narrow_name);

            dvfs_change change;
            switch (info->Action) {
                case FILE_ACTION_ADDED:
                case FILE_ACTION_RENAMED_NEW_NAME:
                    change = dvfs_change_created;
                    break;
                case FILE_ACTION_REMOVED:
                case FILE_ACTION_RENAMED_OLD_NAME:
                    change = dvfs_change_removed;
                    break;
                default: // FILE_ACTION_MODIFIED
                    change = dvfs_change_modified;
                    break;
            }

            change_node* node = (change_node*)malloc(sizeof(change_node));
            node->change = change;
            node->filepath = _strdup(virtual_path);
            node->next = NULL;

            EnterCriticalSection(&fs->queue_lock);
            if (fs->queue_tail) { fs->queue_tail->next = node; fs->queue_tail = node; }
            else                { fs->queue_head = fs->queue_tail = node; }
            LeaveCriticalSection(&fs->queue_lock);

            if (info->NextEntryOffset == 0) break;
            p += info->NextEntryOffset;
        }

        ResetEvent(ov.hEvent);
    }

    CloseHandle(ov.hEvent);
    return 0;
}

// ===========================
// Filesystem API

dvfs_filesystem* dvfs_create_filesystem(const dvfs_filesystem_create_info* info) {
    dvfs_filesystem* fs = (dvfs_filesystem*)calloc(1, sizeof(dvfs_filesystem));

    fs->mount_count = info->mounted_directories_count;
    fs->mounts = (mount*)calloc(fs->mount_count, sizeof(mount));
    for (uint32_t i = 0; i < fs->mount_count; ++i) {
        snprintf(fs->mounts[i].mount_name, sizeof(fs->mounts[i].mount_name), "%s", info->mounted_directories[i].mount_name);
        snprintf(fs->mounts[i].system_path, sizeof(fs->mounts[i].system_path), "%s", info->mounted_directories[i].system_path);
        fs->mounts[i].watch_changes = info->mounted_directories[i].watch_changes;
    }

    InitializeCriticalSection(&fs->queue_lock);

    uint32_t watch_count = 0;
    for (uint32_t i = 0; i < fs->mount_count; ++i) if (fs->mounts[i].watch_changes) watch_count++;

    if (watch_count > 0) {
        fs->watchers = (watcher*)calloc(watch_count, sizeof(watcher));

        for (uint32_t i = 0; i < fs->mount_count; ++i) {
            if (!fs->mounts[i].watch_changes) continue;

            watcher* w = &fs->watchers[fs->watcher_count];
            w->dir_handle = CreateFileA(
                fs->mounts[i].system_path,
                FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                NULL);

            if (w->dir_handle == INVALID_HANDLE_VALUE) continue; // skip mounts that can't be opened for watching

            w->stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
            w->fs = fs;
            w->mount_index = i;
            w->thread_handle = CreateThread(NULL, 0, watch_thread_proc, w, 0, NULL);
            fs->watcher_count++;
        }
    }

    return fs;
}

void dvfs_free_filesystem(dvfs_filesystem* fs) {
    if (!fs) return;

    for (uint32_t i = 0; i < fs->watcher_count; ++i) {
        watcher* w = &fs->watchers[i];
        SetEvent(w->stop_event);
        WaitForSingleObject(w->thread_handle, INFINITE);
        CloseHandle(w->thread_handle);
        CloseHandle(w->stop_event);
        CloseHandle(w->dir_handle);
    }
    free(fs->watchers);

    change_node* n = fs->queue_head;
    while (n) {
        change_node* next = n->next;
        free(n->filepath);
        free(n);
        n = next;
    }

    for (uint32_t i = 0; i < fs->out_count; ++i) free(fs->out_strings[i]);
    free(fs->out_strings);
    free(fs->out_buffer);

    DeleteCriticalSection(&fs->queue_lock);
    free(fs->mounts);
    free(fs);
}

dvfs_status dvfs_filesystem_check(dvfs_filesystem* fs, const char* filepath) {
    const char* rest;
    mount* m = find_mount(fs, filepath, &rest);
    if (!m) return dvfs_status_bad_filepath;
    if (!path_is_safe(rest)) return dvfs_status_not_accessible;

    char sys_path[MAX_PATH];
    int n = build_system_path(m, rest, sys_path, sizeof(sys_path));
    if (n < 0 || (size_t)n >= sizeof(sys_path)) return dvfs_status_bad_filepath;

    DWORD attrs = GetFileAttributesA(sys_path);
    if (attrs == INVALID_FILE_ATTRIBUTES) return dvfs_status_not_existent;
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) return dvfs_status_directory;
    return dvfs_status_okay;
}

int dvfs_filesystem_system(dvfs_filesystem* fs, const char* filepath, size_t system_path_buffer_bytes, char* system_path_buffer) {
    const char* rest;
    mount* m = find_mount(fs, filepath, &rest);
    if (!m || !path_is_safe(rest)) return 0;

    int n = build_system_path(m, rest, system_path_buffer, system_path_buffer_bytes);
    return (n >= 0 && (size_t)n < system_path_buffer_bytes);
}

void dvfs_filesystem_update(dvfs_filesystem* fs, uint32_t* out_count, const dvfs_change_info** out_infos) {
    for (uint32_t i = 0; i < fs->out_count; ++i) free(fs->out_strings[i]);
    fs->out_count = 0;

    EnterCriticalSection(&fs->queue_lock);
    change_node* n = fs->queue_head;
    fs->queue_head = fs->queue_tail = NULL;
    LeaveCriticalSection(&fs->queue_lock);

    while (n) {
        if (fs->out_count == fs->out_cap) {
            uint32_t new_cap = fs->out_cap ? fs->out_cap * 2 : 16;
            fs->out_buffer = (dvfs_change_info*)realloc(fs->out_buffer, new_cap * sizeof(dvfs_change_info));
            fs->out_strings = (char**)realloc(fs->out_strings, new_cap * sizeof(char*));
            fs->out_cap = new_cap;
        }

        fs->out_strings[fs->out_count] = n->filepath; // ownership transferred, freed next update()/free()
        fs->out_buffer[fs->out_count].change = n->change;
        fs->out_buffer[fs->out_count].filepath = n->filepath;
        fs->out_count++;

        change_node* next = n->next;
        free(n);
        n = next;
    }

    *out_count = fs->out_count;
    *out_infos = fs->out_buffer;
}

// ===========================
// File Map

typedef struct mmap_node {
    const unsigned char* ptr;
    uint64_t size;
    HANDLE file_handle;
    HANDLE mapping_handle;
    struct mmap_node* next;
} mmap_node;

struct dvfs_file_mmap_registry {
    mmap_node** buckets;
    size_t bucket_count;
};

static size_t hash_ptr(const void* p, size_t bucket_count) {
    uintptr_t v = (uintptr_t)p;
    v = (v >> 4) ^ (v >> 12) ^ (v >> 20); // mapped views are page aligned; spread the low bits out
    return (size_t)(v % bucket_count);
}

dvfs_file_mmap_registry* dvfs_create_file_mmap_registry() {
    dvfs_file_mmap_registry* reg = (dvfs_file_mmap_registry*)malloc(sizeof(dvfs_file_mmap_registry));
    reg->bucket_count = 61;
    reg->buckets = (mmap_node**)calloc(reg->bucket_count, sizeof(mmap_node*));
    return reg;
}

void dvfs_free_file_mmap_registry(dvfs_file_mmap_registry* reg) {
    if (!reg) return;

    for (size_t i = 0; i < reg->bucket_count; ++i) {
        mmap_node* n = reg->buckets[i];
        while (n) {
            mmap_node* next = n->next;
            UnmapViewOfFile((LPCVOID)n->ptr);
            CloseHandle(n->mapping_handle);
            CloseHandle(n->file_handle);
            free(n);
            n = next;
        }
    }

    free(reg->buckets);
    free(reg);
}

int dvfs_file_mmap_registry_map(dvfs_file_mmap_registry* reg, const char* system_filepath, uint64_t* out_bytes, const unsigned char** out_mapped) {
    if (!reg) return 0;

    HANDLE file_handle = CreateFileA(system_filepath, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE) return 0;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(file_handle, &size) || size.QuadPart == 0) {
        CloseHandle(file_handle);
        return 0;
    }

    HANDLE mapping_handle = CreateFileMappingA(file_handle, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!mapping_handle) {
        CloseHandle(file_handle);
        return 0;
    }

    void* mapped = MapViewOfFile(mapping_handle, FILE_MAP_READ, 0, 0, 0);
    if (!mapped) {
        CloseHandle(mapping_handle);
        CloseHandle(file_handle);
        return 0;
    }

    mmap_node* node = (mmap_node*)malloc(sizeof(mmap_node));
    node->ptr = (const unsigned char*)mapped;
    node->size = (uint64_t)size.QuadPart;
    node->file_handle = file_handle;
    node->mapping_handle = mapping_handle;

    size_t bucket = hash_ptr(mapped, reg->bucket_count);
    node->next = reg->buckets[bucket];
    reg->buckets[bucket] = node;

    *out_bytes = node->size;
    *out_mapped = node->ptr;
    return 1;
}

void dvfs_file_mmap_registry_unmap(dvfs_file_mmap_registry* reg, const char* mapped) {
    if (!reg) return;

    size_t bucket = hash_ptr(mapped, reg->bucket_count);
    mmap_node** pp = &reg->buckets[bucket];

    while (*pp) {
        if ((const char*)(*pp)->ptr == mapped) {
            mmap_node* dead = *pp;
            *pp = dead->next;
            UnmapViewOfFile((LPCVOID)dead->ptr);
            CloseHandle(dead->mapping_handle);
            CloseHandle(dead->file_handle);
            free(dead);
            return;
        }
        pp = &(*pp)->next;
    }
}

#endif // DEMIURG_VIRTUAL_FILESYSTEM_WINDOWS
#endif // DEMIURG_VIRTUAL_FILESYSTEM_IMPL
