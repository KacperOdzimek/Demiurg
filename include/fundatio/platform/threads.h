/*
----------------------------------------------------------------
Contents:
This file provides cross-platform C99 threading primitives: thread, mutex, condition variable,
read-write lock, semaphore, and barrier objects. Each primitive wraps the native OS backend (win32 or pthreads).

----------------------------------------------------------------
Code info:
- fnd_thr prefix
- FUNDATIO_THREADS_IMPL macro to build

----------------------------------------------------------------
Usage:
- Fill the matching create info struct, create the object, use it, then free it
- fnd_thr_create_thread begins execution immediately, before the call returns
- Wait/lock operations are O(1); all objects are individually heap-allocated

----------------------------------------------------------------
Notes:
- Objects are not safe to create/free concurrently with their own use from another thread
- Freeing a thread that was neither joined nor detached will block until it finishes
- timeout_ms of 0 in a *_wait_for call polls once and returns immediately
*/

#ifndef FUNDATIO_THREADS_H
#define FUNDATIO_THREADS_H

#include <stddef.h>
#include <stdint.h>

// ===========================
// Thread-Local Storage

#if defined(_MSC_VER)
    #define fnd_thr_thread_local __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
    #define fnd_thr_thread_local __thread
#else
    #define fnd_thr_thread_local _Thread_local // C11 fallback
#endif

// ===========================
// Thread

typedef int (*fnd_thr_entry_point)(void* user_data);

typedef struct fnd_thr_thread_create_info {
    fnd_thr_entry_point entry_point;
    void*           user_data;
    size_t          stack_size_bytes;   // 0 = platform default
    const char*     debug_name;         // optional, may be NULL
} fnd_thr_thread_create_info;

typedef struct fnd_thr_thread fnd_thr_thread;
fnd_thr_thread* fnd_thr_create_thread(const fnd_thr_thread_create_info*);
void fnd_thr_free_thread  (fnd_thr_thread*);

int  fnd_thr_join_thread  (fnd_thr_thread*, int* out_exit_code); // 0 = success
void fnd_thr_detach_thread(fnd_thr_thread*);
uint64_t fnd_thr_get_thread_id(const fnd_thr_thread*);

// ===========================
// Mutex

typedef struct fnd_thr_mutex_create_info {
    int recursive; // non-zero = recursive mutex
} fnd_thr_mutex_create_info;

typedef struct fnd_thr_mutex fnd_thr_mutex;
fnd_thr_mutex* fnd_thr_create_mutex(const fnd_thr_mutex_create_info*);
void fnd_thr_free_mutex(fnd_thr_mutex*);

void fnd_thr_mutex_lock(fnd_thr_mutex*);
int  fnd_thr_mutex_trylock(fnd_thr_mutex*); // 1 = acquired
void fnd_thr_mutex_unlock(fnd_thr_mutex*);

// ===========================
// Condition Variable

typedef struct fnd_thr_cond fnd_thr_cond;
fnd_thr_cond* fnd_thr_create_cond(void);
void fnd_thr_free_cond(fnd_thr_cond*);

void fnd_thr_cond_wait (fnd_thr_cond*, fnd_thr_mutex*);
int  fnd_thr_cond_wait_for(fnd_thr_cond*, fnd_thr_mutex*, uint64_t timeout_ms); // 0 = signaled, 1 = timed out
void fnd_thr_cond_signal (fnd_thr_cond*);
void fnd_thr_cond_broadcast(fnd_thr_cond*);

// ===========================
// Read-Write Lock

typedef struct fnd_thr_rwlock fnd_thr_rwlock;
fnd_thr_rwlock* fnd_thr_create_rwlock(void);
void fnd_thr_free_rwlock(fnd_thr_rwlock*);

void fnd_thr_rwlock_lock_read(fnd_thr_rwlock*);
void fnd_thr_rwlock_lock_write(fnd_thr_rwlock*);
void fnd_thr_rwlock_unlock_read(fnd_thr_rwlock*);
void fnd_thr_rwlock_unlock_write(fnd_thr_rwlock*);

// ===========================
// Semaphore

typedef struct fnd_thr_semaphore_create_info {
    uint32_t initial_count;
    uint32_t max_count; // 0 = unbounded (implementation defined ceiling)
} fnd_thr_semaphore_create_info;

typedef struct fnd_thr_semaphore fnd_thr_semaphore;
fnd_thr_semaphore* fnd_thr_create_semaphore(const fnd_thr_semaphore_create_info*);
void fnd_thr_free_semaphore(fnd_thr_semaphore*);

