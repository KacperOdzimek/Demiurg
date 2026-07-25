/*
----------------------------------------------------------------
Contents
This file provides segmenter object, allowing segmentation of multiple uploads, into batches, restricted by bandwidth

----------------------------------------------------------------
Code info:
- dsg prefix
- DEMIURG_SEGMENTER_IMPL macro to build

----------------------------------------------------------------
Usage
- Create segmenter object
- Request uploads with dgs_segmenter_push_upload
- Get upload segmented upload jobs with dgs_segmenter_continue
- Loop till dgx_segmenter_query_empty
*/

#ifndef DEMIURG_SEGMENTER_H
#define DEMIURG_SEGMENTER_H

#include <stdint.h>

// Upload Request

typedef struct dgs_upload_request {
    uint64_t target;
    uint64_t offset;
    void*    source;
    uint64_t bytes;
} dgs_upload_request;

// Segmenter

typedef struct dgs_segmenter dgs_segmenter;
typedef struct dgs_segmenter_create_info {
    uint64_t        bandwidth;  // Upload bandwidth
    dgs_segmenter*  parent;     // Reuse resources
} dgs_segmenter_create_info;

dgs_segmenter* dgs_create_segmenter(dgs_segmenter_create_info* info);
void dgs_free_segmenter(dgs_segmenter*);

int  dgs_segmenter_upload(dgs_segmenter*, dgs_upload_request upload); // non-zero at success
void dgs_segmenter_continue(dgs_segmenter*, uint64_t* out_uploads_count, dgs_upload_request** out_first_upload);
int  dgx_segmenter_query_empty(dgs_segmenter*);

#endif

#ifdef DEMIURG_SEGMENTER_IMPL
#include <stdlib.h>
 
struct dgs_segmenter {
    uint64_t            bandwidth;              // Upload bandwidth
    dgs_upload_request* uploads;                // Uploads circular buffer 
    uint64_t            uploads_capacity;       // Buffer capacity
    uint64_t            uploads_first;          // First element index in buffer
    uint64_t            uploads_count;          // Uploads count in buffer
    uint64_t            first_trimmed;          // Part of the first upload we trimmed last time, so user can read
    dgs_upload_request* out_buffer;             // Scratch array copied uploads are handed to the caller through
    uint64_t            out_buffer_capacity;    // Capacity of out_buffer
};
 
dgs_segmenter* dgs_create_segmenter(dgs_segmenter_create_info* info) {
    if (info->bandwidth == 0) {dgs_free_segmenter(info->parent); return NULL;}
    if (info->parent) {
        info->parent->uploads_count = 0;
        info->parent->uploads_first = 0;
        info->parent->bandwidth     = info->bandwidth;
        info->parent->first_trimmed = 0;
        return info->parent;
    }
    dgs_segmenter* segmenter = malloc(sizeof(dgs_segmenter));
    if (!segmenter) return NULL;
    *segmenter = (dgs_segmenter){.bandwidth = info->bandwidth};
    return segmenter;
}
 
void dgs_free_segmenter(dgs_segmenter* segmenter) {
    if (!segmenter) return;
    free(segmenter->uploads);
    free(segmenter->out_buffer);
    free(segmenter);
}
 
int dgs_segmenter_upload(dgs_segmenter* segmenter, dgs_upload_request upload) {
    if (upload.bytes == 0) return 1;
    if (segmenter->uploads_count == segmenter->uploads_capacity) {
        uint64_t new_caps[2] = {
            segmenter->uploads_capacity ? segmenter->uploads_capacity * 2 : 16, 
            segmenter->uploads_capacity ? segmenter->uploads_capacity + 1 : 16, 
        };
        for (int i = 0; i < 2; i++) {
            uint64_t            new_cap = new_caps[i];
            dgs_upload_request* new_upl = malloc(new_cap * sizeof(dgs_upload_request));
            if (!new_upl) continue;
            for (uint64_t j = 0; j < segmenter->uploads_count; j++) {
                uint64_t pos = (segmenter->uploads_first + j) % segmenter->uploads_capacity;
                new_upl[j] = segmenter->uploads[pos];
            }
            free(segmenter->uploads);
            segmenter->uploads_capacity = new_cap;
            segmenter->uploads_first = 0;
            segmenter->uploads = new_upl;
            goto _set;
        }
        return 0;
    }
_set:
    uint64_t pos = (segmenter->uploads_first + segmenter->uploads_count) % segmenter->uploads_capacity;
    segmenter->uploads[pos] = upload; segmenter->uploads_count++; return 1;
} 
 
void dgs_segmenter_continue(dgs_segmenter* segmenter, uint64_t* out_uploads_count, dgs_upload_request** out_first_upload) {
    if (dgx_segmenter_query_empty(segmenter)) {*out_uploads_count = 0; return;}
    uint64_t bytes_budget = segmenter->bandwidth;
 
    // Apply any trim left over from the previous call to the first upload
    if (segmenter->first_trimmed) {
        dgs_upload_request* first = &segmenter->uploads[segmenter->uploads_first];
        first->source += first->bytes;              // previously uploaded
        first->offset += first->bytes;              // advance
        first->bytes   = segmenter->first_trimmed;  // left to be uploaded
    }
 
    uint64_t itr        = 0; // uploads fully consumed, to be removed from the ring buffer
    uint64_t copy_count = 0; // uploads copied into out_buffer for the caller (includes a trailing partial one)
    segmenter->first_trimmed = 0;
    while (bytes_budget && itr < segmenter->uploads_count) {
        uint64_t pos = (segmenter->uploads_first + itr) % segmenter->uploads_capacity;
        dgs_upload_request* upload = &segmenter->uploads[pos];
 
        if (copy_count >= segmenter->out_buffer_capacity) {
            uint64_t new_cap = segmenter->out_buffer_capacity ? segmenter->out_buffer_capacity * 2 : 16;
            dgs_upload_request* new_buf = realloc(segmenter->out_buffer, new_cap * sizeof(dgs_upload_request));
            if (!new_buf) break; // cannot grow further; give back what we have 
            segmenter->out_buffer          = new_buf;
            segmenter->out_buffer_capacity = new_cap;
        }
 
        if (upload->bytes <= bytes_budget) {
            bytes_budget -= upload->bytes;
            segmenter->out_buffer[copy_count++] = *upload;
            itr++; continue;
        }
 
        segmenter->first_trimmed = upload->bytes - bytes_budget;
        upload->bytes -= segmenter->first_trimmed;
        segmenter->out_buffer[copy_count++] = *upload;
        break;
    }
 
    segmenter->uploads_count -= itr;
    segmenter->uploads_first  = (segmenter->uploads_first + itr) % segmenter->uploads_capacity;
 
    *out_uploads_count = copy_count;
    *out_first_upload  = segmenter->out_buffer;
}
 
int dgx_segmenter_query_empty(dgs_segmenter* segmenter) {
    return segmenter->uploads_count == 0;
}
 
#endif
