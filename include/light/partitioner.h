/*
----------------------------------------------------------------
Contents
This file provides partitioner object, allowing performant memory partitioning, with TLSF algorithm.

----------------------------------------------------------------
Code info:
- lpr prefix
- LIGHT_PARTITIONER_IMPL macro to build

----------------------------------------------------------------
Usage
- Create partitioner object per partitioned memory.
    Partitioner does only need to know memory size and desired partitions alignment,
    so it can be used to partition even GPU memory like graphics.h buffers.
- Create partitions with lpr_partitioner_alloc_partition
- Free partitions with lpr_partitioner_free_partition
- Partitions are partitioner owned - erasing partitioner frees all partitions.
*/

#ifndef LIGHT_PARTITIONER_H
#define LIGHT_PARTITIONER_H

#include <stddef.h>

// Partitioner Object

typedef struct lpr_partitioner_create_info {
    // all allocations will be aligned to this amount of bytes, 
    // both in size and in offset, will be snaped to nearest bigger power of 2
    size_t  align_bytes;
    // managed memory size
    size_t  memory_bytes;
} lpr_partitioner_create_info;

typedef struct lpr_partitioner lpr_partitioner;
typedef struct lpr_partition lpr_partition;

lpr_partitioner* lpr_create_partitioner(lpr_partitioner_create_info* info);
void lpr_free_partitioner(lpr_partitioner* partitioner);

lpr_partition* lpr_partitioner_alloc_partition(lpr_partitioner*, size_t required_size);
void lpr_partitioner_free_partition(lpr_partitioner*, lpr_partition*);

// Partition Queries

size_t lpr_partition_query_offset(lpr_partition*);
size_t lpr_partition_query_size  (lpr_partition*);

#endif

#ifdef LIGHT_PARTITIONER_IMPL

#include <stdlib.h>
#include <stdint.h>

// ===========================
// Config

#define MINOR_BINS_COUNT_LOG2 3

// ===========================
// Typedefs

typedef struct lpr_partition {
    size_t          size;
    size_t          offset;
    size_t          adjustment;
    lpr_partition*  next_free;
    lpr_partition*  prev_free;
    lpr_partition*  next_physical;
    lpr_partition*  prev_physical;
} lpr_partition;

struct lpr_partitioner {
    // Config
    size_t          memory_bytes;               // total partitioned memory size
    size_t          minor_bins_count_log2;      // log2(minor bins count)
    size_t          major_bins_count;           // count per partitioner
    size_t          minor_bins_count;           // count per major bin
    size_t          skipped_major_bins;         // log2(align bytes)
    size_t          align_bytes;                // memory align

    // State
    size_t          major_bins_free_bitmap;     // if 1, major bin have an free minor bin
    size_t*         minor_bins_free_bitmaps;    // major_bins_count array elements
    lpr_partition** minor_bins_free_partitions; // major_bins_count * minor_bins_count array elements
    lpr_partition*  physical_first_partition;   // the first physical partition
};

typedef struct locant {
    size_t  major_bin_index;
    size_t  minor_bin_index;
} locant;

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

static inline size_t get_flat_minor_bin_index(const lpr_partitioner* partitioner, locant loc) {
    return loc.major_bin_index * partitioner->minor_bins_count + loc.minor_bin_index;
}

// rounds down the size class, for inserts
static inline locant binmap_down(const lpr_partitioner* partitioner, size_t size) {
    size_t major_bin_idx = bit_scan_msb(size | partitioner->align_bytes);

    size_t log2_subbin_size = (size_t)(major_bin_idx - partitioner->minor_bins_count_log2);
    size_t sub_bin_idx      = size >> log2_subbin_size;

    size_t adjusted_major_bin_idx = (size_t)((
        major_bin_idx - partitioner->skipped_major_bins) + (sub_bin_idx >> partitioner->minor_bins_count_log2)
    );
    
    size_t adjusted_minor_bin_idx = sub_bin_idx & (partitioner->minor_bins_count - 1);

    return (locant){
        .major_bin_index = adjusted_major_bin_idx,
        .minor_bin_index = adjusted_minor_bin_idx
    };
}