void fnd_thr_semaphore_wait(fnd_thr_semaphore*);
int  fnd_thr_semaphore_wait_for(fnd_thr_semaphore*, uint64_t timeout_ms); // 0 = acquired, 1 = timed out
void fnd_thr_semaphore_signal(fnd_thr_semaphore*, uint32_t count);

// ===========================
// Barrier

typedef struct fnd_thr_barrier_create_info {
    uint32_t participant_count;
} fnd_thr_barrier_create_info;
typedef struct fnd_thr_barrier fnd_thr_barrier;

fnd_thr_barrier* fnd_thr_create_barrier(const fnd_thr_barrier_create_info*);
void fnd_thr_free_barrier(fnd_thr_barrier*);

int fnd_thr_barrier_wait(fnd_thr_barrier*); // non-zero for exactly one participant (the serial thread)

// ===========================
// Misc

void fnd_thr_sleep_ms(uint64_t milliseconds);
void fnd_thr_yield(void);
uint64_t fnd_thr_current_thread_id(void);
uint32_t fnd_thr_hardware_concurrency(void);

#endif // FUNDATIO_THREADS_H

#ifdef FUNDATIO_THREADS_IMPL

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <process.h>
#else
    #include <pthread.h>
    #include <sched.h>
    #include <time.h>
    #include <unistd.h>
    #include <limits.h>
#endif

// ===========================
// Thread

struct fnd_thr_thread {
#if defined(_WIN32)
    HANDLE   handle;
    uint64_t id;
#else
    pthread_t handle;
#endif
    fnd_thr_entry_point entry_point;
    void*               user_data;
    int                 exit_code;
    int                 joined_or_detached;
};

#if defined(_WIN32)
static unsigned __stdcall fnd_thr_win32_trampoline(void* param) {
    fnd_thr_thread* thread = (fnd_thr_thread*)param;
    thread->exit_code = thread->entry_point(thread->user_data);
    return (unsigned)thread->exit_code;
}
#else
static void* fnd_thr_posix_trampoline(void* param) {
    fnd_thr_thread* thread = (fnd_thr_thread*)param;
    thread->exit_code = thread->entry_point(thread->user_data);
    return NULL;
}
#endif

fnd_thr_thread* fnd_thr_create_thread(const fnd_thr_thread_create_info* info) {
    fnd_thr_thread* thread = calloc(1, sizeof(fnd_thr_thread));
    if (!thread) return NULL;

    thread->entry_point = info->entry_point;
    thread->user_data   = info->user_data;

#if defined(_WIN32)
    unsigned int win32_id = 0;
    thread->handle = (HANDLE)_beginthreadex(
        NULL,
        (unsigned int)info->stack_size_bytes,
        fnd_thr_win32_trampoline,
        thread,
        0,
        &win32_id
    );
    thread->id = (uint64_t)win32_id;
    if (!thread->handle) goto _fail;
#else
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (info->stack_size_bytes > 0) pthread_attr_setstacksize(&attr, info->stack_size_bytes);

    int result = pthread_create(&thread->handle, &attr, fnd_thr_posix_trampoline, thread);
    pthread_attr_destroy(&attr);
    if (result != 0) goto _fail;
#endif

    (void)info->debug_name; // reserved for platform thread-naming hooks

    return thread;
_fail:
    free(thread);
    return NULL;
}

void fnd_thr_free_thread(fnd_thr_thread* thread) {
    if (thread == NULL) return;
    if (!thread->joined_or_detached) fnd_thr_join_thread(thread, NULL); // see Notes
    free(thread);
}

int fnd_thr_join_thread(fnd_thr_thread* thread, int* out_exit_code) {
#if defined(_WIN32)
    if (WaitForSingleObject(thread->handle, INFINITE) != WAIT_OBJECT_0) return 1;
    CloseHandle(thread->handle);
    thread->handle = NULL;
#else
    if (pthread_join(thread->handle, NULL) != 0) return 1;
#endif
    thread->joined_or_detached = 1;
    if (out_exit_code) *out_exit_code = thread->exit_code;
    return 0;
}

void fnd_thr_detach_thread(fnd_thr_thread* thread) {
#if defined(_WIN32)
    CloseHandle(thread->handle);
    thread->handle = NULL;
#else
    pthread_detach(thread->handle);
#endif
    thread->joined_or_detached = 1;
}

uint64_t fnd_thr_get_thread_id(const fnd_thr_thread* thread) {
#if defined(_WIN32)
    return thread->id;
#else
    return (uint64_t)(uintptr_t)thread->handle;
#endif
}

