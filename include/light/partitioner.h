#ifndef LIGHT_PARTITIONER_H
#define LIGHT_PARTITIONER_H

#include <stddef.h>

// Partitioner Object

typedef struct lpr_partitioner_create_info {
    size_t given_memory_size;
} lpr_partitioner_create_info;

typedef struct lpr_partitioner lpr_partitioner;
typedef struct lpr_partition lpr_partition;

lpr_partitioner* lpr_create_partitioner(lpr_partitioner_create_info* info);
void lpr_free_partitioner(lpr_partitioner* partitioner);

lpr_partition* lpr_partitioner_alloc_partition(lpr_partitioner*, size_t required_size, size_t required_align);
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

#define SKIPPED_MAJOR_BINS      5
#define MINOR_BINS_COUNT_LOG2   3
#define MAJOR_BINS_COUNT        (sizeof(size_t) * 8 - SKIPPED_MAJOR_BINS)
#define MINOR_BINS_COUNT        (1 << MINOR_BINS_COUNT_LOG2)
#define MINIMAL_ALLOC_SIZE      (1 << SKIPPED_MAJOR_BINS)

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
    size_t          size;
    size_t          major_bins_free_bitmap;
    size_t          minor_bins_free_bitmaps   [MAJOR_BINS_COUNT];
    lpr_partition*  minor_bins_free_partitions[MAJOR_BINS_COUNT * MINOR_BINS_COUNT];
    lpr_partition*  physical_first_partition;
};

typedef struct locant {
    size_t  major_bin_index;
    size_t  minor_bin_index;
    size_t  rounded_size;   // try to remove
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

static inline size_t get_flat_minor_bin_index(locant loc) {
    return loc.major_bin_index * MINOR_BINS_COUNT + loc.minor_bin_index;
}

static inline void free_used_partition(lpr_partition* partition) {
    partition->prev_free = NULL;
    partition->next_free = NULL;
}

// rounds down the size class, for inserts
static inline locant binmap_down(size_t size) {
    size_t major_bin_idx = bit_scan_msb(size | MINIMAL_ALLOC_SIZE);

    size_t log2_subbin_size = (size_t)(major_bin_idx - MINOR_BINS_COUNT_LOG2);
    size_t sub_bin_idx      = size >> log2_subbin_size;

    size_t adjusted_major_bin_idx = (size_t)((major_bin_idx - SKIPPED_MAJOR_BINS) + (sub_bin_idx >> MINOR_BINS_COUNT_LOG2));
    size_t adjusted_minor_bin_idx = sub_bin_idx & (MINOR_BINS_COUNT - 1);

    return (locant){
        .major_bin_index = adjusted_major_bin_idx,
        .minor_bin_index = adjusted_minor_bin_idx,
        .rounded_size    = size
    };
}

// rounds up the size class, for find queries
static inline locant binmap_up(size_t size) {
    size_t major_bin_idx = bit_scan_msb(size | MINIMAL_ALLOC_SIZE);

    size_t log2_subbin_size   = (size_t)(major_bin_idx - MINOR_BINS_COUNT_LOG2);
    size_t next_subbin_offset = (((size_t)1) << log2_subbin_size) - 1;

    size_t rounded     = size + next_subbin_offset;
    size_t sub_bin_idx = rounded >> log2_subbin_size;

    size_t adjusted_major_bin_idx = (size_t)((major_bin_idx - SKIPPED_MAJOR_BINS) + (sub_bin_idx >> MINOR_BINS_COUNT_LOG2));
    size_t adjusted_minor_bin_idx = sub_bin_idx & (MINOR_BINS_COUNT - 1);
    size_t rounded_size           = rounded & ~next_subbin_offset;

    return (locant){
        .major_bin_index = adjusted_major_bin_idx,
        .minor_bin_index = adjusted_minor_bin_idx,
        .rounded_size = rounded_size
    };
}

static inline int is_partition_free(lpr_partition* partition) {
    return partition != partition->prev_free;
}

static inline void mark_partition_used(lpr_partition* partition) {
    partition->prev_free = partition;
}

// ensures partitioner->physical_first_partition is actually first partition
// must be called on partition after every offset change
static inline void physical_update_first_partition(lpr_partitioner* partitioner, lpr_partition* partition) {
    if (partitioner->physical_first_partition->offset >= partition->offset) partitioner->physical_first_partition = partition;
}

// partition shall be physicaly linked
// does free linkage
static inline void free_list_insert_free_partition(lpr_partitioner* partitioner, lpr_partition* partition) {
    locant loc = binmap_down(partition->size);
    size_t idx = get_flat_minor_bin_index(loc);

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

    size_t flat_idx = get_flat_minor_bin_index(loc);

    // if first and head
    if (partitioner->minor_bins_free_partitions[flat_idx] == partition && !next) {
        // mark minor bin used and not free
        size_t* minor_bins_free_bitmap = &partitioner->minor_bins_free_bitmaps[loc.major_bin_index];
        bitmap_set(minor_bins_free_bitmap, loc.minor_bin_index, 0);

        // if no minor bin free mark major bin not free
        if (*minor_bins_free_bitmap == 0) bitmap_set(&partitioner->major_bins_free_bitmap, loc.major_bin_index, 0);
    }

    partitioner->minor_bins_free_partitions[flat_idx] = next;
}

// removes partition from free list
static inline void free_list_remove_free_partition(lpr_partitioner* partitioner, lpr_partition* partition) {
    locant loc = binmap_down(partition->size);
    free_list_remove_free_partition_given_locant(partitioner, partition, loc);
}

// if split_point != 0, partition can be divided at split_point byte (split_point exclusive)
static inline int physical_prepare_partition_for_use
(lpr_partitioner* partitioner, lpr_partition* partition, size_t required_size, size_t required_alignment) {
    // adjust alignment and size
    size_t aligned_offset; if (required_alignment == 0) aligned_offset = partition->offset;
    else   aligned_offset = ((partition->offset + required_alignment - 1) / required_alignment) * required_alignment;

    size_t offset_adjustment    = aligned_offset - partition->offset;
    size_t size_with_adjustment = required_size + offset_adjustment;

    // can be trimmed, split
    if (partition->size >= size_with_adjustment + MINIMAL_ALLOC_SIZE) {
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
    else if (partition->size < size_with_adjustment) return 0;

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
    locant loc = binmap_up(size);

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
    if (info->given_memory_size == 0) return NULL;

    lpr_partitioner* partitioner = calloc(1, sizeof(lpr_partitioner));
    lpr_partition*   partition   = calloc(1, sizeof(lpr_partition));
    if (!partitioner || !partition) {
        free(partitioner); free(partition);
        return NULL;
    }

    *partition = (lpr_partition){
        .size = info->given_memory_size
    };

    *partitioner = (lpr_partitioner){
        .size                       = info->given_memory_size,
        .physical_first_partition   = partition
    };

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

    free(partitioner);
}

lpr_partition* lpr_partitioner_alloc_partition(lpr_partitioner* partitioner, size_t required_size, size_t required_align) {
    if (required_size == 0 || required_size > partitioner->size) return NULL;

    locant loc; if (!find_free_partition_for_size(partitioner, required_size, &loc)) return NULL;
    size_t flat_idx = get_flat_minor_bin_index(loc);

    lpr_partition* partition = partitioner->minor_bins_free_partitions[flat_idx];
    free_list_remove_free_partition_given_locant(partitioner, partition, loc);

    // try to prepare partition for use, if failed back to free list
    if (!physical_prepare_partition_for_use(partitioner, partition, required_size, required_align)) {
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