// rounds up the size class, for find queries
static inline locant binmap_up(const lpr_partitioner* partitioner, size_t size) {
    size_t major_bin_idx = bit_scan_msb(size | partitioner->align_bytes);

    size_t log2_subbin_size   = (size_t)(major_bin_idx - partitioner->minor_bins_count_log2);
    size_t next_subbin_offset = (((size_t)1) << log2_subbin_size) - 1;

    size_t rounded     = size + next_subbin_offset;
    size_t sub_bin_idx = rounded >> log2_subbin_size;

    size_t adjusted_major_bin_idx = (size_t)(
        (major_bin_idx - partitioner->skipped_major_bins) + (sub_bin_idx >> partitioner->minor_bins_count_log2)
    );
    size_t adjusted_minor_bin_idx = sub_bin_idx & (partitioner->minor_bins_count - 1);
    size_t rounded_size           = rounded & ~next_subbin_offset;

    return (locant){
        .major_bin_index = adjusted_major_bin_idx,
        .minor_bin_index = adjusted_minor_bin_idx
    };
}

// ===========================
// Partition Operations

static inline int is_partition_free(lpr_partition* partition) {
    return partition != partition->prev_free;
}

static inline void mark_partition_used(lpr_partition* partition) {
    partition->prev_free = partition;
}

static inline void free_used_partition(lpr_partition* partition) {
    partition->prev_free = NULL;
    partition->next_free = NULL;
}

// ===========================
// Partitioner Operations

// ensures partitioner->physical_first_partition is actually first partition
// must be called on partition after every offset change
static inline void physical_update_first_partition(lpr_partitioner* partitioner, lpr_partition* partition) {
    if (partitioner->physical_first_partition->offset >= partition->offset) partitioner->physical_first_partition = partition;
}

// partition shall be physicaly linked
// does free linkage
static inline void free_list_insert_free_partition(lpr_partitioner* partitioner, lpr_partition* partition) {
    locant loc = binmap_down(partitioner, partition->size);
    size_t idx = get_flat_minor_bin_index(partitioner, loc);

    lpr_partition* current = partitioner->minor_bins_free_partitions[idx];

    // insert partition
    partition->prev_free = NULL;
    partition->next_free = current;
    if (current) current->prev_free = partition;
    partitioner->minor_bins_free_partitions[idx] = partition;

    // mark bins free
    bitmap_set(&partitioner->major_bins_free_bitmap, loc.major_bin_index, 1);
    bitmap_set(&partitioner->minor_bins_free_bitmaps[loc.major_bin_index], loc.minor_bin_index, 1);
}

// removes partition from free list given free list locant
static inline void free_list_remove_free_partition_given_locant(lpr_partitioner* partitioner, lpr_partition* partition, locant loc) {
    lpr_partition* next = partition->next_free;
    lpr_partition* prev = partition->prev_free;

    if (next) next->prev_free = prev;
    if (prev) prev->next_free = next;

    size_t flat_idx = get_flat_minor_bin_index(partitioner, loc);

    // if first and head
    if (partitioner->minor_bins_free_partitions[flat_idx] == partition && !next) {
        // mark minor bin used and not free
        size_t* minor_bins_free_bitmap = &partitioner->minor_bins_free_bitmaps[loc.major_bin_index];
        bitmap_set(minor_bins_free_bitmap, loc.minor_bin_index, 0);

        // if no minor bin free mark major bin not free
        if (*minor_bins_free_bitmap == 0) bitmap_set(&partitioner->major_bins_free_bitmap, loc.major_bin_index, 0);
    }

    if (prev) prev->next_free = next;
    else      partitioner->minor_bins_free_partitions[flat_idx] = next;
}

// removes partition from free list
static inline void free_list_remove_free_partition(lpr_partitioner* partitioner, lpr_partition* partition) {
    locant loc = binmap_down(partitioner, partition->size);
    free_list_remove_free_partition_given_locant(partitioner, partition, loc);
}

