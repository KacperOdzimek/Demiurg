/*
----------------------------------------------------------------
Contents:
This file provides cross-platform C99 threading primitives: thread, mutex, condition variable,
read-write lock, semaphore, and barrier objects. Each primitive wraps the native OS backend (win32 or pthreads).

----------------------------------------------------------------
Code info:
- dth prefix
- DEMIURG_THREADS_IMPL macro to build

----------------------------------------------------------------
Usage:
- Fill the matching create info struct, create the object, use it, then free it
- dth_create_thread begins execution immediately, before the call returns
- Wait/lock operations are O(1); all objects are individually heap-allocated

----------------------------------------------------------------
Notes:
- Objects are not safe to create/free concurrently with their own use from another thread
- Freeing a thread that was neither joined nor detached will block until it finishes
- timeout_ms of 0 in a *_wait_for call polls once and returns immediately
*/

#ifndef DEMIURG_THREADS_H
#define DEMIURG_THREADS_H

#include <stddef.h>
#include <stdint.h>

// ===========================
// Thread-Local Storage

#if defined(_MSC_VER)
    #define dth_thread_local __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
    #define dth_thread_local __thread
#else
    #define dth_thread_local _Thread_local // C11 fallback
#endif

// ===========================
// Thread

typedef int (*dth_entry_point)(void* user_data);

typedef struct dth_thread_create_info {
    dth_entry_point entry_point;
    void*           user_data;
    size_t          stack_size_bytes;   // 0 = platform default
    const char*     debug_name;         // optional, may be NULL
} dth_thread_create_info;

typedef struct dth_thread dth_thread;
dth_thread* dth_create_thread(const dth_thread_create_info*);
void dth_free_thread  (dth_thread*);

int  dth_join_thread  (dth_thread*, int* out_exit_code); // 0 = success
void dth_detach_thread(dth_thread*);
uint64_t dth_get_thread_id(const dth_thread*);

// ===========================
// Mutex

typedef struct dth_mutex_create_info {
    int recursive; // non-zero = recursive mutex
} dth_mutex_create_info;

typedef struct dth_mutex dth_mutex;
dth_mutex* dth_create_mutex(const dth_mutex_create_info*);
void dth_free_mutex(dth_mutex*);

void dth_mutex_lock(dth_mutex*);
int  dth_mutex_trylock(dth_mutex*); // 1 = acquired
void dth_mutex_unlock(dth_mutex*);

// ===========================
// Condition Variable

typedef struct dth_cond dth_cond;
dth_cond* dth_create_cond(void);
void dth_free_cond(dth_cond*);

void dth_cond_wait (dth_cond*, dth_mutex*);
int  dth_cond_wait_for(dth_cond*, dth_mutex*, uint64_t timeout_ms); // 0 = signaled, 1 = timed out
void dth_cond_signal (dth_cond*);
void dth_cond_broadcast(dth_cond*);

// ===========================
// Read-Write Lock

typedef struct dth_rwlock dth_rwlock;
dth_rwlock* dth_create_rwlock(void);
void dth_free_rwlock(dth_rwlock*);

void dth_rwlock_lock_read(dth_rwlock*);
void dth_rwlock_lock_write(dth_rwlock*);
void dth_rwlock_unlock_read(dth_rwlock*);
void dth_rwlock_unlock_write(dth_rwlock*);

// ===========================
// Semaphore

typedef struct dth_semaphore_create_info {
    uint32_t initial_count;
    uint32_t max_count; // 0 = unbounded (implementation defined ceiling)
} dth_semaphore_create_info;

typedef struct dth_semaphore dth_semaphore;
dth_semaphore* dth_create_semaphore(const dth_semaphore_create_info*);
void dth_free_semaphore(dth_semaphore*);

void dth_semaphore_wait(dth_semaphore*);
int  dth_semaphore_wait_for(dth_semaphore*, uint64_t timeout_ms); // 0 = acquired, 1 = timed out
void dth_semaphore_signal(dth_semaphore*, uint32_t count);

// ===========================
// Barrier

typedef struct dth_barrier_create_info {
    uint32_t participant_count;
} dth_barrier_create_info;
typedef struct dth_barrier dth_barrier;

dth_barrier* dth_create_barrier(const dth_barrier_create_info*);
void dth_free_barrier(dth_barrier*);

int dth_barrier_wait(dth_barrier*); // non-zero for exactly one participant (the serial thread)

// ===========================
// Misc

void dth_sleep_ms(uint64_t milliseconds);
void dth_yield(void);
uint64_t dth_current_thread_id(void);
uint32_t dth_hardware_concurrency(void);

#endif // DEMIURG_THREADS_H

#ifdef DEMIURG_THREADS_IMPL

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

