/*
----------------------------------------------------------------
Contents
This file provides partitioner object, allowing memory partitioning, with TLSF algorithm and linear search fallback.

----------------------------------------------------------------
Code info:
- dpr prefix
- DEMIURG_PARTITIONER_IMPL macro to build

----------------------------------------------------------------
Usage
- Create partitioner object per partitioned memory.
    Partitioner does not make any memory moves, so it can even manage unaccessible memory, like gpu vram.
- Create partitions with dpr_partitioner_alloc_partition
- Free partitions with dpr_partitioner_free_partition
- Partitions are partitioner owned - erasing partitioner frees all partitions.
- Partitioner can be reallocated without recreating partitions
    by recreating, with old_partitioner field set in create info

----------------------------------------------------------------
Notes
- Partitioner managed memory must be maximally aligned for partitioner to work
*/

#ifndef DEMIURG_PARTITIONER_H
#define DEMIURG_PARTITIONER_H

#include <stddef.h>

// Partitioner Object

typedef struct dpr_partitioner dpr_partitioner;
typedef struct dpr_partition dpr_partition;

typedef struct dpr_partitioner_create_info {
    // Total size of the managed memory region.
    size_t memory_bytes;

    // Optional source partitioner to migrate.
    // If not NULL, the new partitioner attempts to claim all partitions from
    // old_partitioner. On success, old_partitioner is destroyed and all
    // dpr_partition handles remain valid. On failure, old_partitioner is left
    // unchanged and dpr_create_partitioner() returns NULL.
    dpr_partitioner* old_partitioner;
} dpr_partitioner_create_info;

dpr_partitioner* dpr_create_partitioner(const dpr_partitioner_create_info* info);
void dpr_free_partitioner(dpr_partitioner* partitioner);

dpr_partition* dpr_partitioner_alloc_partition(dpr_partitioner* partitioner, size_t required_size, size_t required_align);
void dpr_partitioner_free_partition(dpr_partitioner*, dpr_partition*);

// Partition Queries

size_t dpr_partition_query_offset(dpr_partition*);
size_t dpr_partition_query_size  (dpr_partition*);

#endif

#ifdef DEMIURG_PARTITIONER_IMPL

#include <stdlib.h>
#include <stdint.h>

// ===========================
// Constants

#define MAJOR_BINS_COUNT_SKIPPED 3
#define MINOR_BINS_COUNT_LOG2    3

// ===========================
// Auto Constants

#define MAJOR_BINS_COUNT ((sizeof(size_t) * 8) - MAJOR_BINS_COUNT_SKIPPED)
#define MINOR_BINS_COUNT ((size_t)1 << MINOR_BINS_COUNT_LOG2)
#define MINIMAL_ALLOC    ((size_t)1 << MAJOR_BINS_COUNT_SKIPPED)

// ===========================
// Typedefs

// Invariant: Partitions have stable pointers, since are heap alloc
// Invariant: if next_free != self, then partition is free
// Invariant: Offset is aligned
struct dpr_partition {
    size_t          size;
    size_t          offset;
    dpr_partition*  next_free;
    dpr_partition*  prev_free;
    dpr_partition*  next_physical;
    dpr_partition*  prev_physical;
};

struct dpr_partitioner {
    size_t          memory_bytes;
    size_t          major_bins_free_bitmap;
    size_t          minor_bins_free_bitmaps[MAJOR_BINS_COUNT];
    dpr_partition*  minor_bins_free_partitions[MAJOR_BINS_COUNT][MINOR_BINS_COUNT];
    dpr_partition*  physical_first_partition;
};

// ===========================
// Helpers

static inline size_t bit_scan_msb(size_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return (sizeof(size_t) == 8)
        ? (63u - (size_t)__builtin_clzll(x))
        : (31u - (size_t)__builtin_clz((unsigned)x));
#else
    size_t r = 0; while (x >>= 1) ++r; return r;
#endif
}

