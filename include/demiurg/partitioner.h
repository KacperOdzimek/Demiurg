/*
----------------------------------------------------------------
Contents
This file provides partitioner object, allowing performant memory partitioning, with TLSF algorithm.

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
- Partitioner can be reallocated and defragmented without recreating partitions objects
    by recreating, using old_partitioner and defragmentation_moves create info fields
*/

#ifndef DEMIURG_PARTITIONER_H
#define DEMIURG_PARTITIONER_H

#include <stddef.h>

// Defragmentation Moves

typedef struct dpr_defragmentation_move {
    size_t old_offset;
    size_t new_offset;
    size_t bytes;
} dpr_defragmentation_move;

// Partitioner Object

typedef struct dpr_partitioner dpr_partitioner;
typedef struct dpr_partition dpr_partition;

typedef struct dpr_partitioner_create_info {
    // Alignment of all partitions.
    // Both partition offsets and sizes are rounded up to the next power-of-two
    // alignment not smaller than this value.
    size_t  align_bytes;

    // Total size of the managed memory region.
    size_t  memory_bytes;

    // Optional source partitioner to migrate.
    // If not NULL, the new partitioner attempts to claim all partitions from
    // old_partitioner. On success, old_partitioner is destroyed and all
    // dpr_partition handles remain valid. On failure, old_partitioner is left
    // unchanged and dpr_create_partitioner() returns NULL.
    // Also, if not using defragmentation, new paritioner align must be equal or lesser to old partitioner align
    dpr_partitioner* old_partitioner;

    // Optional output for the generated defragmentation moves.
    // When old_partitioner is not NULL, the library allocates and returns a
    // sequence of moves describing how the managed memory should be relocated
    // from the old layout to the new one. The new partitioner already assumes
    // these moves have been applied. Set to NULL if defragmentation is not desired.
    // The returned array must be freed with free().
    // The returned array is null terminated with dpr_defragmentation_move with bytes equal 0
    dpr_defragmentation_move** defragmentation_moves;
} dpr_partitioner_create_info;

dpr_partitioner* dpr_create_partitioner(const dpr_partitioner_create_info* info);
void dpr_free_partitioner(dpr_partitioner* partitioner);

dpr_partition* dpr_partitioner_alloc_partition(dpr_partitioner*, size_t required_size);
void dpr_partitioner_free_partition(dpr_partitioner*, dpr_partition*);

// Partition Queries

size_t dpr_partition_query_offset(dpr_partition*);
size_t dpr_partition_query_size  (dpr_partition*);

#endif

#ifdef DEMIURG_PARTITIONER_IMPL

#include <stdlib.h>
#include <stdint.h>

// ===========================
// Config

#define MINOR_BINS_COUNT_LOG2 3

// ===========================
// Typedefs

// invariant : if next_free != self ptr, then partition is free
typedef struct dpr_partition {
    size_t          size;
    size_t          offset;
    size_t          adjustment;
    dpr_partition*  next_free;
    dpr_partition*  prev_free;
    dpr_partition*  next_physical;
    dpr_partition*  prev_physical;
} dpr_partition;