// ===========================
// Mutex

struct fnd_thr_mutex {
#if defined(_WIN32)
    CRITICAL_SECTION handle;
#else
    pthread_mutex_t handle;
#endif
};

fnd_thr_mutex* fnd_thr_create_mutex(const fnd_thr_mutex_create_info* info) {
    fnd_thr_mutex* mutex = calloc(1, sizeof(fnd_thr_mutex));
    if (!mutex) return NULL;

#if defined(_WIN32)
    InitializeCriticalSection(&mutex->handle); // recursive by nature
    (void)info;
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    if (info && info->recursive) pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    int result = pthread_mutex_init(&mutex->handle, &attr);
    pthread_mutexattr_destroy(&attr);
    if (result != 0) goto _fail;
#endif

    return mutex;
#if !defined(_WIN32)
_fail:
    free(mutex);
    return NULL;
#endif
}

void fnd_thr_free_mutex(fnd_thr_mutex* mutex) {
    if (mutex == NULL) return;
#if defined(_WIN32)
    DeleteCriticalSection(&mutex->handle);
#else
    pthread_mutex_destroy(&mutex->handle);
#endif
    free(mutex);
}

void fnd_thr_mutex_lock(fnd_thr_mutex* mutex) {
#if defined(_WIN32)
    EnterCriticalSection(&mutex->handle);
#else
    pthread_mutex_lock(&mutex->handle);
#endif
}

int fnd_thr_mutex_trylock(fnd_thr_mutex* mutex) {
#if defined(_WIN32)
    return TryEnterCriticalSection(&mutex->handle) != 0;
#else
    return pthread_mutex_trylock(&mutex->handle) == 0;
#endif
}

void fnd_thr_mutex_unlock(fnd_thr_mutex* mutex) {
#if defined(_WIN32)
    LeaveCriticalSection(&mutex->handle);
#else
    pthread_mutex_unlock(&mutex->handle);
#endif
}

// ===========================
// Condition Variable

struct fnd_thr_cond {
#if defined(_WIN32)
    CONDITION_VARIABLE handle;
#else
    pthread_cond_t handle;
#endif
};

fnd_thr_cond* fnd_thr_create_cond(void) {
    fnd_thr_cond* cond = calloc(1, sizeof(fnd_thr_cond));
    if (!cond) return NULL;

#if defined(_WIN32)
    InitializeConditionVariable(&cond->handle);
#else
    if (pthread_cond_init(&cond->handle, NULL) != 0) { free(cond); return NULL; }
#endif

    return cond;
}

void fnd_thr_free_cond(fnd_thr_cond* cond) {
    if (cond == NULL) return;
#if !defined(_WIN32)
    pthread_cond_destroy(&cond->handle);
#endif
    free(cond);
}

void fnd_thr_cond_wait(fnd_thr_cond* cond, fnd_thr_mutex* mutex) {
#if defined(_WIN32)
    SleepConditionVariableCS(&cond->handle, &mutex->handle, INFINITE);
#else
    pthread_cond_wait(&cond->handle, &mutex->handle);
#endif
}

int fnd_thr_cond_wait_for(fnd_thr_cond* cond, fnd_thr_mutex* mutex, uint64_t timeout_ms) {
#if defined(_WIN32)
    return SleepConditionVariableCS(&cond->handle, &mutex->handle, (DWORD)timeout_ms) ? 0 : 1;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec  += (time_t)(timeout_ms / 1000);
    ts.tv_nsec += (long)((timeout_ms % 1000) * 1000000);
    if (ts.tv_nsec >= 1000000000) { ts.tv_nsec -= 1000000000; ts.tv_sec += 1; }

    return (pthread_cond_timedwait(&cond->handle, &mutex->handle, &ts) == 0) ? 0 : 1;
#endif
}

void fnd_thr_cond_signal(fnd_thr_cond* cond) {
#if defined(_WIN32)
    WakeConditionVariable(&cond->handle);
#else
    pthread_cond_signal(&cond->handle);
#endif
}

void fnd_thr_cond_broadcast(fnd_thr_cond* cond) {
#if defined(_WIN32)
    WakeAllConditionVariable(&cond->handle);
#else
    pthread_cond_broadcast(&cond->handle);
#endif
}

// ===========================
// Read-Write Lock

struct fnd_thr_rwlock {
#if defined(_WIN32)
    SRWLOCK handle;
#else
    pthread_rwlock_t handle;
#endif
};