// if split_point != 0, partition can be divided at split_point byte (split_point exclusive)
static inline int physical_prepare_partition_for_use
(lpr_partitioner* partitioner, lpr_partition* partition, size_t required_size) {
    // adjust alignment and size
    size_t aligned_offset; if (partitioner->align_bytes == 0) aligned_offset = partition->offset;
    else   aligned_offset = ((partition->offset + partitioner->align_bytes - 1) / partitioner->align_bytes) * partitioner->align_bytes;

    size_t offset_adjustment    = aligned_offset - partition->offset;
    size_t size_with_adjustment = required_size + offset_adjustment;

    // if can be trimmed, split (+ partitioner->align_bytes since trimmed part cannot be lesser than align)
    if (partition->size >= size_with_adjustment + partitioner->align_bytes) {
        lpr_partition* new_partition = calloc(1, sizeof(lpr_partition));
        if (!new_partition) return 0;

        // by definition later than the partion, never physicaly first
        // dont update physicaly_first_partition
        *new_partition = (lpr_partition){
            .size   = partition->size   - size_with_adjustment,
            .offset = partition->offset + size_with_adjustment
        };

        // since we insert new partition, relink next to splited partition
        // (old) -> (new) -> (other)
        if (partition->next_physical) {
            new_partition->next_physical = partition->next_physical;
            partition->next_physical->prev_physical = new_partition;
        }

        partition->next_physical     = new_partition;
        new_partition->prev_physical = partition;

        // update partition size
        partition->size = size_with_adjustment;

        free_list_insert_free_partition(partitioner, new_partition);
    }
    else if (partition->size < size_with_adjustment) 
        return 0;

    // update offset
    partition->adjustment = offset_adjustment;
    mark_partition_used(partition);

    return 1;
}

// tries to merge freed partition
// does not update free list pointer
// therefore partition must not be in free list
static inline void merge_free_partitions(lpr_partitioner* partitioner, lpr_partition* partition) {
    // try merging previous
    if (partition->prev_physical && is_partition_free(partition->prev_physical)) {
        lpr_partition* prev = partition->prev_physical;
        free_list_remove_free_partition(partitioner, prev);

        partition->offset        = prev->offset;
        partition->size         += prev->size;
        partition->prev_physical = prev->prev_physical;

        // undo forward connection to removed partition
        if (partition->prev_physical && partition->prev_physical->next_physical == prev) {
            partition->prev_physical->next_physical = partition;
        }
        
        // ensure physical_first_partition pointer is not invalidated on free
        if (partitioner->physical_first_partition == prev) partitioner->physical_first_partition = partition;

        free(prev);
    }

    // try merging following
    if (partition->next_physical && is_partition_free(partition->next_physical)) {
        lpr_partition* next = partition->next_physical;
        free_list_remove_free_partition(partitioner, next);

        partition->size         += next->size;
        partition->next_physical = next->next_physical;

        // undo backward connection to removed partition
        if (partition->next_physical && partition->next_physical->prev_physical == next) {
            partition->next_physical->prev_physical = partition;
        }

        free(next);
    }

    // update first partition
    physical_update_first_partition(partitioner, partition);
}

// tries to find suitable free partition
// returns non zero at success
static inline int find_free_partition_for_size(lpr_partitioner* partitioner, size_t size, locant* loc_out) {
    locant loc = binmap_up(partitioner, size);

    size_t minor_bins_bitmap = partitioner->minor_bins_free_bitmaps[loc.major_bin_index];
    minor_bins_bitmap &= (~((size_t)0) << loc.minor_bin_index); // mask-out all minor bins to small

    if (minor_bins_bitmap == 0) {                                                       // no free minor bin inside major bin
        size_t major_bins_bitmap = partitioner->major_bins_free_bitmap;
        major_bins_bitmap &= (~((size_t)0) << (loc.major_bin_index + 1));               // mask-out all bitmap smaller than first found
        if (major_bins_bitmap == 0) return 0;                                           // no free major row either; failure
        loc.major_bin_index = bit_scan_lsb(major_bins_bitmap);                          // take first greater major bitmap
        minor_bins_bitmap = partitioner->minor_bins_free_bitmaps[loc.major_bin_index];  // update minor bins bitmap var
    }

    loc.minor_bin_index = bit_scan_lsb(minor_bins_bitmap);

    *loc_out = loc;
    return 1;
}