static inline size_t bit_scan_lsb(size_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return (sizeof(size_t) == 8)
        ? (size_t)__builtin_ctzll(x)
        : (size_t)__builtin_ctz((unsigned)x);
#else
    size_t r = 0; while ((x & 1) == 0) {++r; x >>= 1;} return r;
#endif
}

static inline char bitmap_get(size_t bitmap, size_t idx) {
    return (bitmap & ((size_t)1 << idx)) != 0;
}

static inline void bitmap_set(size_t* bitmap, size_t idx, char val) {
    size_t mask = (size_t)1 << idx;
    *bitmap = (val ? (*bitmap | mask) : (*bitmap & (~mask)));
}

// ===========================
// Indexing

// Free list locant
typedef struct locant {
    size_t  major_bin_index;
    size_t  minor_bin_index;
} locant;

// Rounds down the size class, to nearest minor bin, for inserts, size cannot be zero, does not account align!
static inline locant binmap_down(const dpr_partitioner* partitioner, size_t size) {
    size_t major_bin_idx    = bit_scan_msb(size);
    size_t log2_subbin_size = (size_t)(major_bin_idx - MINOR_BINS_COUNT_LOG2);
    size_t sub_bin_idx      = size >> log2_subbin_size;

    size_t adjusted_major_bin_idx = (size_t)(
        (major_bin_idx - MAJOR_BINS_COUNT_SKIPPED) + (sub_bin_idx >> MINOR_BINS_COUNT_LOG2)
    );
    
    size_t adjusted_minor_bin_idx = sub_bin_idx & (MINOR_BINS_COUNT - 1);

    return (locant){
        .major_bin_index = adjusted_major_bin_idx,
        .minor_bin_index = adjusted_minor_bin_idx
    };
}

// Rounds up the size class, to next minor bin, for find queries, size cannot be zero, does not account align!
static inline locant binmap_up(const dpr_partitioner* partitioner, size_t size) {
    size_t major_bin_idx        = bit_scan_msb(size);
    size_t log2_subbin_size     = (size_t)(major_bin_idx - MINOR_BINS_COUNT_LOG2);
    size_t next_subbin_offset   = (((size_t)1) << log2_subbin_size) - 1;

    size_t rounded     = size + next_subbin_offset;
    size_t sub_bin_idx = rounded >> log2_subbin_size;

    size_t adjusted_major_bin_idx = (size_t)(
        (major_bin_idx - MAJOR_BINS_COUNT_SKIPPED) + (sub_bin_idx >> MINOR_BINS_COUNT_LOG2)
    );
    size_t adjusted_minor_bin_idx = sub_bin_idx & (MINOR_BINS_COUNT - 1);
    size_t rounded_size           = rounded & ~next_subbin_offset;

    return (locant){
        .major_bin_index = adjusted_major_bin_idx,
        .minor_bin_index = adjusted_minor_bin_idx
    };
}

size_t align_up(size_t value, size_t alignment) {
    if (alignment == 0) return value;
    return ((value + alignment - 1) / alignment) * alignment;
}

// ===========================
// Partition Operations

static inline int is_partition_free(dpr_partition* partition) {
    return partition->prev_free != partition;
}

static inline void mark_partition_used(dpr_partition* partition) {
    partition->prev_free = partition;
    partition->next_free = NULL;
}

static inline void mark_partition_free(dpr_partition* partition) {
    partition->prev_free = NULL;
    partition->next_free = NULL;
}

// ===========================
// Partitioner Operations

// Push partitions onto free list - cannot fail
// Partitions must be physically linked!
static inline void push_to_free_list(dpr_partitioner* partitioner, dpr_partition* partition) {
    locant loc = binmap_down(partitioner, partition->size);

    // Make partition new free list head
    dpr_partition* head = partitioner->minor_bins_free_partitions[loc.major_bin_index][loc.minor_bin_index];
    partition->prev_free = NULL;
    partition->next_free = head;
    if (head) head->prev_free = partition;
    partitioner->minor_bins_free_partitions[loc.major_bin_index][loc.minor_bin_index] = partition;

    // Mark bins free
    bitmap_set(&partitioner->major_bins_free_bitmap, loc.major_bin_index, 1);
    bitmap_set(&partitioner->minor_bins_free_bitmaps[loc.major_bin_index], loc.minor_bin_index, 1);
}