fnd_thr_rwlock* fnd_thr_create_rwlock(void) {
    fnd_thr_rwlock* rwlock = calloc(1, sizeof(fnd_thr_rwlock));
    if (!rwlock) return NULL;

#if defined(_WIN32)
    InitializeSRWLock(&rwlock->handle);
#else
    if (pthread_rwlock_init(&rwlock->handle, NULL) != 0) { free(rwlock); return NULL; }
#endif

    return rwlock;
}

void fnd_thr_free_rwlock(fnd_thr_rwlock* rwlock) {
    if (rwlock == NULL) return;
#if !defined(_WIN32)
    pthread_rwlock_destroy(&rwlock->handle);
#endif
    free(rwlock);
}

void fnd_thr_rwlock_lock_read(fnd_thr_rwlock* rwlock) {
#if defined(_WIN32)
    AcquireSRWLockShared(&rwlock->handle);
#else
    pthread_rwlock_rdlock(&rwlock->handle);
#endif
}

void fnd_thr_rwlock_lock_write(fnd_thr_rwlock* rwlock) {
#if defined(_WIN32)
    AcquireSRWLockExclusive(&rwlock->handle);
#else
    pthread_rwlock_wrlock(&rwlock->handle);
#endif
}

void fnd_thr_rwlock_unlock_read(fnd_thr_rwlock* rwlock) {
#if defined(_WIN32)
    ReleaseSRWLockShared(&rwlock->handle);
#else
    pthread_rwlock_unlock(&rwlock->handle);
#endif
}

void fnd_thr_rwlock_unlock_write(fnd_thr_rwlock* rwlock) {
#if defined(_WIN32)
    ReleaseSRWLockExclusive(&rwlock->handle);
#else
    pthread_rwlock_unlock(&rwlock->handle);
#endif
}

// ===========================
// Semaphore
// win32 uses the native kernel semaphore; posix uses a mutex+cond counter,
// since unnamed sem_t timed-waits are not reliably portable (notably on macOS)

struct fnd_thr_semaphore {
#if defined(_WIN32)
    HANDLE handle;
#else
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    uint32_t        count;
    uint32_t        max_count;
#endif
};

fnd_thr_semaphore* fnd_thr_create_semaphore(const fnd_thr_semaphore_create_info* info) {
    fnd_thr_semaphore* semaphore = calloc(1, sizeof(fnd_thr_semaphore));
    if (!semaphore) return NULL;

#if defined(_WIN32)
    LONG max_count = info->max_count > 0 ? (LONG)info->max_count : LONG_MAX;
    semaphore->handle = CreateSemaphoreW(NULL, (LONG)info->initial_count, max_count, NULL);
    if (!semaphore->handle) goto _fail;
#else
    if (pthread_mutex_init(&semaphore->mutex, NULL) != 0) goto _fail;
    if (pthread_cond_init(&semaphore->cond, NULL) != 0) { pthread_mutex_destroy(&semaphore->mutex); goto _fail; }
    semaphore->count     = info->initial_count;
    semaphore->max_count = info->max_count > 0 ? info->max_count : UINT32_MAX;
#endif

    return semaphore;
_fail:
    free(semaphore);
    return NULL;
}

void fnd_thr_free_semaphore(fnd_thr_semaphore* semaphore) {
    if (semaphore == NULL) return;
#if defined(_WIN32)
    CloseHandle(semaphore->handle);
#else
    pthread_cond_destroy(&semaphore->cond);
    pthread_mutex_destroy(&semaphore->mutex);
#endif
    free(semaphore);
}

void fnd_thr_semaphore_wait(fnd_thr_semaphore* semaphore) {
#if defined(_WIN32)
    WaitForSingleObject(semaphore->handle, INFINITE);
#else
    pthread_mutex_lock(&semaphore->mutex);
    while (semaphore->count == 0) pthread_cond_wait(&semaphore->cond, &semaphore->mutex);
    semaphore->count--;
    pthread_mutex_unlock(&semaphore->mutex);
#endif
}

int fnd_thr_semaphore_wait_for(fnd_thr_semaphore* semaphore, uint64_t timeout_ms) {
#if defined(_WIN32)
    return (WaitForSingleObject(semaphore->handle, (DWORD)timeout_ms) == WAIT_OBJECT_0) ? 0 : 1;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec  += (time_t)(timeout_ms / 1000);
    ts.tv_nsec += (long)((timeout_ms % 1000) * 1000000);
    if (ts.tv_nsec >= 1000000000) { ts.tv_nsec -= 1000000000; ts.tv_sec += 1; }

    pthread_mutex_lock(&semaphore->mutex);
    int timed_out = 0;
    while (semaphore->count == 0 && !timed_out) {
        timed_out = (pthread_cond_timedwait(&semaphore->cond, &semaphore->mutex, &ts) != 0);
    }
    if (!timed_out) semaphore->count--;
    pthread_mutex_unlock(&semaphore->mutex);
    return timed_out ? 1 : 0;
#endif
}