// ===========================
// API Implementation

lpr_partitioner* lpr_create_partitioner(lpr_partitioner_create_info* info) {
    if (info->memory_bytes == 0) return NULL;

    size_t platform_bits = sizeof(size_t) * 8;

    // Swap align to power of two
    size_t align_power_of_two   = 0;
    size_t real_align           = 1;
    while (info->align_bytes > real_align) {
        real_align *= 2; align_power_of_two++;
        if (align_power_of_two > platform_bits) return NULL;
    }

    // Create objects
    lpr_partitioner* partitioner = calloc(1, sizeof(lpr_partitioner));
    lpr_partition*   partition   = calloc(1, sizeof(lpr_partition));
    if (!partitioner || !partition) {
        free(partitioner); free(partition);
        return NULL;
    }

    *partition = (lpr_partition){
        .size = info->memory_bytes
    };

    *partitioner = (lpr_partitioner){
        .memory_bytes               = info->memory_bytes,
        .minor_bins_count_log2      = MINOR_BINS_COUNT_LOG2,
        .major_bins_count           = (platform_bits - align_power_of_two),
        .minor_bins_count           = (size_t)1 << MINOR_BINS_COUNT_LOG2,
        .skipped_major_bins         = align_power_of_two,
        .align_bytes                = real_align,
        .major_bins_free_bitmap     = 0,
        .physical_first_partition   = partition,
    };

    partitioner->minor_bins_free_bitmaps    = calloc(partitioner->major_bins_count, sizeof(size_t));
    partitioner->minor_bins_free_partitions = calloc(
        partitioner->major_bins_count * partitioner->minor_bins_count, sizeof(lpr_partition*)
    );

    // Check allocations
    if (!partitioner->minor_bins_free_bitmaps || !partitioner->minor_bins_free_partitions) {
        free(partitioner->minor_bins_free_bitmaps); free(partitioner->minor_bins_free_partitions);
        free(partitioner); free(partition);
        return NULL;
    }

    // Insert free partition
    free_list_insert_free_partition(partitioner, partition);
    
    return partitioner;
}

void lpr_free_partitioner(lpr_partitioner* partitioner) {
    if (!partitioner) return;

    lpr_partition* part = partitioner->physical_first_partition;
    while (part) { 
        lpr_partition* next = part->next_physical; 
        free(part); part = next;
    }

    free(partitioner->minor_bins_free_bitmaps);
    free(partitioner->minor_bins_free_partitions);
    free(partitioner);
}

lpr_partition* lpr_partitioner_alloc_partition(lpr_partitioner* partitioner, size_t required_size) {
    if (required_size == 0 || required_size > partitioner->memory_bytes) return NULL;

    locant loc; if (!find_free_partition_for_size(partitioner, required_size, &loc)) return NULL;
    size_t flat_idx = get_flat_minor_bin_index(partitioner, loc);

    lpr_partition* partition = partitioner->minor_bins_free_partitions[flat_idx];
    free_list_remove_free_partition_given_locant(partitioner, partition, loc);

    // try to prepare partition for use, if failed back to free list
    if (!physical_prepare_partition_for_use(partitioner, partition, required_size)) {
        free_list_insert_free_partition(partitioner, partition); return NULL;
    };

    // since new node was created, update first partition
    physical_update_first_partition(partitioner, partition);
    return partition;
}

void lpr_partitioner_free_partition(lpr_partitioner* partitioner, lpr_partition* partition) {
    free_used_partition(partition);
    merge_free_partitions(partitioner, partition);
    free_list_insert_free_partition(partitioner, partition);
}

size_t lpr_partition_query_offset(lpr_partition* partition) {
    return partition->offset + partition->adjustment;
}
size_t lpr_partition_query_size(lpr_partition* partition) {
    return partition->size - partition->adjustment;
}

#endif