struct dpr_partitioner {
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
    dpr_partition** minor_bins_free_partitions; // major_bins_count * minor_bins_count array elements
    dpr_partition*  physical_first_partition;   // the first physical partition
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

static inline size_t get_flat_minor_bin_index(const dpr_partitioner* partitioner, locant loc) {
    return loc.major_bin_index * partitioner->minor_bins_count + loc.minor_bin_index;
}

// rounds down the size class, for inserts, size cannot be zero
static inline locant binmap_down(const dpr_partitioner* partitioner, size_t size) {
    size_t major_bin_idx = bit_scan_msb(size | partitioner->align_bytes);
    if (major_bin_idx < partitioner->minor_bins_count_log2) major_bin_idx = partitioner->minor_bins_count_log2;

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

// rounds up the size class, for find queries, size cannot be zero
static inline locant binmap_up(const dpr_partitioner* partitioner, size_t size) {
    size_t major_bin_idx = bit_scan_msb(size | partitioner->align_bytes);
    if (major_bin_idx < partitioner->minor_bins_count_log2) major_bin_idx = partitioner->minor_bins_count_log2;

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

// ensures partitioner->physical_first_partition is actually first partition
// must be called on partition after every offset change
static inline void physical_update_first_partition(dpr_partitioner* partitioner, dpr_partition* partition) {
    if (partitioner->physical_first_partition->offset >= partition->offset) partitioner->physical_first_partition = partition;
}

// partition shall be physicaly linked
// does free linkage
static inline void free_list_insert_free_partition(dpr_partitioner* partitioner, dpr_partition* partition) {
    locant loc = binmap_down(partitioner, partition->size);
    size_t idx = get_flat_minor_bin_index(partitioner, loc);

    dpr_partition* current = partitioner->minor_bins_free_partitions[idx];

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
static inline void free_list_remove_free_partition_given_locant(dpr_partitioner* partitioner, dpr_partition* partition, locant loc) {
    dpr_partition* next = partition->next_free;
    dpr_partition* prev = partition->prev_free;

    if (next) next->prev_free = prev;
    size_t flat_idx = get_flat_minor_bin_index(partitioner, loc);

    if (!prev) {
        // partition was the head of this bin's list
        partitioner->minor_bins_free_partitions[flat_idx] = next;

        if (!next) {
            // bin is now empty; mark minor bin used and not free
            size_t* minor_bins_free_bitmap = &partitioner->minor_bins_free_bitmaps[loc.major_bin_index];
            bitmap_set(minor_bins_free_bitmap, loc.minor_bin_index, 0);

            // if no minor bin free, mark major bin not free
            if (*minor_bins_free_bitmap == 0) bitmap_set(&partitioner->major_bins_free_bitmap, loc.major_bin_index, 0);
        }
    }
    else {
        prev->next_free = next;
    }
}

// removes partition from free list
static inline void free_list_remove_free_partition(dpr_partitioner* partitioner, dpr_partition* partition) {
    locant loc = binmap_down(partitioner, partition->size);
    free_list_remove_free_partition_given_locant(partitioner, partition, loc);
}

// if split_point != 0, partition can be divided at split_point byte (split_point exclusive)
static inline int physical_prepare_partition_for_use
(dpr_partitioner* partitioner, dpr_partition* partition, size_t required_size) {
    // adjust alignment and size
    size_t aligned_offset; if (partitioner->align_bytes == 0) aligned_offset = partition->offset;
    else   aligned_offset = ((partition->offset + partitioner->align_bytes - 1) / partitioner->align_bytes) * partitioner->align_bytes;

    size_t offset_adjustment    = aligned_offset - partition->offset;
    size_t size_with_adjustment = required_size + offset_adjustment;

    // if can be trimmed, split (+ partitioner->align_bytes since trimmed part cannot be lesser than align)
    if (partition->size >= size_with_adjustment + partitioner->align_bytes) {
        dpr_partition* new_partition = calloc(1, sizeof(dpr_partition));
        if (!new_partition) return 0;

        // by definition later than the partion, never physicaly first
        // dont update physicaly_first_partition
        *new_partition = (dpr_partition){
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
static inline void merge_free_partitions(dpr_partitioner* partitioner, dpr_partition* partition) {
    // try merging previous
    if (partition->prev_physical && is_partition_free(partition->prev_physical)) {
        dpr_partition* prev = partition->prev_physical;
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
        dpr_partition* next = partition->next_physical;
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

// Tries to find suitable free partition
// returns non zero at success
// Tries to find suitable free partition
// returns non zero at success
static inline int find_free_partition_for_size(dpr_partitioner* partitioner, size_t size, locant* loc_out) {
    locant loc = binmap_up(partitioner, size);

    // Try to find suitable bins O(1)
    size_t minor_bins_bitmap = partitioner->minor_bins_free_bitmaps[loc.major_bin_index];
    minor_bins_bitmap &= (~((size_t)0) << loc.minor_bin_index); // mask-out all minor bins to small
    if (minor_bins_bitmap == 0) goto _find_higher_major;        // no free minor bin inside major bin

    loc.minor_bin_index = bit_scan_lsb(minor_bins_bitmap);
    *loc_out = loc;
    return 1;

    // Try to find other suitable major bin, since this is all occupied, O(1)
_find_higher_major: {
    size_t major_bins_bitmap = partitioner->major_bins_free_bitmap;
    major_bins_bitmap &= (~((size_t)0) << (loc.major_bin_index + 1)); // mask-out all bitmap smaller than first found
    if (major_bins_bitmap == 0) goto _fallback_down_bin; // no free major row either

    loc.major_bin_index = bit_scan_lsb(major_bins_bitmap);          // take first greater major bitmap
    minor_bins_bitmap   = partitioner->minor_bins_free_bitmaps[loc.major_bin_index];
    loc.minor_bin_index = bit_scan_lsb(minor_bins_bitmap);

    *loc_out = loc;
    return 1;
}

    // No higher major row either. Fall back to the request's own
    // down-bin: binmap_up() may have overshot past a same-or-lower
    // major row that still holds an adequate block - O(minor_bins_count).
_fallback_down_bin: {
    locant down_loc = binmap_down(partitioner, size);

    size_t fallback_bitmap = partitioner->minor_bins_free_bitmaps[down_loc.major_bin_index];
    fallback_bitmap &= (~((size_t)0) << down_loc.minor_bin_index); // skip provably-too-small minor bins

    while (fallback_bitmap != 0) {
        size_t minor = bit_scan_lsb(fallback_bitmap);
        locant candidate_loc = (locant){ .major_bin_index = down_loc.major_bin_index, .minor_bin_index = minor };
        dpr_partition* head = partitioner->minor_bins_free_partitions[get_flat_minor_bin_index(partitioner, candidate_loc)];

        if (head && head->size >= size) {
            *loc_out = candidate_loc;
            return 1;
        }

        bitmap_set(&fallback_bitmap, minor, 0); // Clear and try next set bit
    }

    return 0; // No adequate block anywhere; failure
}
}

// ===========================
// API Implementation

// non-zero at success
int partitioner_create_blank_branch
(dpr_partitioner* partitioner, const dpr_partitioner_create_info* info, dpr_partition** first, dpr_partition** last) {
    *first = NULL;
    *last  = NULL;
    return 1; // success
}

// non-zero at success
int partitioner_create_realloc_branch
(dpr_partitioner* partitioner, const dpr_partitioner_create_info* info, dpr_partition** first, dpr_partition** last) {
    dpr_partitioner* old  = info->old_partitioner;
    dpr_partition*   part = old->physical_first_partition;

    // Ensure old parititioner align is same or greater than new
    // Else an defragmentation would be needed
    if (old->align_bytes < partitioner->align_bytes) return 0;

    // Inherit old partitions
    *first = old->physical_first_partition;

    // Rebuild free-list bitmap state
    while (1) { 
        if (is_partition_free(part)) free_list_insert_free_partition(partitioner, part);
        if (part->next_physical == NULL) break;
        part = part->next_physical;
    } *last = part;

    // Get occupied memory size from last partition
    size_t occupied_bytes = dpr_partition_query_offset(*last) + dpr_partition_query_size(*last);
    if (occupied_bytes > info->memory_bytes) return 0; // This does not fit - failure

    return 1; // success
}

// non-zero at success
int partitioner_create_defrag_branch
(dpr_partitioner* partitioner, const dpr_partitioner_create_info* info, dpr_partition** first, dpr_partition** last) {
    dpr_partitioner* old = info->old_partitioner;
    
    // Pass 1 - Try to put allocated partitions one by one in new span, regarding new align
    // Afterwards part is last partition pointer

    size_t position = 0; // offset cursor
    size_t p_count  = 0; // allocated partitions count
    dpr_partition* part = old->physical_first_partition;
    while (1) {
        if (!is_partition_free(part)) {
            position = (position + partitioner->align_bytes - 1) & ~(partitioner->align_bytes - 1); // align placement position
            position += part->size; p_count++; // advance
        }
        if (!part->next_physical) break;
        part = part->next_physical;
    }

    // Check whether defrag possible
    if (position > info->memory_bytes) return 0;

    // Pass 2 - Since defrag possible, do defrag - create moves and alter partitions
    size_t moves_itr = 0; dpr_defragmentation_move* moves = malloc((p_count + 1) * sizeof(dpr_defragmentation_move));
    if (!moves) return 0; // Allocation failed

    // New align is lesser, partitions gets closer to each other
    // Move left to right, copy backward
    if (old->align_bytes >= partitioner->align_bytes) {
        position = 0;  part = old->physical_first_partition;
        dpr_partition* left = NULL;
        while (part) {
            dpr_partition* next = part->next_physical;
      
            if (is_partition_free(part)) { free(part); part = next; continue;}                      // Free free partitions
            if (!*first) *first = part;                                                             // Set first partition
            position = (position + partitioner->align_bytes - 1) & ~(partitioner->align_bytes - 1); // Align up placement position

            // Emit defragmentation move
            moves[moves_itr++] = (dpr_defragmentation_move){
                .old_offset = part->offset + part->adjustment,
                .new_offset = position,
                .bytes      = part->size - part->adjustment
            };
            
            // Update partition
            *part = (dpr_partition){
                .size          = part->size,
                .offset        = position,
                .prev_physical = left,
                .next_physical = NULL
            }; mark_partition_used(part);

            // Link previous
            if (left) left->next_physical = part;

            // Advance
            position += dpr_partition_query_size(part);
            left = part; *last = part; part = next;
        }
    }
    // Align increased, strides between partitions gets bigger
    // Go right to left, copy forward
    else {
        dpr_partition* right = NULL;
        while (part) {
            dpr_partition* next = part->prev_physical;

            if (is_partition_free(part)) { free(part); part = next; continue;}      // Free free partitions
            if (!*last) *last = part;                                               // Set last partition
            position = (position - part->size) & ~(partitioner->align_bytes - 1);   // Align down placement position, advance backward

            // Emit defragmentation move
            moves[moves_itr++] = (dpr_defragmentation_move){
                .old_offset = part->offset + part->adjustment,
                .new_offset = position,
                .bytes      = part->size - part->adjustment
            };

            // Update partition
            *part = (dpr_partition){
                .size          = part->size,
                .offset        = position,
                .prev_physical = NULL,
                .next_physical = right
            }; mark_partition_used(part);

            // Link previous
            if (right) right->prev_physical = part;

            // Advance
            right = part; *first = part; part = next;
        }
    }

    moves[moves_itr].bytes = 0;             // Terminator move
    *info->defragmentation_moves = moves;   // Return moves to client

    return 1; // success
}

dpr_partitioner* dpr_create_partitioner(const dpr_partitioner_create_info* info) {
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
    dpr_partitioner* partitioner = calloc(1, sizeof(dpr_partitioner));
    dpr_partition*   partition   = calloc(1, sizeof(dpr_partition));
    if (!partitioner || !partition) {
        free(partitioner); free(partition);
        return NULL;
    }

    *partitioner = (dpr_partitioner){
        .memory_bytes               = info->memory_bytes,
        .minor_bins_count_log2      = MINOR_BINS_COUNT_LOG2,
        .major_bins_count           = (platform_bits - align_power_of_two),
        .minor_bins_count           = (size_t)1 << MINOR_BINS_COUNT_LOG2,
        .skipped_major_bins         = align_power_of_two,
        .align_bytes                = real_align,
        .major_bins_free_bitmap     = 0
    };

    // Create bitmaps
    partitioner->minor_bins_free_bitmaps    = calloc(partitioner->major_bins_count, sizeof(size_t));
    partitioner->minor_bins_free_partitions = calloc(
        partitioner->major_bins_count * partitioner->minor_bins_count, sizeof(dpr_partition*)
    );
    if (!partitioner->minor_bins_free_bitmaps || !partitioner->minor_bins_free_partitions) goto _fail;
    
    // Dispatch on inheritance
    int success = 1; dpr_partition *first = NULL, *last = NULL;
    if (!info->old_partitioner)            success &= partitioner_create_blank_branch  (partitioner, info, &first, &last);
    else if (!info->defragmentation_moves) success &= partitioner_create_realloc_branch(partitioner, info, &first, &last);
    else if (info->defragmentation_moves)  success &= partitioner_create_defrag_branch (partitioner, info, &first, &last);

    // Ensure succeeded
    if (!success) goto _fail;

    // Consume old partitioner
    if (info->old_partitioner) {
        free(info->old_partitioner->minor_bins_free_bitmaps);
        free(info->old_partitioner->minor_bins_free_partitions);
        free(info->old_partitioner);
    }

    // Set partitioner first physical state
    if (first)  partitioner->physical_first_partition = first;
    else        partitioner->physical_first_partition = partition;

    // Insert back rest of free space
    size_t occupied_bytes = !last ? 0 : dpr_partition_query_offset(last) + dpr_partition_query_size(last);
    size_t leftower_space = info->memory_bytes - occupied_bytes;
    if (leftower_space) {
        // Setup last free partition
        *partition = (dpr_partition){
            .adjustment     = 0,
            .offset         = occupied_bytes, 
            .size           = leftower_space,
            .prev_physical  = last,
            .next_physical  = NULL
        };

        // Inform previous partition
        if (last) last->next_physical = partition;

        // Insert partition
        mark_partition_free(partition);
        merge_free_partitions(partitioner, partition);
        free_list_insert_free_partition(partitioner, partition);
    }
    else {
        free(partition);
    }

    return partitioner;

_fail:
    free(partitioner->minor_bins_free_bitmaps); free(partitioner->minor_bins_free_partitions);
    free(partitioner); free(partition);
    return NULL;
}

void dpr_free_partitioner(dpr_partitioner* partitioner) {
    if (!partitioner) return;

    dpr_partition* part = partitioner->physical_first_partition;
    while (part) { 
        dpr_partition* next = part->next_physical; 
        free(part); part = next;
    }

    free(partitioner->minor_bins_free_bitmaps);
    free(partitioner->minor_bins_free_partitions);
    free(partitioner);
}

dpr_partition* dpr_partitioner_alloc_partition(dpr_partitioner* partitioner, size_t required_size) {
    if (required_size == 0 || required_size > partitioner->memory_bytes) return NULL;

    locant loc; if (!find_free_partition_for_size(partitioner, required_size, &loc)) return NULL;
    size_t flat_idx = get_flat_minor_bin_index(partitioner, loc);

    dpr_partition* partition = partitioner->minor_bins_free_partitions[flat_idx];
    free_list_remove_free_partition_given_locant(partitioner, partition, loc);

    // try to prepare partition for use, if failed back to free list
    if (!physical_prepare_partition_for_use(partitioner, partition, required_size)) {
        free_list_insert_free_partition(partitioner, partition); return NULL;
    };

    // since new node was created, update first partition
    physical_update_first_partition(partitioner, partition);
    return partition;
}

void dpr_partitioner_free_partition(dpr_partitioner* partitioner, dpr_partition* partition) {
    mark_partition_free(partition);
    merge_free_partitions(partitioner, partition);
    free_list_insert_free_partition(partitioner, partition);
}

size_t dpr_partition_query_offset(dpr_partition* partition) {
    return partition->offset + partition->adjustment;
}
size_t dpr_partition_query_size(dpr_partition* partition) {
    return partition->size - partition->adjustment;
}

#endif