// Removes partition from free at locant cannot fail
static inline void remove_from_free_list(dpr_partitioner* partitioner, dpr_partition* partition) {
    // Not head
    if (partition->prev_free) {
        partition->prev_free->next_free = partition->next_free;
        if (partition->next_free) {
            partition->next_free->prev_free = partition->prev_free;
        }
        return; // Disappeared from free list
    }

    // Was head, now the next one is head
    locant loc = binmap_down(partitioner, partition->size);
    partitioner->minor_bins_free_partitions[loc.major_bin_index][loc.minor_bin_index] = partition->next_free;

    // If there was no next, mark free list owning minor bin empty
    if (!partition->next_free) {
        bitmap_set(&partitioner->minor_bins_free_bitmaps[loc.major_bin_index], loc.minor_bin_index, 0);
        bitmap_set(&partitioner->major_bins_free_bitmap, loc.major_bin_index, partitioner->minor_bins_free_bitmaps[loc.major_bin_index] != 0);
    }
    else {  // Else remove next node connection
        partition->next_free->prev_free = NULL;
    }
}

// Merges neighbour free partitions into this partition
// Shall be called before inserting partition to free list
static inline void merge_neighbour_free(dpr_partitioner* partitioner, dpr_partition* partition) {
    // Try merging previous
    if (partition->prev_physical && is_partition_free(partition->prev_physical)) {
        dpr_partition* prev = partition->prev_physical;
        remove_from_free_list(partitioner, prev);
        *partition = (dpr_partition){
            .offset         = prev->offset,
            .size           = prev->size + partition->size,
            .prev_physical  = prev->prev_physical,
            .next_physical  = partition->next_physical
        };
        
        // Change forward connection to removed partition
        if (prev->prev_physical) {
            prev->prev_physical->next_physical = partition;
        }

        // Fix partitioner first partition, since it might have changed
        if (partitioner->physical_first_partition == prev) {
            partitioner->physical_first_partition = partition;
        }
        
        free(prev);
    }

    // Try merging next
    if (partition->next_physical && is_partition_free(partition->next_physical)) {
        dpr_partition* next = partition->next_physical;
        remove_from_free_list(partitioner, next);
        *partition = (dpr_partition){
            .offset         = partition->offset,
            .size           = next->size + partition->size,
            .prev_physical  = partition->prev_physical,
            .next_physical  = next->next_physical
        };

        // Change backward connection to removed partition
        if (next->next_physical) {
            next->next_physical->prev_physical = partition;
        }

        free(next);
    }
}