struct dth_thread {
#if defined(_WIN32)
    HANDLE   handle;
    uint64_t id;
#else
    pthread_t handle;
#endif
    dth_entry_point entry_point;
    void*               user_data;
    int                 exit_code;
    int                 joined_or_detached;
};

#if defined(_WIN32)
static unsigned __stdcall dth_win32_trampoline(void* param) {
    dth_thread* thread = (dth_thread*)param;
    thread->exit_code = thread->entry_point(thread->user_data);
    return (unsigned)thread->exit_code;
}
#else
static void* dth_posix_trampoline(void* param) {
    dth_thread* thread = (dth_thread*)param;
    thread->exit_code = thread->entry_point(thread->user_data);
    return NULL;
}
#endif

dth_thread* dth_create_thread(const dth_thread_create_info* info) {
    dth_thread* thread = calloc(1, sizeof(dth_thread));
    if (!thread) return NULL;

    thread->entry_point = info->entry_point;
    thread->user_data   = info->user_data;

#if defined(_WIN32)
    unsigned int win32_id = 0;
    thread->handle = (HANDLE)_beginthreadex(
        NULL,
        (unsigned int)info->stack_size_bytes,
        dth_win32_trampoline,
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

    int result = pthread_create(&thread->handle, &attr, dth_posix_trampoline, thread);
    pthread_attr_destroy(&attr);
    if (result != 0) goto _fail;
#endif

    (void)info->debug_name; // reserved for platform thread-naming hooks

    return thread;
_fail:
    free(thread);
    return NULL;
}

void dth_free_thread(dth_thread* thread) {
    if (thread == NULL) return;
    if (!thread->joined_or_detached) dth_join_thread(thread, NULL); // see Notes
    free(thread);
}

int dth_join_thread(dth_thread* thread, int* out_exit_code) {
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

void dth_detach_thread(dth_thread* thread) {
#if defined(_WIN32)
    CloseHandle(thread->handle);
    thread->handle = NULL;
#else
    pthread_detach(thread->handle);
#endif
    thread->joined_or_detached = 1;
}

uint64_t dth_get_thread_id(const dth_thread* thread) {
#if defined(_WIN32)
    return thread->id;
#else
    return (uint64_t)(uintptr_t)thread->handle;
#endif
}

// ===========================
// Mutex

struct dth_mutex {
#if defined(_WIN32)
    CRITICAL_SECTION handle;
#else
    pthread_mutex_t handle;
#endif
};

dth_mutex* dth_create_mutex(const dth_mutex_create_info* info) {
    dth_mutex* mutex = calloc(1, sizeof(dth_mutex));
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

void dth_free_mutex(dth_mutex* mutex) {
    if (mutex == NULL) return;
#if defined(_WIN32)
    DeleteCriticalSection(&mutex->handle);
#else
    pthread_mutex_destroy(&mutex->handle);
#endif
    free(mutex);
}

void dth_mutex_lock(dth_mutex* mutex) {
#if defined(_WIN32)
    EnterCriticalSection(&mutex->handle);
#else
    pthread_mutex_lock(&mutex->handle);
#endif
}

int dth_mutex_trylock(dth_mutex* mutex) {
#if defined(_WIN32)
    return TryEnterCriticalSection(&mutex->handle) != 0;
#else
    return pthread_mutex_trylock(&mutex->handle) == 0;
#endif
}

void dth_mutex_unlock(dth_mutex* mutex) {
#if defined(_WIN32)
    LeaveCriticalSection(&mutex->handle);
#else
    pthread_mutex_unlock(&mutex->handle);
#endif
}

// ===========================
// Condition Variable

struct dth_cond {
#if defined(_WIN32)
    CONDITION_VARIABLE handle;
#else
    pthread_cond_t handle;
#endif
};

dth_cond* dth_create_cond(void) {
    dth_cond* cond = calloc(1, sizeof(dth_cond));
    if (!cond) return NULL;

#if defined(_WIN32)
    InitializeConditionVariable(&cond->handle);
#else
    if (pthread_cond_init(&cond->handle, NULL) != 0) { free(cond); return NULL; }
#endif

    return cond;
}

void dth_free_cond(dth_cond* cond) {
    if (cond == NULL) return;
#if !defined(_WIN32)
    pthread_cond_destroy(&cond->handle);
#endif
    free(cond);
}

void dth_cond_wait(dth_cond* cond, dth_mutex* mutex) {
#if defined(_WIN32)
    SleepConditionVariableCS(&cond->handle, &mutex->handle, INFINITE);
#else
    pthread_cond_wait(&cond->handle, &mutex->handle);
#endif
}

int dth_cond_wait_for(dth_cond* cond, dth_mutex* mutex, uint64_t timeout_ms) {
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

void dth_cond_signal(dth_cond* cond) {
#if defined(_WIN32)
    WakeConditionVariable(&cond->handle);
#else
    pthread_cond_signal(&cond->handle);
#endif
}

void dth_cond_broadcast(dth_cond* cond) {
#if defined(_WIN32)
    WakeAllConditionVariable(&cond->handle);
#else
    pthread_cond_broadcast(&cond->handle);
#endif
}

// ===========================
// Read-Write Lock

struct dth_rwlock {
#if defined(_WIN32)
    SRWLOCK handle;
#else
    pthread_rwlock_t handle;
#endif
};

dth_rwlock* dth_create_rwlock(void) {
    dth_rwlock* rwlock = calloc(1, sizeof(dth_rwlock));
    if (!rwlock) return NULL;

#if defined(_WIN32)
    InitializeSRWLock(&rwlock->handle);
#else
    if (pthread_rwlock_init(&rwlock->handle, NULL) != 0) { free(rwlock); return NULL; }
#endif

    return rwlock;
}

void dth_free_rwlock(dth_rwlock* rwlock) {
    if (rwlock == NULL) return;
#if !defined(_WIN32)
    pthread_rwlock_destroy(&rwlock->handle);
#endif
    free(rwlock);
}

void dth_rwlock_lock_read(dth_rwlock* rwlock) {
#if defined(_WIN32)
    AcquireSRWLockShared(&rwlock->handle);
#else
    pthread_rwlock_rdlock(&rwlock->handle);
#endif
}

void dth_rwlock_lock_write(dth_rwlock* rwlock) {
#if defined(_WIN32)
    AcquireSRWLockExclusive(&rwlock->handle);
#else
    pthread_rwlock_wrlock(&rwlock->handle);
#endif
}

void dth_rwlock_unlock_read(dth_rwlock* rwlock) {
#if defined(_WIN32)
    ReleaseSRWLockShared(&rwlock->handle);
#else
    pthread_rwlock_unlock(&rwlock->handle);
#endif
}

void dth_rwlock_unlock_write(dth_rwlock* rwlock) {
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

struct dth_semaphore {
#if defined(_WIN32)
    HANDLE handle;
#else
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    uint32_t        count;
    uint32_t        max_count;
#endif
};

dth_semaphore* dth_create_semaphore(const dth_semaphore_create_info* info) {
    dth_semaphore* semaphore = calloc(1, sizeof(dth_semaphore));
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

void dth_free_semaphore(dth_semaphore* semaphore) {
    if (semaphore == NULL) return;
#if defined(_WIN32)
    CloseHandle(semaphore->handle);
#else
    pthread_cond_destroy(&semaphore->cond);
    pthread_mutex_destroy(&semaphore->mutex);
#endif
    free(semaphore);
}

void dth_semaphore_wait(dth_semaphore* semaphore) {
#if defined(_WIN32)
    WaitForSingleObject(semaphore->handle, INFINITE);
#else
    pthread_mutex_lock(&semaphore->mutex);
    while (semaphore->count == 0) pthread_cond_wait(&semaphore->cond, &semaphore->mutex);
    semaphore->count--;
    pthread_mutex_unlock(&semaphore->mutex);
#endif
}

int dth_semaphore_wait_for(dth_semaphore* semaphore, uint64_t timeout_ms) {
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

void dth_semaphore_signal(dth_semaphore* semaphore, uint32_t count) {
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

struct dth_barrier {
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

dth_barrier* dth_create_barrier(const dth_barrier_create_info* info) {
    dth_barrier* barrier = calloc(1, sizeof(dth_barrier));
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

void dth_free_barrier(dth_barrier* barrier) {
    if (barrier == NULL) return;
#if defined(_WIN32)
    DeleteSynchronizationBarrier(&barrier->handle);
#else
    pthread_cond_destroy(&barrier->cond);
    pthread_mutex_destroy(&barrier->mutex);
#endif
    free(barrier);
}

int dth_barrier_wait(dth_barrier* barrier) {
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

void dth_sleep_ms(uint64_t milliseconds) {
#if defined(_WIN32)
    Sleep((DWORD)milliseconds);
#else
    struct timespec ts;
    ts.tv_sec  = (time_t)(milliseconds / 1000);
    ts.tv_nsec = (long)((milliseconds % 1000) * 1000000);
    nanosleep(&ts, NULL);
#endif
}

void dth_yield(void) {
#if defined(_WIN32)
    SwitchToThread();
#else
    sched_yield();
#endif
}

uint64_t dth_current_thread_id(void) {
#if defined(_WIN32)
    return (uint64_t)GetCurrentThreadId();
#else
    return (uint64_t)(uintptr_t)pthread_self();
#endif
}

uint32_t dth_hardware_concurrency(void) {
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (uint32_t)info.dwNumberOfProcessors;
#else
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? (uint32_t)count : 1;
#endif
}

#endif // DEMIURG_THREADS_IMPL