void fnd_thr_semaphore_signal(fnd_thr_semaphore* semaphore, uint32_t count) {
#if defined(_WIN32)
    ReleaseSemaphore(semaphore->handle, (LONG)count, NULL);
#else
    pthread_mutex_lock(&semaphore->mutex);
    semaphore->count += count;
    if (semaphore->count > semaphore->max_count) semaphore->count = semaphore->max_count;
    pthread_mutex_unlock(&semaphore->mutex);
    if (count == 1) pthread_cond_signal(&semaphore->cond);
    else             pthread_cond_broadcast(&semaphore->cond);
#endif
}

// ===========================
// Barrier
// win32 uses the native SYNCHRONIZATION_BARRIER; posix uses a sense-reversing
// mutex+cond barrier, since pthread_barrier_t is absent on some posix targets (e.g. macOS)

struct fnd_thr_barrier {
#if defined(_WIN32)
    SYNCHRONIZATION_BARRIER handle;
#else
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    uint32_t        participant_count;
    uint32_t        waiting_count;
    uint32_t        generation;
#endif
};

fnd_thr_barrier* fnd_thr_create_barrier(const fnd_thr_barrier_create_info* info) {
    fnd_thr_barrier* barrier = calloc(1, sizeof(fnd_thr_barrier));
    if (!barrier) return NULL;

#if defined(_WIN32)
    if (!InitializeSynchronizationBarrier(&barrier->handle, (LONG)info->participant_count, -1)) goto _fail;
#else
    if (pthread_mutex_init(&barrier->mutex, NULL) != 0) goto _fail;
    if (pthread_cond_init(&barrier->cond, NULL) != 0) { pthread_mutex_destroy(&barrier->mutex); goto _fail; }
    barrier->participant_count = info->participant_count;
#endif

    return barrier;
_fail:
    free(barrier);
    return NULL;
}

void fnd_thr_free_barrier(fnd_thr_barrier* barrier) {
    if (barrier == NULL) return;
#if defined(_WIN32)
    DeleteSynchronizationBarrier(&barrier->handle);
#else
    pthread_cond_destroy(&barrier->cond);
    pthread_mutex_destroy(&barrier->mutex);
#endif
    free(barrier);
}

int fnd_thr_barrier_wait(fnd_thr_barrier* barrier) {
#if defined(_WIN32)
    return EnterSynchronizationBarrier(&barrier->handle, SYNCHRONIZATION_BARRIER_FLAGS_NO_DELETE) ? 1 : 0;
#else
    pthread_mutex_lock(&barrier->mutex);
    uint32_t local_generation = barrier->generation;
    barrier->waiting_count++;

    if (barrier->waiting_count == barrier->participant_count) {
        barrier->generation++;
        barrier->waiting_count = 0;
        pthread_cond_broadcast(&barrier->cond);
        pthread_mutex_unlock(&barrier->mutex);
        return 1;
    }

    while (local_generation == barrier->generation) {
        pthread_cond_wait(&barrier->cond, &barrier->mutex);
    }
    pthread_mutex_unlock(&barrier->mutex);
    return 0;
#endif
}

// ===========================
// Misc

void fnd_thr_sleep_ms(uint64_t milliseconds) {
#if defined(_WIN32)
    Sleep((DWORD)milliseconds);
#else
    struct timespec ts;
    ts.tv_sec  = (time_t)(milliseconds / 1000);
    ts.tv_nsec = (long)((milliseconds % 1000) * 1000000);
    nanosleep(&ts, NULL);
#endif
}

void fnd_thr_yield(void) {
#if defined(_WIN32)
    SwitchToThread();
#else
    sched_yield();
#endif
}

uint64_t fnd_thr_current_thread_id(void) {
#if defined(_WIN32)
    return (uint64_t)GetCurrentThreadId();
#else
    return (uint64_t)(uintptr_t)pthread_self();
#endif
}

uint32_t fnd_thr_hardware_concurrency(void) {
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (uint32_t)info.dwNumberOfProcessors;
#else
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? (uint32_t)count : 1;
#endif
}

#endif // FUNDATIO_THREADS_IMPL