// Tries to find suitable free partition
// returns not NULL partition at success
static inline dpr_partition* find_free_partition_for_size(dpr_partitioner* partitioner, size_t size, size_t align) {
    // Try to find suitable bins O(1)
    // Since we are looking for size + align, found bin can always be trimmed so offset is aligned
    locant loc = binmap_up(partitioner, size + align);
    size_t minor_bins_bitmap = partitioner->minor_bins_free_bitmaps[loc.major_bin_index];
    minor_bins_bitmap &= (~((size_t)0) << loc.minor_bin_index); // mask-out all minor bins to small
    if (minor_bins_bitmap == 0) goto _find_higher_major;        // no free minor bin inside major bin
    loc.minor_bin_index = bit_scan_lsb(minor_bins_bitmap);
    return partitioner->minor_bins_free_partitions[loc.major_bin_index][loc.minor_bin_index];

    // Allocation within our major bin bin failed
    // Try allocation within higher major bin, still O(1),
    // Keeps searching for size + align
_find_higher_major: {
    size_t major_bins_bitmap = partitioner->major_bins_free_bitmap;
    major_bins_bitmap &= (~((size_t)0) << (loc.major_bin_index + 1));   // Mask-out all bitmap smaller than first found
    if (major_bins_bitmap == 0) goto _fallback_down_bin;                // No free major row either
    loc.major_bin_index = bit_scan_lsb(major_bins_bitmap);              // take first greater major bitmap
    minor_bins_bitmap   = partitioner->minor_bins_free_bitmaps[loc.major_bin_index];
    loc.minor_bin_index = bit_scan_lsb(minor_bins_bitmap);
    return partitioner->minor_bins_free_partitions[loc.major_bin_index][loc.minor_bin_index];
}
    // There is no partition bigger or equal to size + align
    // We can still check partitions of size >= than required, if they can be aligned
    // This is O(free partitions) search
_fallback_down_bin: {
    for (size_t major = 0; major <= MAJOR_BINS_COUNT; major++) {
        if (!bitmap_get(partitioner->major_bins_free_bitmap, major)) continue; // Skip entire empty major bin
        for (size_t minor = 0; minor < MINOR_BINS_COUNT; minor++) {
            if (!bitmap_get(partitioner->minor_bins_free_bitmaps[major], minor)) continue;  // Skip empty minor bin
            dpr_partition* part = partitioner->minor_bins_free_partitions[major][minor];
            while (part) {
                size_t aligned_offset = align_up(part->offset, align);
                size_t padding = aligned_offset - part->offset;
                if (part->size < padding || (part->size - padding) < size) {part = part->next_free; continue;}
                return part;
            }
        }
    }

    return NULL; // Truly, there is no way
}
}

// Given partition and it required params, trims both align padding and extra space
// May only fail if new partitions fail to allocate - then partition is untouched and shall return to free list
static inline int trim_partition(dpr_partitioner* partitioner, dpr_partition* partition, size_t size, size_t align) {
    size_t final_offset = align_up(partition->offset, align);

    // Find trim bytes
    size_t trim_begin = final_offset - partition->offset;
    size_t trim_end   = (partition->size - trim_begin) - size;

    // Allocate partitions
    dpr_partition* begin = NULL, *end = NULL;
    if (trim_begin >= MINIMAL_ALLOC) {
        begin = malloc(sizeof(dpr_partition));
        if (!begin) return 0;
    }
    if (trim_end >= MINIMAL_ALLOC) {
        end = malloc(sizeof(dpr_partition));
        if (!end) {free(begin); return 0;}
    }

    // Setup and physically link begin partition
    if (begin) {
        *begin = (dpr_partition){
            .size           = trim_begin,
            .offset         = partition->offset,
            .next_physical  = partition,
            .prev_physical  = partition->prev_physical
        };
        mark_partition_free(begin);
        if (partition->prev_physical) partition->prev_physical->next_physical = begin;
    }

    // Setup and physically link end partition
    if (end) {
        *end = (dpr_partition){
            .size           = trim_end,
            .offset         = final_offset + size,
            .next_physical  = partition->next_physical,
            .prev_physical  = partition
        };
        mark_partition_free(end);
        if (partition->next_physical) partition->next_physical->prev_physical = end;
    }

    // Update partition
    *partition = (dpr_partition){
        .size           = size,
        .offset         = final_offset,
        .next_physical  = end   ? end   : partition->next_physical,
        .prev_physical  = begin ? begin : partition->prev_physical,
    };

    // Push free partitions
    if (begin) push_to_free_list(partitioner, begin);
    if (end)   push_to_free_list(partitioner, end);

    // Since new partition was emited at front, check if it is not first
    if (begin && partitioner->physical_first_partition->offset >= begin->offset) {
        partitioner->physical_first_partition = begin;
    }

    // Success
    return 1;
}

// ===========================
// Partitioner Creation

dpr_partitioner* create_partitioner_brand_new(const dpr_partitioner_create_info* info) {
    dpr_partitioner* partitioner = malloc(sizeof(dpr_partitioner));
    dpr_partition*   partition   = malloc(sizeof(dpr_partition));

    if (!partitioner || !partition) {
        free(partitioner); free(partition); return NULL;
    }

    *partition   = (dpr_partition)  {.offset = 0, .size = info->memory_bytes}; mark_partition_free(partition);
    *partitioner = (dpr_partitioner){.memory_bytes = info->memory_bytes, .physical_first_partition = partition};
    push_to_free_list(partitioner, partition);

    return partitioner;
}

dpr_partitioner* create_partitioner_from_old(const dpr_partitioner_create_info* info) {
    if (info->memory_bytes < info->old_partitioner->memory_bytes)  return NULL;
    if (info->memory_bytes == info->old_partitioner->memory_bytes) return info->old_partitioner;

    dpr_partition* last_part = info->old_partitioner->physical_first_partition;
    while (last_part->next_physical) last_part = last_part->next_physical;

    // Add partition with extra space at the end
    dpr_partition* extra_space = malloc(sizeof(dpr_partition));
    if (!extra_space) return NULL;
    *extra_space = (dpr_partition){
        .size           = info->memory_bytes - info->old_partitioner->memory_bytes,
        .offset         = last_part->offset + last_part->size,
        .prev_physical  = last_part,
        .next_physical  = NULL
    };
    last_part->next_physical = extra_space;
    dpr_partitioner_free_partition(info->old_partitioner, extra_space);

    // Update partitioner info
    info->old_partitioner->memory_bytes = info->memory_bytes;

    return info->old_partitioner;
}

// ===========================
// API

dpr_partitioner* dpr_create_partitioner(const dpr_partitioner_create_info* info) {
    if (info->memory_bytes < MINIMAL_ALLOC) return NULL;
    if (info->old_partitioner) return create_partitioner_from_old(info);
    return create_partitioner_brand_new(info);
}

void dpr_free_partitioner(dpr_partitioner* partitioner) {
    if (!partitioner) return;
    dpr_partition* part = partitioner->physical_first_partition;
    while (part) { 
        dpr_partition* next = part->next_physical; 
        free(part); part = next;
    }
    free(partitioner);
}

dpr_partition* dpr_partitioner_alloc_partition(dpr_partitioner* partitioner, size_t required_size, size_t required_align) {
    if (required_size == 0 || required_size > partitioner->memory_bytes) return NULL; dpr_partition* part = NULL;

    // Apply minimum allocation size
    if (required_size < MINIMAL_ALLOC) required_size = MINIMAL_ALLOC;

    // Check only first
    if (required_align >= partitioner->memory_bytes) {
        if (!is_partition_free(partitioner->physical_first_partition))   return NULL;
        if (partitioner->physical_first_partition->size < required_size) return NULL;
        part = partitioner->physical_first_partition; goto _claim_partition;
    }

    // Find partition
    part = find_free_partition_for_size(partitioner, required_size, required_align);
    if (!part) return NULL; // No suitable free partition

_claim_partition:
    // As is no longer free
    remove_from_free_list(partitioner, part);

    // Trim partition
    if (!trim_partition(partitioner, part, required_size, required_align)) {
        push_to_free_list(partitioner, part); return NULL;
    }

    // Mark used
    mark_partition_used(part);

    // Return partition
    return part;
}

void dpr_partitioner_free_partition(dpr_partitioner* partitioner, dpr_partition* partition) {
    mark_partition_free(partition);
    merge_neighbour_free(partitioner, partition);
    push_to_free_list(partitioner, partition);
}

// Partition Queries

size_t dpr_partition_query_offset(dpr_partition* partition) {
    return partition->offset;
}

size_t dpr_partition_query_size  (dpr_partition* partition) {
    return partition->size;
}

#endif
