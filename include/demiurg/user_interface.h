/*
----------------------------------------------------------------
Contents:
This file implements dui ui system.

----------------------------------------------------------------
Code info:
- dui prefix
- DEMIURG_USER_INTERFACE_IMPL macro to build
- partitioner.h dependent
- linear_algebra.h dependent
- graphics.h dependent
- font.h dependent
- segmenter.h dependent

----------------------------------------------------------------
Usage: See dedicated documentation
*/


/*
    Depedencies
*/

#include "partitioner.h"
#include "linear_algebra.h"
#include "graphics.h"
#include "font.h"

/*
    Implementation Injections
    Define in same file as user interface implementation
*/

#ifdef DEMIURG_USER_INTERFACE_IMPL
    typedef struct dui_length     dui_length;
    typedef struct dui_text_data  dui_text_data;
    typedef struct dui_box_data   dui_box_data;

    // returns non-zero at success (if returned is valid pointer)
    int dui_injection_query_font (const char* font,  dfont_font**  font_out);
    int dui_injection_query_image(const char* image, dgx_texture** texture_out, dgx_uv_2d* uv_out);
#endif // DEMIURG_USER_INTERFACE_IMPL

/*
    Header
*/

#ifndef DEMIURG_USER_INTERFACE_H
#define DEMIURG_USER_INTERFACE_H

// ===========================
// Forwards

typedef struct dui_type   dui_type;
typedef struct dui_node   dui_node;
typedef struct dui_cache  dui_cache;
typedef struct dui_shared dui_shared;
typedef struct dui_frames dui_frames;

// ===========================
// Layout Length

// variable representing infinte length
// not set to int max, to avoid overflows in implementation
// needs to be increased if you are rendering on a (64+)K screen
const static int dui_inf_length = 64 * 1000;

// structure representing 1d length
// min  - minimal size element can be rendered with
// max  - maximal size element can be rendered with
// flex - relative grow speed, compared to other elements inside an container (row/column)
// each length object is expected to met:
// min  <= max
// flex >= 0
typedef struct dui_length {
    int   min;  // minimum dimension
    int   max;  // maximum dimension
    float flex; // flex ratio
} dui_length;

// ===========================
// Colors

// basic 32 bit color
typedef struct dui_color {
    unsigned char r, g, b, a;
} dui_color;

// runtime hex to dui_color conversion
// letters case does not matter, '#' prefix is required
// if hex[7] is not '\0', then alpha channel is read, else it is set to FF
// LUI_HEX <- compile time alternative
static inline dui_color dui_hex(const char* hex);

// LUI_HEX <- compile time LUI_HEX alternative (definied later in the file)

// ===========================
// Cursor

typedef struct dui_cursor_state {
    int     left_down,  right_down;
    int     position_x, position_y;
    float   scroll_delta;
} dui_cursor_state;

typedef void(dui_cursor_handle_func_signature)(
    const void*             node_data,      // node data
    void*                   auxilary,       // node auxilary buffer if requested by type
    dui_cursor_state*       state,          // state with fields possibly consumed by previous handles  
    const dui_cursor_state* raw_state,      // untouched state
    int                     hovered,        // whether cursor is inside node and no upper node was
    int                     raw_hovered     // whether cursor is inside node
);
typedef dui_cursor_handle_func_signature* dui_cursor_handle_func;

// ===========================
// Node Typedefs

typedef struct dui_node_layout_state {
    dui_length              measured_width;     // desired width  of this node
    dui_length              measured_height;    // desired height of this node
    int                     given_width;        // received width
    int                     given_height;       // received height
    int                     hori_offset;        // node center horizontal offset from parent center
    int                     vert_offset;        // node center vertical offset from parent center
} dui_node_layout_state;

typedef void (dui_node_auxilary_destructor_func_signature)(
    void*                   auxilary            // node auxilary buffer - do not free it
);
typedef dui_node_auxilary_destructor_func_signature* dui_node_auxilary_destructor_func;

typedef void(dui_node_layout_func_signature)(
    const void*             node_data,          // node data
    dui_node_layout_state*  node_state,         // node own state
    size_t                  children_count,     // node children count
    dui_node_layout_state** children_states,    // node children states
    void*                   auxilary            // node auxilary buffer if requested by type
);
typedef dui_node_layout_func_signature* dui_node_layout_func;

typedef void(dui_node_render_func_signature)(
    const void*             node_data,          // node data
    dla_mat2x3*             transform,          // given transform, can be changed
    int                     resolution_x,       // screen resolution x
    int                     resolution_y,       // screen resolution y
    void*                   auxilary            // node auxilary buffer if requested by type
);
typedef dui_node_render_func_signature* dui_node_render_func;

typedef struct dui_type {
    // Structure

    // Whether child pointer in node means single node
    // Or and array terminated with LUI_ARRAY_END
    int     array_child;

    // This field can be used to request a cache-owned state per node, sized exactly auxilary_bytes bytes.
    // This state will be shared across all node passes, and given to user in callback functions. 
    // If state_bytes == 0, the pointer will be NULL.
    size_t  auxilary_bytes;

    // This function will be called when auxilary storage is freed
    // You can use it to free some auxilary-owned memory
    dui_node_auxilary_destructor_func auxilary_destructor;

    // Auxilary Stage

    // Auxilary stage
    // Allow for node own data changes (eg. caching some preprocessed state)
    // Unless it is convenient, this pass shall not be used, to preserve data-oriented-design,
    // and composability. Used to generate text format for GPU in implementation.
    dui_node_layout_func    auxilary;

    // Layout Stages

    // First layout stage
    // Generates desired nodes widths, bottom-up
    // IN:  [children measured width]
    // OUT: [own measured width]
    dui_node_layout_func    width_measure;

    // Second layout stage
    // Generates actuall nodes widths, top-down
    // IN:  [width measurements, own given width]
    // OUT: [children given width]
    dui_node_layout_func    width_distribute;

    // Third layout stage
    // Generates desired nodes widths, bottom-up
    // IN:  [given widths, children measured heights]
    // OUT: [own measured height]
    dui_node_layout_func    height_measure;

    // Fourth layout stage
    // Generates actuall nodes heights, top-down
    // IN:  [given widths, measured heights, own given height]
    // OUT  [children given heights]
    dui_node_layout_func    height_distribute;

    // Fifth layout stage
    // Position nodes on screen, top-down
    // IN:  [all widths and heights]
    // OUT: [node offset from ]
    dui_node_layout_func    position;

    // Rendering Stages

    // First render stage
    // Allow altering children render transforms, top down
    // IN:  [complete layout states, parent render transform]
    // OUT: [own and children render transform]
    dui_node_render_func    transform;
} dui_type;

typedef enum dui_flag {
    dui_flag_instanced_data     = 1 << 0,
    dui_flag_instanced_child    = 1 << 1,
    dui_flag_ignore_min_width   = 1 << 2,
    dui_flag_ignore_min_height  = 1 << 3,
    dui_flag_ignore_max_width   = 1 << 4,
    dui_flag_ignore_max_height  = 1 << 5,
} dui_flag;

typedef struct dui_node {
    const dui_type* type;
    const uint32_t  flags;
    
    union {
        const dui_node* child;
        size_t          child_offset;
    };

    union {
        const void*     data;
        size_t          data_offset;
    };
} dui_node;

// Sentinel value to mark array end
#define LUI_ARRAY_END (dui_node){.type = NULL, .child = NULL, .data = NULL}

// ===========================
// Predefinied Functions
// Those implement basic box/overlay behavior, used by most nodes

// width = (min = max(children mins), max = max(children max), flex = 1.0f if min != max, else 0)
dui_node_layout_func_signature dui_overlay_width_measure_func;

// children width = parent width, with applied maxes
dui_node_layout_func_signature dui_overlay_width_distribute_func;

// height = (min = max(children mins), max = max(children max), flex = 1.0f if min != max, else 0)
dui_node_layout_func_signature dui_overlay_height_measure_func;

// children height = parent height, with applied maxes
dui_node_layout_func_signature dui_overlay_height_distribute_func;

// centers children inside parent
dui_node_layout_func_signature dui_overlay_position_func;

// ===========================
// Architectural Node Types

// Sets instance pointer to own data value
// Data shall be arbitrary pointer (or offset in current instance) to instance structure
extern const dui_type dui_instance_type;

// Layout-rebuild gate for the subtree - children layout will only
// be rebuilt if invalidation node was marked with a proper dirty flag
// No data, single child
extern const dui_type dui_invalidation_type;
typedef enum dui_invalidation_flag {
    dui_invalidation_flag_auxilary          = 63,
    dui_invalidation_flag_width_measure     = 62,
    dui_invalidation_flag_width_distribute  = 60,
    dui_invalidation_flag_height_measure    = 56,
    dui_invalidation_flag_height_distribute = 48,
    dui_invalidation_flag_position          = 32,
    dui_invalidation_flag_none              = 0,
    dui_invalidation_flag_all               = 63,
} dui_invalidation_flag;
typedef struct dui_invalidation_data {
    dui_invalidation_flag flag_consumable;
    dui_invalidation_flag flag_always;
} dui_invalidation_data;

// ===========================
// Layout Node Types

// During layout, overwrites selected fields with provided values
// Data is dui_sizebox_data, single child
extern const dui_type dui_sizebox_type;
typedef enum dui_sizebox_overwrite_flag {
    dui_sizebox_overwrite_none        = 0,
    dui_sizebox_overwrite_all         = 255,
    dui_sizebox_overwrite_all_width   = 7,
    dui_sizebox_overwrite_all_height  = 56,

    dui_sizebox_overwrite_width_min   = 1 << 0,
    dui_sizebox_overwrite_width_max   = 1 << 1,
    dui_sizebox_overwrite_width_flex  = 1 << 2,

    dui_sizebox_overwrite_height_min  = 1 << 3,
    dui_sizebox_overwrite_height_max  = 1 << 4,
    dui_sizebox_overwrite_height_flex = 1 << 5
} dui_sizebox_overwrite_flag;
typedef struct dui_sizebox_data {
    dui_sizebox_overwrite_flag  flag;
    dui_length                  width;
    dui_length                  height;    
} dui_sizebox_data;

// Padds child inside self
// Data is dui_padding_data, single child
extern const dui_type dui_padding_type;
typedef struct dui_padding_data {
    dui_length left, right, top, bottom;
} dui_padding_data;

// Layouts children one on another
// The first child is deepest, rendered first
// No data, array children
extern const dui_type dui_overlay_type;

// Layouts children in a row, left to right
// Data is dui_row_data, array children
extern const dui_type dui_row_type;
typedef struct dui_row_data {
    float           vertical_align;     // 0 - align top,  0.5 - align center, 1.0 - align bottom, other values also work
    dui_length      spacing;            // spacing between children
} dui_row_data;

// Layouts children in a column, top to down
// Data is dui_column_data, array children
extern const dui_type dui_column_type;
typedef struct dui_column_data {
    float           horizontal_align;   // 0 - align left,  0.5 - align center, 1.0 - align right, other values also work
    dui_length      spacing;            // spacing between children
} dui_column_data;

// ===========================
// Rendering Node Types

// Constrains rendering to own dimensions
// No data, single child
extern const dui_type dui_clipbox_type;

// Adds node depth offset
// Decreasing depth means going 'into' the screen
// Data is dui_depth_data, ingle childed
extern const dui_type dui_depth_type;
typedef struct dui_depth_data {
    short depth_change;
} dui_depth_data;

// Box render primitive
// Data is dui_box_data, single child
extern const dui_type dui_box_type;
typedef struct dui_box_data {
    dui_color       tint;       // box color
    const char*     image;      // image name/path, may be NULL
    uint32_t        shader;     // shader effect index
} dui_box_data;

// Text render primitive
// Data is dui_text_data, single child
extern const dui_type dui_text_type;
typedef struct dui_text_data {
    unsigned int    size;       // font size
    const char*     font;       // font name/path
    const char*     text;       // text pointer
    dui_color       tint;       // text color modyficator
    uint32_t        shader;     // shader effect index
} dui_text_data;

// ===========================
// Cursor Input Node Types

// This node sets cursor handle function for subtree to own data
// Data is dui_cursor_handle_func, single child
extern const dui_type dui_cursor_input_handle_type;

// This node pushes cursor input box
// If handle was provided higher in the tree
// during cache update, callback will be called for this box
// Data of this node will be passed to callback
// Data arbitrary, single child
extern const dui_type dui_cursor_input_box_type;

// ===========================
// Cache

dui_cache* dui_create_cache();
void dui_free_cache(dui_cache*);

void dui_update_cache(
    dui_cache*              cache,
    const dui_node*         root,
    int                     resolution_x,
    int                     resolution_y,
    dui_cursor_state        cursor_state
);

// ===========================
// Rendering API

typedef struct dui_shared_create_info {
    dgx_pipeline_attachment_state   attachment_state;
    dgx_shader_create_info          vertex_shader_info;
    dgx_shader_create_info          pixel_shader_info;
} dui_shared_create_info;

dui_shared* dui_create_shared(dgx_hardware*, const dui_shared_create_info*);
void dui_free_shared(dui_shared*);

typedef struct dui_frames_create_info {
    dui_shared* shared;
    uint32_t    count;
} dui_frames_create_info;

dui_frames* dui_create_frames(dgx_hardware*, const dui_frames_create_info*);
void dui_free_frames(dui_frames*);

void dui_upload_cache(
    dui_cache*          cache,
    dui_shared*         shared,
    dui_frames*         frames,
    uint32_t            frame_idx,
    uint8_t             transfer_work_group_index,
    uint8_t             command_list_allocator_index,
    dgx_staging_memory* staging_memory,
    uint64_t            staging_memory_region_offset,
    uint64_t            staging_memory_region_size,
    dgx_timeline*       signal_timeline,
    uint64_t            signal_value
);

void dui_gcmd_render(
    dui_frames*         frames,
    uint32_t            frame
);

// ===========================
// Hex to Ui Color Implementations

// convert single hex char to value at compile time
#define LUI_HEX_VAL(c) ( ((c) >= '0' && (c) <= '9') ? ((c)-'0') :    \
                        ((c) >= 'a' && (c) <= 'f') ? ((c)-'a'+10) : \
                        ((c) >= 'A' && (c) <= 'F') ? ((c)-'A'+10) : 0 )

// convert two hex chars to byte at compile time
#define LUI_HEX_BYTE(c1, c2) ((LUI_HEX_VAL(c1) << 4) | LUI_HEX_VAL(c2))

static inline dui_color dui_hex(const char* hex) {
    dui_color result;
    result.r = LUI_HEX_BYTE(hex[1], hex[2]);
    result.g = LUI_HEX_BYTE(hex[3], hex[4]);
    result.b = LUI_HEX_BYTE(hex[5], hex[6]);

    // if 8 digits after #, read alpha
    if (hex[7] != '\0' && hex[8] != '\0') result.a = LUI_HEX_BYTE(hex[7], hex[8]);
    else result.a = 0xFF;

    return result;
}

// compile time dui_color from hex builder
// allows both lower and upper case letters
// may include alpha (8 hex digits) or not (6 hex digits)
// '#' prefix required
#define LUI_HEX(s) (dui_color){ \
    LUI_HEX_BYTE(s[1], s[2]), \
    LUI_HEX_BYTE(s[3], s[4]), \
    LUI_HEX_BYTE(s[5], s[6]), \
    (sizeof(s) > 8 ? LUI_HEX_BYTE(s[7], s[8]) : 0xFF) \
}

#endif // DEMIURG_USER_INTERFACE_H

#ifdef DEMIURG_USER_INTERFACE_IMPL

#include <stdlib.h>
#include "segmenter.h"

// Implementation Notes:
// 1 - last_frame_used_in_render values reference
//  last_frame_used_in_render is used to clear hashmap from dead nodes
/*
    0     - empty cell
    1     - imposible value, to force garbage collection on all
    2     - tombstone
    3-255 - rendered at frame of index
*/

#define LAST_FRAME_USED_IN_RENDER_EMPTY      0
#define LAST_FRAME_USED_IN_RENDER_IMPOSIBLE  1
#define LAST_FRAME_USED_IN_RENDER_TOMBSTONE  2
#define LAST_FRAME_USED_IN_RENDER_FIRST      3

// ===========================
// Math helpers

static inline int min_int(int a, int b) { return a < b ? a : b; }
static inline int max_int(int a, int b) { return a < b ? b : a; }

static inline int limit_length(int length, dui_length limits) {
    if (length > limits.max) length = limits.max;
    if (length < limits.min) length = limits.min;
    return length;
}

static inline int limit_length_gain(int current, dui_length limit, int proposed) {
    if (current + proposed < limit.min) return limit.min - current;
    if (current + proposed > limit.max) return limit.max - current;
    return proposed;
}

static inline dla_mat2x3 mat2x3_scale(dla_mat2x3 m, float sx, float sy) {
    m.m[0][0] *= sx;  m.m[0][1] *= sy;
    m.m[1][0] *= sx;  m.m[1][1] *= sy;
    return m;
}

static inline dla_mat2x3 mat2x3_offset(dla_mat2x3 m, float ox, float oy) {
    m.m[2][0] += ox; m.m[2][1] += oy;
    return m;
}

int is_point_in_transformed_box(dla_mat2x3 t, float px, float py) {
    // Translation
    float tx = t.m[2][0];
    float ty = t.m[2][1];

    // Linear part (column-major)
    float a = t.m[0][0];
    float b = t.m[0][1];
    float c = t.m[1][0];
    float d = t.m[1][1];

    // Point relative to box center
    float x = px - tx;
    float y = py - ty;

    // Inverse of 2×2 matrix
    float det = a * d - b * c;
    if (det == 0.0f) return 0; // degenerate transform

    float inv_det = 1.0f / det;

    // Local coordinates
    float lx = ( d * x - c * y) * inv_det;
    float ly = (-b * x + a * y) * inv_det;

    return lx >= -1.0f && lx <= 1.0f && ly >= -1.0f && ly <= 1.0f;
}

// ===========================
// Types helper

#define box_behavior_type (dui_type){                           \
    .array_child        = 0,                                    \
    .auxilary_bytes     = 0,                                    \
    .auxilary           = NULL,                                 \
    .width_measure      = dui_overlay_width_measure_func,       \
    .width_distribute   = dui_overlay_width_distribute_func,    \
    .height_measure     = dui_overlay_height_measure_func,      \
    .height_distribute  = dui_overlay_height_distribute_func,   \
    .position           = dui_overlay_position_func,            \
    .transform          = NULL                                  \
} 

// ===========================
// Instance type
// This type is specially handled in pass implementation
const dui_type dui_instance_type = box_behavior_type;

// ===========================
// Invalidation type
// This type is specially handled in pass implementation
const dui_type dui_invalidation_type = box_behavior_type;

// ===========================
// Overlay Type

void dui_overlay_width_measure_func(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)node_data; (void)auxilary; dui_length own = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        dui_length child = children_states[i]->measured_width;
        own.min  = max_int(own.min, child.min);
        own.max  = max_int(own.max, child.max);
    }

    if (own.min != own.max) own.flex = 1.0f;
    node_state->measured_width = own;
}

void dui_overlay_width_distribute_func(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)node_data; (void)auxilary;

    for (size_t i = 0; i < children_count; ++i) {
        children_states[i]->given_width = limit_length(node_state->given_width, children_states[i]->measured_width);
    }
}

void dui_overlay_height_measure_func(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)node_data; (void)auxilary; dui_length own = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        dui_length child = children_states[i]->measured_height;
        own.min  = max_int(own.min, child.min);
        own.max  = max_int(own.max, child.max);
    }

    if (own.min != own.max) own.flex = 1.0f;
    node_state->measured_height = own;
}

void dui_overlay_height_distribute_func(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)node_data; (void)auxilary;

    for (size_t i = 0; i < children_count; ++i) {
        children_states[i]->given_height = limit_length(node_state->given_height, children_states[i]->measured_height);
    }
}

void dui_overlay_position_func(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)node_data; (void)auxilary;

    for (size_t i = 0; i < children_count; ++i) {
        children_states[i]->hori_offset = 0;
        children_states[i]->vert_offset = 0;
    }
}

const dui_type dui_overlay_type = {
    .array_child        = 1,
    .auxilary_bytes     = 0,
    .auxilary           = NULL,
    .width_measure      = dui_overlay_width_measure_func,
    .width_distribute   = dui_overlay_width_distribute_func,
    .height_measure     = dui_overlay_height_measure_func,
    .height_distribute  = dui_overlay_height_distribute_func,
    .position           = dui_overlay_position_func,
    .transform          = NULL
};

// ===========================
// Sizebox Type

void sizebox_width_measure(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    const dui_sizebox_data* data = node_data;
    dui_overlay_width_measure_func(node_data, node_state, children_count, children_states, auxilary);
    if (data->flag & dui_sizebox_overwrite_width_min)   node_state->measured_width.min   = data->width.min;
    if (data->flag & dui_sizebox_overwrite_width_max)   node_state->measured_width.max   = data->width.max;
    if (data->flag & dui_sizebox_overwrite_width_flex)  node_state->measured_width.flex  = data->width.flex;
}

void sizebox_height_measure(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    const dui_sizebox_data* data = node_data;
    dui_overlay_height_measure_func(node_data, node_state, children_count, children_states, auxilary);
    if (data->flag & dui_sizebox_overwrite_height_min)  node_state->measured_height.min  = data->height.min;
    if (data->flag & dui_sizebox_overwrite_height_max)  node_state->measured_height.max  = data->height.max;
    if (data->flag & dui_sizebox_overwrite_height_flex) node_state->measured_height.flex = data->height.flex;
}

const dui_type dui_sizebox_type = {
    .array_child        = 0,
    .auxilary_bytes     = 0,
    .auxilary           = NULL,
    .width_measure      = sizebox_width_measure,
    .width_distribute   = dui_overlay_width_distribute_func,
    .height_measure     = sizebox_height_measure,
    .height_distribute  = dui_overlay_height_distribute_func,
    .position           = dui_overlay_position_func,
    .transform          = NULL
};

// ===========================
// Padding Type

static inline int padding_distribute_length(
    int* a, dui_length al,
    int* b, dui_length bl,
    int* c, dui_length cl,
    int remaining
) {
    for (int pass = 0; pass < 3 && remaining > 0; pass++) {
        float tf = 0.0f;
        if (*a < al.max) tf += al.flex;
        if (*b < bl.max) tf += bl.flex;
        if (*c < cl.max) tf += cl.flex;
        if (tf <= 0.0f) break;

        int ga = (*a < al.max && al.flex > 0.0f) ? limit_length_gain(*a, al, (int)((float)remaining * al.flex / tf)) : 0;
        int gb = (*b < bl.max && bl.flex > 0.0f) ? limit_length_gain(*b, bl, (int)((float)remaining * bl.flex / tf)) : 0;
        int gc = (*c < cl.max && cl.flex > 0.0f) ? limit_length_gain(*c, cl, (int)((float)remaining * cl.flex / tf)) : 0;

        if (ga + gb + gc == 0) break; // all remaining too small after int cast
        *a += ga; *b += gb; *c += gc;
        remaining -= ga + gb + gc;
    }
    return remaining;
}

void padding_width_measure(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary;
    const dui_padding_data* data = node_data;
    dui_length own = {0, 0, 0.0f};
    
    int child_min = 0, child_max = 0;
    if (children_count > 0) {
        child_min = children_states[0]->measured_width.min;
        child_max = children_states[0]->measured_width.max;
    }

    int w_min = data->left.min + child_min + data->right.min;
    int w_max = data->left.max + child_max + data->right.max;

    node_state->measured_width = (dui_length){
        .min  = w_min,
        .max  = w_max,
        .flex = (w_min != w_max) ? 1.0f : 0.0f,
    };
}

void padding_width_distribute(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary; if (children_count == 0) return;
    const dui_padding_data* data = node_data;
    dui_node_layout_state* child = children_states[0];

    // Give every element its minimum
    int left_w  = data->left.min;
    int right_w = data->right.min;
    int child_w = child->measured_width.min;

    // Divide remaining space, give leftover to child
    int remaining = node_state->given_width - left_w - right_w - child_w;
    if (remaining > 0) {
        remaining = padding_distribute_length(
            &left_w, data->left, &right_w, data->right, &child_w, child->measured_width, remaining
        ); child_w += limit_length_gain(child_w, child->measured_width, remaining);
    }

    // Assign child width and position
    child->given_width = child_w;
    child->hori_offset = (left_w - right_w) / 2;
}

void padding_height_measure(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary; const dui_padding_data* data = node_data;

    int child_min = 0, child_max = 0;
    if (children_count > 0) {
        child_min = children_states[0]->measured_height.min;
        child_max = children_states[0]->measured_height.max;
    }

    int h_min = data->top.min + child_min + data->bottom.min;
    int h_max = data->top.max + child_max + data->bottom.max;

    node_state->measured_height = (dui_length){
        .min  = h_min,
        .max  = h_max,
        .flex = (h_min != h_max) ? 1.0f : 0.0f,
    };
}

void padding_height_distribute(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary; if (children_count == 0) return;
    const dui_padding_data* data = node_data;
    dui_node_layout_state* child = children_states[0];

    // Give every element its minimum
    int top_h    = data->top.min;
    int bottom_h = data->bottom.min;
    int child_h  = child->measured_height.min;

    // Divide remaining space, give leftover to child
    int remaining = node_state->given_height - top_h - bottom_h - child_h;
    if (remaining > 0) {
        remaining = padding_distribute_length(
            &top_h, data->top, &bottom_h, data->bottom, &child_h, child->measured_height, remaining
        ); child_h += limit_length_gain(child_h, child->measured_height, remaining);
    }

    // Assign child width and position
    child->given_height = child_h;
    child->vert_offset  = (bottom_h - top_h) / 2;
}

const dui_type dui_padding_type = {
    .array_child        = 0,
    .auxilary_bytes     = 0,
    .auxilary           = NULL,
    .width_measure      = padding_width_measure,
    .width_distribute   = padding_width_distribute,
    .height_measure     = padding_height_measure,
    .height_distribute  = padding_height_distribute,
    .position           = NULL,
    .transform          = NULL
};

// ===========================
// Row Type

void row_width_measure(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary; 
    const dui_row_data* data = (const dui_row_data*)node_data;
    dui_length          own  = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        dui_length child = children_states[i]->measured_width;
        own.min += child.min; own.max += child.max;
    }

    size_t spaces = children_count ? children_count - 1 : 0;
    own.min += spaces * data->spacing.min;

    if (own.max != dui_inf_length && data->spacing.max != dui_inf_length) own.max += spaces * data->spacing.max;
    else own.max = dui_inf_length;

    if (own.min != own.max) own.flex = 1.0f;
    node_state->measured_width = own;
}

void row_width_distribute(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary; const dui_row_data* data = (const dui_row_data*)node_data;

    // Find spaces count
    size_t spaces_count = children_count ? children_count - 1 : 0;

    // Minimal pass
    int   used_width = 0;
    float flexsum    = 0.0f;
    int   spacing    = data->spacing.min;
    for (size_t i = 0; i < children_count; ++i) {
        flexsum += children_states[i]->measured_width.flex;
        children_states[i]->given_width = children_states[i]->measured_width.min;
        used_width += children_states[i]->given_width;
    }
    flexsum += data->spacing.flex;
    used_width += spaces_count * data->spacing.min;

    // Divide extra space
    int left_width = node_state->given_width - used_width;
    if (left_width < 0) left_width = 0;
    while (left_width) {
        float next_flexsum = 0.0f;
        int   partitioned  = 0;
        
        // add to spacing
        if (spacing < data->spacing.max) {
            int gain = (int)(left_width * (data->spacing.flex / flexsum));
            if (spaces_count) gain /= (int)spaces_count;
            gain = limit_length_gain(spacing, data->spacing, gain);

            spacing     += gain; 
            partitioned += gain * spaces_count;

            if (spacing != data->spacing.max) next_flexsum += data->spacing.flex;
        }

        // add to children
        for (size_t i = 0; i < children_count; ++i) {
            dui_length  m = children_states[i]->measured_width;
            int* assigned = &children_states[i]->given_width;
            if (*assigned == m.max) continue;   // maxed

            int gain = (int)(left_width * (m.flex / flexsum));
            gain = limit_length_gain(*assigned, m, gain);

            *assigned   += gain;
            partitioned += gain;

            if (*assigned != m.max) next_flexsum += m.flex;
        }

        // if failed to divide the space, break
        if (partitioned == 0) break;

        left_width -= partitioned;
        flexsum     = next_flexsum;
    }

    // Position children in horizontal axis
    int cursor_x = -node_state->given_width / 2;
    for (size_t i = 0; i < children_count; ++i) {
        dui_node_layout_state* child = children_states[i];
        cursor_x += child->given_width / 2;
        child->hori_offset = cursor_x;
        cursor_x += child->given_width / 2 + spacing;
    }
}

void row_position(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary; const dui_row_data* data = (const dui_row_data*)node_data;

    // Position children in vertial axis
    for (size_t i = 0; i < children_count; ++i) {
        dui_node_layout_state* child = children_states[i];
        child->vert_offset = (node_state->given_height - child->given_height) * (0.5f - data->vertical_align);
    }
}

const dui_type dui_row_type = {
    .array_child        = 1,
    .auxilary_bytes     = 0,
    .auxilary           = NULL,
    .width_measure      = row_width_measure,
    .width_distribute   = row_width_distribute,
    .height_measure     = dui_overlay_height_measure_func,
    .height_distribute  = dui_overlay_height_distribute_func,
    .position           = row_position,
    .transform          = NULL
};

// ===========================
// Column Type

void column_height_measure(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary;
    const dui_column_data* data = (const dui_column_data*)node_data;
    dui_length             own  = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        dui_length child = children_states[i]->measured_height;
        own.min += child.min; own.max += child.max;
    }

    size_t spaces = children_count ? children_count - 1 : 0;
    own.min += spaces * data->spacing.min;

    if (own.max != dui_inf_length && data->spacing.max != dui_inf_length) own.max += spaces * data->spacing.max;
    else own.max = dui_inf_length;

    if (own.min != own.max) own.flex = 1.0f;
    node_state->measured_height = own;
}

void column_height_distribute(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary;
    const dui_column_data* data = (const dui_column_data*)node_data;

    // Find spaces count
    size_t spaces_count = children_count ? children_count - 1 : 0;

    // Minimal pass
    int   used_height = 0;
    float flexsum     = 0.0f;
    int   spacing     = data->spacing.min;
    for (size_t i = 0; i < children_count; ++i) {
        flexsum += children_states[i]->measured_height.flex;
        children_states[i]->given_height = children_states[i]->measured_height.min;
        used_height += children_states[i]->given_height;
    }
    flexsum += data->spacing.flex;
    used_height += spaces_count * data->spacing.min;

    // Divide extra space
    int left_height = node_state->given_height - used_height;
    if (left_height < 0) left_height = 0;
    while (left_height) {
        float next_flexsum = 0.0f;
        int   partitioned  = 0;

        // add to spacing
        if (spacing < data->spacing.max) {
            int gain = (int)(left_height * (data->spacing.flex / flexsum));
            if (spaces_count) gain /= (int)spaces_count;
            gain = limit_length_gain(spacing, data->spacing, gain);

            spacing     += gain;
            partitioned += gain * spaces_count;

            if (spacing != data->spacing.max) next_flexsum += data->spacing.flex;
        }

        // add to children
        for (size_t i = 0; i < children_count; ++i) {
            dui_length  m = children_states[i]->measured_height;
            int* assigned = &children_states[i]->given_height;
            if (*assigned == m.max) continue;   // maxed

            int gain = (int)(left_height * (m.flex / flexsum));
            gain = limit_length_gain(*assigned, m, gain);

            *assigned   += gain;
            partitioned += gain;

            if (*assigned != m.max) next_flexsum += m.flex;
        }

        // if failed to divide the space, break
        if (partitioned == 0) break;

        left_height -= partitioned;
        flexsum      = next_flexsum;
    }

    // Position children in vertical axis
    int cursor_y = node_state->given_height / 2;
    for (size_t i = 0; i < children_count; i++) {
        dui_node_layout_state* child = children_states[i];
        cursor_y -= child->given_height / 2;
        child->vert_offset = cursor_y;
        cursor_y -= child->given_height / 2 + spacing;
    }
}

void column_position(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary; const dui_column_data* data = (const dui_column_data*)node_data;

    // Position children in horizontal axis
    for (size_t i = 0; i < children_count; ++i) {
        dui_node_layout_state* child = children_states[i];
        child->hori_offset = (node_state->given_width  - child->given_width)  * (data->horizontal_align - 0.5f);
    }
}

const dui_type dui_column_type = {
    .array_child        = 1,
    .auxilary_bytes     = 0,
    .auxilary           = NULL,
    .width_measure      = dui_overlay_width_measure_func,
    .width_distribute   = dui_overlay_width_distribute_func,
    .height_measure     = column_height_measure,
    .height_distribute  = column_height_distribute,
    .position           = column_position,
    .transform          = NULL
};

// ===========================
// Clipbox Type
// This type is specially handled in pass implementation
const dui_type dui_clipbox_type = box_behavior_type;

// ===========================
// Depth Type
// This type is specially handled in pass implementation
const dui_type dui_depth_type = box_behavior_type;

// ===========================
// Box Type
// This type is specially handled in pass implementation
const dui_type dui_box_type = box_behavior_type;

// ===========================
// Text type
// This type is specially handled in pass implementation

typedef struct text_type_auxilary_state {
    dpr_partitioner*    partitioner;
    dpr_partition*      owned_glyph_buffer_partition;
    float               text_width;
    float               text_height;
} text_type_auxilary_state;

void text_auxilary_destructor(void* auxilary) {
    text_type_auxilary_state* aux = auxilary;
    if (aux->owned_glyph_buffer_partition) dpr_partitioner_free_partition(aux->partitioner, aux->owned_glyph_buffer_partition);
    aux->owned_glyph_buffer_partition = NULL;
}

void text_width_measure(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    dui_overlay_width_measure_func(node_data, node_state, children_count, children_states, auxilary);
    text_type_auxilary_state* aux = auxilary;
    node_state->measured_width.min = max_int(node_state->measured_width.min, (int)aux->text_width);
    node_state->measured_width.max = max_int(node_state->measured_width.max, (int)aux->text_width);
    if (node_state->measured_width.min != node_state->measured_width.max) node_state->measured_width.flex = 1.0f;
}

void text_height_measure(
    const void*             node_data,
    dui_node_layout_state*  node_state,
    size_t                  children_count,
    dui_node_layout_state** children_states,
    void*                   auxilary
) {
    dui_overlay_height_measure_func(node_data, node_state, children_count, children_states, auxilary);
    text_type_auxilary_state* aux = auxilary;
    node_state->measured_height.min = max_int(node_state->measured_height.min, (int)aux->text_height);
    node_state->measured_height.max = max_int(node_state->measured_height.max, (int)aux->text_height);
    if (node_state->measured_height.min != node_state->measured_height.max) node_state->measured_height.flex = 1.0f;
}

const dui_type dui_text_type = {
    .array_child            = 0,
    .auxilary_bytes         = sizeof(text_type_auxilary_state),
    .auxilary               = NULL,
    .auxilary_destructor    = text_auxilary_destructor,
    .width_measure          = text_width_measure,
    .width_distribute       = dui_overlay_width_distribute_func,
    .height_measure         = text_height_measure,
    .height_distribute      = dui_overlay_height_distribute_func,
    .position               = dui_overlay_position_func,
    .transform              = NULL
};

// ===========================
// Cursor Input Handle Type
// This type is specially handled in pass implementation
const dui_type dui_cursor_input_handle_type = box_behavior_type;

// ===========================
// Cursor Input Handle Type
// This type is specially handled in pass implementation
const dui_type dui_cursor_input_box_type = box_behavior_type;

// ===========================
// Node fields reads

static inline const void* get_node_data(const dui_node* node, const char* instance) {
    if (node->flags & dui_flag_instanced_data) return (void*)(instance + node->data_offset);
    return node->data;
}

static inline const dui_node* get_node_child(const dui_node* node, const char* instance) {
    if (node->flags & dui_flag_instanced_child) return (const dui_node*)(instance + node->child_offset);
    return node->child;
}

// ===========================
// Stable sort helper

// currently implemeted as mergesort
void stable_sort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
    if (nmemb < 2) return;

    char *arr = (char*)base;
    char *tmp = malloc(nmemb * size);
    if (!tmp) return;

    for (size_t width = 1; width < nmemb; width *= 2) {
        for (size_t i = 0; i < nmemb; i += 2 * width) {
            size_t l = i;
            size_t m = i + width < nmemb ? i + width : nmemb;
            size_t r = i + 2 * width < nmemb ? i + 2 * width : nmemb;
            size_t p = l, q = m, k = i;

            while (p < m && q < r) {
                if (compar(arr + p * size, arr + q * size) <= 0) memcpy(tmp + k++ * size, arr + p++ * size, size);
                else memcpy(tmp + k++ * size, arr + q++ * size, size);
            }

            while (p < m) memcpy(tmp + k++ * size, arr + p++ * size, size);
            while (q < r) memcpy(tmp + k++ * size, arr + q++ * size, size);
        }

        memcpy(arr, tmp, nmemb * size);
    }

    free(tmp);
    return;
}

// ===========================
// Cache Object

typedef struct cache_slot cache_slot;
typedef struct auxilary_slot auxilary_slot;
typedef struct draw_request draw_request;
typedef struct text_request text_request;
typedef struct clipbox_request clipbox_request;
typedef struct cursor_input_box cursor_input_box;

struct dui_cache {
    // Passes constants
    int                 resolution_x;
    int                 resolution_y;
    unsigned char       frame_index;

    // Nodes cache hashmap
    size_t              cache_capacity;
    size_t              cache_fill;
    cache_slot*         cache_slots;

    // Nodes auxilary state hashmap
    size_t              auxilary_capacity;
    size_t              auxilary_fill;
    auxilary_slot*      auxilary_slots;
    
    // Draw requests dynamic array
    size_t              draw_requests_capacity;
    size_t              draw_requests_count;
    draw_request*       draw_requests;

    // Text requests dynamic array
    size_t              text_requests_capacity;
    size_t              text_requests_count;
    text_request*       text_requests;

    // Clipbox requests dynamic array
    size_t              clipbox_requests_capacity;
    size_t              clipbox_requests_count;
    clipbox_request*    clipbox_requests;

    // Cursor input boxes dynamic array
    size_t              cursor_input_boxes_capacity;
    size_t              cursor_input_boxes_count;
    cursor_input_box*   cursor_input_boxes;
};

dui_cache* dui_create_cache() {
    dui_cache* cache = calloc(1, sizeof(dui_cache));
    return cache;
}

static void auxilary_hashmap_garbage_collect(dui_cache* cache);
static void free_cached_text_requests(dui_cache* cache);
void dui_free_cache(dui_cache* cache) {
    if (!cache) return;

    // Free all auxilary slots by using impossible value
    cache->frame_index = LAST_FRAME_USED_IN_RENDER_IMPOSIBLE;
    auxilary_hashmap_garbage_collect(cache);

    // Free all cached textes
    free_cached_text_requests(cache);

    free(cache->cache_slots);
    free(cache->auxilary_slots);
    free(cache->draw_requests);
    free(cache->text_requests);
    free(cache->clipbox_requests);
    free(cache);
}

// ===========================
// Cache Hashmaps

typedef struct node_stable_index {
    const dui_node* node;
    const void*     instance;
} node_stable_index;

typedef struct cache_slot {
    node_stable_index       key;
    size_t                  value_child_count;
    dui_node_layout_state   value_state;
    unsigned char           last_frame_used_in_render;  
} cache_slot;

typedef struct auxilary_slot {
    node_stable_index       key;
    const dui_type*         state_type;
    void*                   state_ptr;
    unsigned char           last_frame_used_in_render;  
} auxilary_slot;

static uint64_t hash_ptr(const void* p) {
    uint64_t x = (uint64_t)(uintptr_t)p;
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    return (x ^ (x >> 31));
}

static size_t hash_key(node_stable_index key) {
    uint64_t h1 = hash_ptr(key.node);
    uint64_t h2 = hash_ptr(key.instance);
    return (size_t)(h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2)));
}

// Definies three functions:
// void       PREFIX##_hashmap_grow             (dui_cache* cache);
// SLOT_TYPE* PREFIX##_hashmap_get              (dui_cache* cache, node_stable_index key, int insert_if_none)
// void       PREFIX##_hashmap_garbage_collect  (dui_cache* cache) 
// Define HASHMAP_SLOT_INITIALIZER to define default slot value
// Define HASHMAP_SLOT_DESTRUCTOR(slot ptr) to set garbage collector slot free method
#define DEFINE_HASHMAP_FUNCS(PREFIX, SLOT_TYPE, SLOTS_FIELD, CAP_FIELD, FILL_FIELD) \
\
static SLOT_TYPE* PREFIX##_hashmap_get                                          \
(dui_cache* cache, node_stable_index key, int insert_if_none);                  \
\
static void PREFIX##_hashmap_grow(dui_cache* cache) {                           \
    size_t old_cap = cache->CAP_FIELD;                                          \
    SLOT_TYPE* old_slots = cache->SLOTS_FIELD;                                  \
\
    size_t new_cap = old_cap ? old_cap * 2 : 64;                                \
\
    cache->SLOTS_FIELD = calloc(new_cap, sizeof(*cache->SLOTS_FIELD));          \
    cache->CAP_FIELD   = new_cap;                                               \
    cache->FILL_FIELD  = 0;                                                     \
\
    for (size_t i = 0; i < old_cap; ++i) {                                      \
        unsigned char time = old_slots[i].last_frame_used_in_render;            \
        if (time == LAST_FRAME_USED_IN_RENDER_EMPTY ||                          \
            time == LAST_FRAME_USED_IN_RENDER_TOMBSTONE) continue;              \
        SLOT_TYPE* dst = PREFIX##_hashmap_get(cache, old_slots[i].key, 1);      \
        *dst = old_slots[i];                                                    \
    }                                                                           \
\
    free(old_slots);                                                            \
}                                                                               \
\
static SLOT_TYPE* PREFIX##_hashmap_get(                                         \
    dui_cache* cache, node_stable_index key, int insert_if_none) {              \
    if ((cache->FILL_FIELD + 1) * 10 >= cache->CAP_FIELD * 7) {                 \
        PREFIX##_hashmap_grow(cache);                                           \
    }                                                                           \
\
    size_t mask = cache->CAP_FIELD - 1;                                         \
    size_t idx  = hash_key(key) & mask;                                         \
\
    for (SLOT_TYPE* tombstone = NULL;;) {                                       \
        SLOT_TYPE* slot = &cache->SLOTS_FIELD[idx];                             \
        unsigned char time = slot->last_frame_used_in_render;                   \
\
        if (time == LAST_FRAME_USED_IN_RENDER_EMPTY) {                          \
            if (!insert_if_none) return NULL;                                   \
            if (tombstone) slot = tombstone;                                    \
            else cache->FILL_FIELD++;                                           \
            *slot = (SLOT_TYPE)HASHMAP_SLOT_INITIALIZER;                        \
            return slot;                                                        \
        }                                                                       \
\
        if (time == LAST_FRAME_USED_IN_RENDER_TOMBSTONE) {                      \
            if (!tombstone) tombstone = slot;                                   \
         }                                                                      \
        else if (                                                               \
            slot->key.node == key.node &&                                       \
            slot->key.instance == key.instance                                  \
        ) return slot;                                                          \
\
        idx = (idx + 1) & mask;                                                 \
    }                                                                           \
}                                                                               \
\
static void PREFIX##_hashmap_garbage_collect(dui_cache* cache) {                \
    for (size_t i = 0; i < cache->CAP_FIELD; i++) {                             \
        SLOT_TYPE*     slot = &cache->SLOTS_FIELD[i];                           \
        unsigned char* time = &slot->last_frame_used_in_render;                 \
        if (*time                                           &&                  \
            *time != LAST_FRAME_USED_IN_RENDER_TOMBSTONE    &&                  \
            *time != cache->frame_index                                         \
        ) {                                                                     \
            HASHMAP_SLOT_DESTRUCTOR(slot);                                      \
            *time = LAST_FRAME_USED_IN_RENDER_TOMBSTONE;                        \
        }                                                                       \
    }                                                                           \
}

#define HASHMAP_SLOT_INITIALIZER {.key = key}
#define HASHMAP_SLOT_DESTRUCTOR(slot_ptr)
DEFINE_HASHMAP_FUNCS(
    cache, cache_slot, cache_slots, cache_capacity, cache_fill
);

#undef HASHMAP_SLOT_INITIALIZER
#undef HASHMAP_SLOT_DESTRUCTOR

static inline void auxilary_hashmap_slot_destructor(auxilary_slot* slot) {
    if (slot->key.node->type->auxilary_destructor) {
        slot->key.node->type->auxilary_destructor(slot->state_ptr); 
    } free(slot->state_ptr); slot->state_ptr = NULL;
}

#define HASHMAP_SLOT_INITIALIZER {.key = key, .state_type = NULL, .state_ptr = NULL}
#define HASHMAP_SLOT_DESTRUCTOR(slot_ptr) auxilary_hashmap_slot_destructor(slot_ptr)
DEFINE_HASHMAP_FUNCS(
    auxilary, auxilary_slot, auxilary_slots, auxilary_capacity, auxilary_fill
);

// Gets slot, always inserts, as cache must always exist for node
static inline cache_slot* cache_get_utill(dui_cache* cache, node_stable_index index) {
    return cache_hashmap_get(cache, index, 1);
}

// Gets slot, inserts if type size != 0, only then slot must exist, allocs memory if needed
static inline auxilary_slot* auxilary_get_utill(dui_cache* cache, node_stable_index index) {
    if (!index.node->type->auxilary_bytes) return NULL; // none desired
    auxilary_slot* slot = auxilary_hashmap_get(cache, index, 1);

    // handle case where node type has changed
    if (slot->state_type && slot->state_type != index.node->type) {
        if (index.node->type->auxilary_destructor) index.node->type->auxilary_destructor(slot->state_ptr);
        free(slot->state_ptr); slot->state_ptr = NULL;
    }
    
    // allocate state
    if (!slot->state_ptr) {
        slot->state_ptr  = calloc(1, index.node->type->auxilary_bytes);
        slot->state_type = index.node->type;
    }

    return slot;
}

// ===========================
// Cache dynamic arrays

struct draw_request {
    dla_mat2x3              transform;
    int                     clip_index;
    short                   depth_index;
    char                    is_box_not_text;
    union {
        dui_box_data        box_data;
        node_stable_index   text_node;
    };
};

struct text_request {
    node_stable_index       owning_node;
    size_t                  glyphs_count;
    struct gpu_glyph*       glyphs;
};

struct clipbox_request {
    dla_mat2x3              transform;
};

struct cursor_input_box {
    node_stable_index       owner;
    dui_cursor_handle_func  handle;
    int                     clip_index;
    short                   depth_index;
    dla_mat2x3              box_transform;
};

// Definies one function:
// void PREFIX##_cache_push (dui_cache* cache, ELEMENT_TYPE element);
#define DEFINE_DYNAMIC_ARRAY_FUNCS(PREFIX, ELEMENT_TYPE, ARRAY_FIELD, CAP_FIELD, CNT_FIELD)     \
static int PREFIX##_cache_push(dui_cache* cache, ELEMENT_TYPE element) {                        \
    if (cache->CNT_FIELD + 1 > cache->CAP_FIELD) {                                              \
        size_t          new_cap = cache->CAP_FIELD ? cache->CAP_FIELD * 2 : 64;                 \
        ELEMENT_TYPE*   new_arr = realloc(cache->ARRAY_FIELD, new_cap * sizeof(ELEMENT_TYPE));  \
        if (!new_arr)   return - 1;                                                             \
\
        cache->ARRAY_FIELD  = new_arr;                                                          \
        cache->CAP_FIELD    = new_cap;                                                          \
    }                                                                                           \
\
    cache->ARRAY_FIELD[cache->CNT_FIELD] = element;                                             \
    return (int)cache->CNT_FIELD++;                                                             \
}

DEFINE_DYNAMIC_ARRAY_FUNCS(
    draw_request, draw_request, draw_requests, draw_requests_capacity, draw_requests_count
);

DEFINE_DYNAMIC_ARRAY_FUNCS(
    text_request, text_request, text_requests, text_requests_capacity, text_requests_count
);

DEFINE_DYNAMIC_ARRAY_FUNCS(
    clipbox_request, clipbox_request, clipbox_requests, clipbox_requests_capacity, clipbox_requests_count
);

DEFINE_DYNAMIC_ARRAY_FUNCS(
    cursor_input_box, cursor_input_box, cursor_input_boxes, cursor_input_boxes_capacity, cursor_input_boxes_count
);

static inline void free_cached_text_requests(dui_cache* cache) {
    for (size_t i = 0; i < cache->text_requests_count; i++) {
        free(cache->text_requests[i].glyphs);
    }
    cache->text_requests_count = 0;
}

// ===========================
// Cache Update

// Invalidation Node Gate

typedef enum invalidation_flag_only {
    invalidation_flag_only_auxilary          = 1,
    invalidation_flag_only_width_measure     = 2,
    invalidation_flag_only_width_distribute  = 4,
    invalidation_flag_only_height_measure    = 8,
    invalidation_flag_only_height_distribute = 16,
    invalidation_flag_only_position          = 32,
} invalidation_flag_only;

static inline int find_shall_recurse(cache_slot* node_slot, const void* data, invalidation_flag_only pass) {
    if (node_slot->key.node->type != &dui_invalidation_type) return 1;
    dui_invalidation_data* inv_data = (dui_invalidation_data*)data; // special case where const may be discarded

    // recurse one time in this pass
    if (inv_data->flag_consumable & pass) {
        inv_data->flag_consumable &= ~(pass);   // turn off this pass bit
        return 1;
    }

    // recurse only if marked always to do it
    return inv_data->flag_always & pass;
}

// Cache Walk Pass
// Called on remeasure
// Computes: 
// - walk order (the order layout and auxilary caches are visited, to avoid hashmaping multiple times)
// - nodes children count (simplify implementations)
// - nodes subtree size (including self, to easily skip on invalidation nodes)

// shall initialized with cache and 0 in other fields
typedef struct caches_walk_order {
    dui_cache*              cache;      // cache owning cache slots
    size_t                  capacity;   // in cache_slot pointers
    size_t                  position;   // in cache_slot pointers
    cache_slot**            slots;      // sized capacity, node cache slots in enter order
    dui_node_layout_state** states;     // sized capacity, node layout states in children oreder
    auxilary_slot**         auxilary;   // sized capacity, node auxilary slots in enter order
    size_t*                 subtree;    // sized capacity, node subtree size, including self
} caches_walk_order;

// Guaranteed valid 0-intialized object after free
// except cache field being untouched
void free_caches_walk_order(caches_walk_order* order) {
    free(order->slots);     order->slots    = NULL;
    free(order->states);    order->states   = NULL;
    free(order->auxilary);  order->auxilary = NULL;
    free(order->subtree);   order->subtree  = NULL;
    order->capacity = 0;    order->position = 0;
}

// Returns non-zero at success
static inline int caches_walk_order_push(caches_walk_order* walk_order, cache_slot* slot, void* auxilary) {
    if (walk_order->position + 1 > walk_order->capacity) {
        size_t new_cap = walk_order->capacity ? walk_order->capacity * 2 : 64;
    
        cache_slot**            new_slt = realloc(walk_order->slots,    new_cap * sizeof(cache_slot*));
        dui_node_layout_state** new_sts = realloc(walk_order->states,   new_cap * sizeof(dui_node_layout_state*));
        auxilary_slot**         new_aux = realloc(walk_order->auxilary, new_cap * sizeof(auxilary_slot*));
        size_t*                 new_sub = realloc(walk_order->subtree,  new_cap * sizeof(size_t));

        if (!new_slt || !new_sts || !new_aux || !new_sub) {
            free(new_slt); free(new_sts); free(new_aux); free(new_sub);
            free_caches_walk_order(walk_order);
            return 0; // failed to realloc -> failed to push -> entire layout fails
        }

        walk_order->capacity = new_cap;
        walk_order->slots    = new_slt;
        walk_order->states   = new_sts;
        walk_order->auxilary = new_aux;
        walk_order->subtree  = new_sub;
    }

    walk_order->slots   [walk_order->position]  = slot;
    walk_order->states  [walk_order->position]  = &slot->value_state;
    walk_order->auxilary[walk_order->position]  = auxilary;
    walk_order->subtree [walk_order->position]  = 1; // included node itself
    walk_order->position++;

    return 1; // success
}

// Pushes all child nodes caches of node to caches_walk_order
// Recurse into children left to right
// Returns non-zero at success
int caches_walk_dfs(
    caches_walk_order*  walk_order, 
    cache_slot*         current, 
    size_t*             subtree_size_target, 
    const void*         instance
) {
    const dui_node* node  = current->key.node;
    const dui_node* child = get_node_child(current->key.node, current->key.instance);
    size_t          count = 0;
    int             scc   = 1;

    // change instance for subtree
    if (node->type == &dui_instance_type) {
        instance = get_node_data(current->key.node, instance);
    }

    if (!node->type->array_child && child) {
        cache_slot*     child_slot = cache_get_utill(walk_order->cache, (node_stable_index){child, instance});
        auxilary_slot*  auxlr_slot = auxilary_get_utill(walk_order->cache, (node_stable_index){child, instance});
        scc &= caches_walk_order_push(walk_order, child_slot, auxlr_slot); count++;
    }
    else if (child) for (const dui_node* cc = child; cc->type != NULL; cc++) {
        cache_slot*     child_slot = cache_get_utill(walk_order->cache, (node_stable_index){cc, instance});
        auxilary_slot*  auxlr_slot = auxilary_get_utill(walk_order->cache, (node_stable_index){cc, instance});
        scc &= caches_walk_order_push(walk_order, child_slot, auxlr_slot); count++;
    }

    // recurse
    size_t begin_pos = walk_order->position - count;
    for (size_t i = 0; i < count; i++) {
        scc &= caches_walk_dfs(walk_order, walk_order->slots[begin_pos + i], &walk_order->subtree[begin_pos + i], instance);
        *subtree_size_target += walk_order->subtree[begin_pos + i];
    }

    current->value_child_count = count;
    return scc;
}

// Generic layout dfs generation macros

// Definies function:
// void PREFIX##_dfs(caches_walk_order* walk_order, cache_slot* current, auxilary_slot* auxilary, size_t first_child)
// Exec order: recurse -> own function -> additional code
#define BOTTOM_UP_DFS(PREFIX, TYPE_FUNC_NAME, INV_PASS_ONLY_FLAG, ...)                              \
void PREFIX##_dfs(                                                                                  \
    caches_walk_order*  walk_order,                                                                 \
    cache_slot*         current,                                                                    \
    auxilary_slot*      auxilary,                                                                   \
    size_t              first_child                                                                 \
) {                                                                                                 \
    cache_slot**    children    = &walk_order->slots[first_child];                                  \
    auxilary_slot** auxilaries  = &walk_order->auxilary[first_child];                               \
    size_t*         subtrees    = &walk_order->subtree[first_child];                                \
    const void*     data        = get_node_data(current->key.node, current->key.instance);          \
\
    if (find_shall_recurse(current, data, INV_PASS_ONLY_FLAG)) {                                    \
        size_t child_first_child = first_child + current->value_child_count;                        \
        for (size_t i = 0; i < current->value_child_count; i++) {                                   \
            PREFIX##_dfs(walk_order, children[i], auxilaries[i], child_first_child);                \
            child_first_child += subtrees[i] - 1;                                                   \
        }                                                                                           \
    }                                                                                               \
\
    dui_node_layout_func func = current->key.node->type->TYPE_FUNC_NAME;                            \
    if (func != NULL) func(                                                                         \
        data, &current->value_state, current->value_child_count, &walk_order->states[first_child],  \
        auxilary ? auxilary->state_ptr : NULL                                                       \
    );                                                                                              \
\
    __VA_ARGS__                                                                                     \
}

// Definies function:
// void PREFIX##_dfs(caches_walk_order* walk_order, cache_slot* current, auxilary_slot* auxilary, size_t first_child)
// Exec order: additional code -> own function -> recurse
#define TOP_DOWN_DFS(PREFIX, TYPE_FUNC_NAME, INV_PASS_ONLY_FLAG, ...)                               \
void PREFIX##_dfs(                                                                                  \
    caches_walk_order*  walk_order,                                                                 \
    cache_slot*         current,                                                                    \
    auxilary_slot*      auxilary,                                                                   \
    size_t              first_child                                                                 \
) {                                                                                                 \
    cache_slot**    children    = &walk_order->slots[first_child];                                  \
    auxilary_slot** auxilaries  = &walk_order->auxilary[first_child];                               \
    size_t*         subtrees    = &walk_order->subtree[first_child];                                \
    const void*     data        = get_node_data(current->key.node, current->key.instance);          \
\
    __VA_ARGS__                                                                                     \
\
    dui_node_layout_func func = current->key.node->type->TYPE_FUNC_NAME;                            \
    if (func != NULL) func(                                                                         \
        data, &current->value_state, current->value_child_count, &walk_order->states[first_child],  \
        auxilary ? auxilary->state_ptr : NULL                                                       \
    );                                                                                              \
\
    if (find_shall_recurse(current, data, INV_PASS_ONLY_FLAG)) {                                    \
        size_t child_first_child = first_child + current->value_child_count;                        \
        for (size_t i = 0; i < current->value_child_count; i++) {                                   \
            PREFIX##_dfs(walk_order, children[i], auxilaries[i], child_first_child);                \
            child_first_child += subtrees[i] - 1;                                                   \
        }                                                                                           \
    }                                                                                               \
}

// Layout passes
// Travels tree, call functions as specified in type comments
// to calcualate what specfied in type comments

void create_text_request(dui_cache* cache, cache_slot* slot, text_type_auxilary_state* aux);

// auxilary pass, additionaly update text buffer
TOP_DOWN_DFS(
    auxilary, auxilary, invalidation_flag_only_auxilary,
    if (current->key.node->type == &dui_text_type) {
        create_text_request(walk_order->cache, current, (text_type_auxilary_state*)auxilary->state_ptr);
    }
);

// width measure pass, additionaly handle ignore flags
BOTTOM_UP_DFS(
    width_measure, width_measure, invalidation_flag_only_width_measure,
    if (current->key.node->flags & dui_flag_ignore_min_width) {
        current->value_state.measured_width.min = 0;
        current->value_state.measured_width.flex = 1.0f;
    }
    if (current->key.node->flags & dui_flag_ignore_max_width) {
        current->value_state.measured_width.max  = dui_inf_length;
        current->value_state.measured_width.flex = 1.0f;
    }
);

// width distribute pass, additionaly ensure received width
// is within node measured limits
TOP_DOWN_DFS(
    width_distribute, width_distribute, invalidation_flag_only_width_distribute,
    current->value_state.given_width = limit_length(
        current->value_state.given_width,
        current->value_state.measured_width
    );
);

// height measure pass, additionaly handle ignore flags
BOTTOM_UP_DFS(
    height_measure, height_measure, invalidation_flag_only_height_measure,
    if (current->key.node->flags & dui_flag_ignore_min_height) {
        current->value_state.measured_height.min = 0;
        current->value_state.measured_height.flex = 1.0f;
    }
    if (current->key.node->flags & dui_flag_ignore_max_height) {
        current->value_state.measured_height.max  = dui_inf_length;
        current->value_state.measured_height.flex = 1.0f;
    }
);

// height distribute pass, additionaly ensure received height 
// is within node measured limits
TOP_DOWN_DFS(
    height_distribute, height_distribute, invalidation_flag_only_height_distribute,
    current->value_state.given_height = limit_length(
        current->value_state.given_height,
        current->value_state.measured_height
    );
);

// position pass, no additional code
TOP_DOWN_DFS(
    position, position, invalidation_flag_only_position
);

// Renders widget
// Issues rendering of ui primitives
// Also renders input boxes into input dynamic array
// Render pass is safe in terms of hashmap pointers invalidation
// Since it refers on the pointer only on enter - after visiting any child it is not used

typedef struct render_dfs_subtree_state {
    const void*             instance;
    short                   depth_index;
    int                     clipbox_index;
    dui_cursor_handle_func  cursor_handle;
} render_dfs_subtree_state;

static void render_dfs(
    dui_cache*                      cache, 
    int                             previous_width,
    int                             previous_height, 
    const dui_node*                 node,
    dla_mat2x3                      transform, 
    const render_dfs_subtree_state* state
);

static inline void render_dfs_recurse(
    dui_cache*                      cache, 
    cache_slot*                     own,
    dla_mat2x3                      transform, 
    const render_dfs_subtree_state* state
) {
    const dui_node* child = get_node_child(own->key.node, own->key.instance);

    // back node dimensions to avoid reading own slot after visiting child
    int own_width  = own->value_state.given_width;
    int own_height = own->value_state.given_height;

    // single child
    if (!own->key.node->type->array_child && child) {
        render_dfs(cache, own_width, own_height, child, transform, state);
    }
    // multiple children
    else if (child) for (const dui_node* current_child = child; current_child->type != NULL; current_child++) {
        render_dfs(cache, own_width, own_height, current_child, transform, state);
    }
}

static void render_dfs(
    dui_cache*                      cache, 
    int                             previous_width,
    int                             previous_height, 
    const dui_node*                 node,
    dla_mat2x3                      transform, 
    const render_dfs_subtree_state* state
) {
    node_stable_index index = {node, state->instance};

    // get node data
    const void*     data  = get_node_data (node, state->instance);
    cache_slot*     own   = cache_get_utill(cache, index);
    auxilary_slot*  aux   = auxilary_get_utill(cache, index);

    // mark used, to avoid garbage collect
    own->last_frame_used_in_render = cache->frame_index;
    if (aux) aux->last_frame_used_in_render = cache->frame_index;

    // change transform based on node's position and scale
    float off_x   = ((float)own->value_state.hori_offset * 2)   / cache->resolution_x;
    float off_y   = ((float)own->value_state.vert_offset * 2)   / cache->resolution_y;
    float scale_x = ((float)own->value_state.given_width)       / previous_width;
    float scale_y = ((float)own->value_state.given_height)      / previous_height;
    transform = mat2x3_offset(transform, off_x, off_y);
    transform = mat2x3_scale (transform, scale_x, scale_y);

    // do transform if method provided
    if (node->type->transform) node->type->transform(
        data, &transform, 
        cache->resolution_x, 
        cache->resolution_y,
        aux
    );
    
    // special nodes
     // update instance for subtree
    if (node->type == &dui_instance_type) {
        render_dfs_subtree_state new_state = *state;
        new_state.instance = data;
        render_dfs_recurse(cache, own, transform, &new_state); 
        return; // recursed
    }
    // request box draw
    else if (node->type == &dui_box_type){
        const dui_box_data* bdata = data;
        draw_request_cache_push(cache, (draw_request){
            .transform          = transform,
            .clip_index         = state->clipbox_index,
            .depth_index        = state->depth_index,
            .is_box_not_text    = 1,
            .box_data           = *bdata
        });
    }
    // request text draw
    else if (node->type == &dui_text_type) {
        const dui_text_data*            tdata = data;
        const text_type_auxilary_state* taux  = aux->state_ptr;

        draw_request_cache_push(cache, (draw_request){
            .transform          = transform,
            .clip_index         = state->clipbox_index,
            .depth_index        = state->depth_index,
            .is_box_not_text    = 0,
            .text_node          = index
        });
    }
    // update depth for subtree
    else if (node->type == &dui_depth_type) {
        const dui_depth_data* ddata = data;
        render_dfs_subtree_state new_state = *state;
        new_state.depth_index += ddata->depth_change;
        render_dfs_recurse(cache, own, transform, &new_state); 
        return; // recursed
    }
    // request clipbox, update clipbox for subtree
    else if (node->type == &dui_clipbox_type) {
        render_dfs_subtree_state new_state = *state;
        new_state.clipbox_index = clipbox_request_cache_push(cache, (clipbox_request){.transform = transform});
        render_dfs_recurse(cache, own, transform, &new_state); 
        return; // recursed
    }
    // request cursor input box
    else if (node->type == &dui_cursor_input_box_type) {
        cursor_input_box_cache_push(cache, (cursor_input_box){
            .owner          = index,
            .handle         = state->cursor_handle,
            .depth_index    = state->depth_index,
            .clip_index     = state->clipbox_index,
            .box_transform  = transform
        });
    }
    // update cursor input handle for subtree
    else if (node->type == &dui_cursor_input_handle_type) { 
        dui_cursor_handle_func cursor_handle = (dui_cursor_handle_func)data;
        render_dfs_subtree_state new_state = *state;
        new_state.cursor_handle = cursor_handle;   
        render_dfs_recurse(cache, own, transform, &new_state); 
        return; // recursed
    }

    // default recursion without state changes
    render_dfs_recurse(cache, own, transform, state);
}

// Helper for draw requests depth sorting : deepest first
static inline int helper_draw_requests_greater_depth(const void* av, const void* bv) {
    const draw_request* a = (const draw_request*)av; 
    const draw_request* b = (const draw_request*)bv;
    if (a->depth_index > b->depth_index) return 1;
    return 0;
}

// Helper for cursor input boxes depth sorting : deepest first
static inline int helper_cursor_input_boxes_greater_depth(const void* av, const void* bv) {
    const cursor_input_box* a = (const cursor_input_box*)av; 
    const cursor_input_box* b = (const cursor_input_box*)bv;
    if (a->depth_index > b->depth_index) return 1;
    return 0;
}

// Main update function
// Calls passes
void dui_update_cache(
    dui_cache*              cache,
    const dui_node*         root,
    int                     resolution_x,
    int                     resolution_y,
    dui_cursor_state        cursor_state
) {
    // Init state
    cache->resolution_x             = resolution_x;
    cache->resolution_y             = resolution_y;
    cache->draw_requests_count      = 0;
    cache->text_requests_count      = 0;
    cache->clipbox_requests_count   = 0;
    cache->cursor_input_boxes_count = 0;

    // Pick next frame index
    cache->frame_index++; if (cache->frame_index < LAST_FRAME_USED_IN_RENDER_FIRST) cache->frame_index = LAST_FRAME_USED_IN_RENDER_FIRST;

    // Render pass
    render_dfs_subtree_state default_subtree_state = {
        .instance       = NULL,
        .depth_index    = 0,
        .clipbox_index  = -1,
        .cursor_handle  = NULL
    };
    render_dfs(cache, cache->resolution_x, cache->resolution_y, root, dla_mat2x3_identity(), &default_subtree_state);

    // Sort render requests and input boxes by depth
    stable_sort(cache->draw_requests,       cache->draw_requests_count,      sizeof(draw_request),      helper_draw_requests_greater_depth);
    stable_sort(cache->cursor_input_boxes,  cache->cursor_input_boxes_count, sizeof(cursor_input_box),  helper_cursor_input_boxes_greater_depth);

    // Find out normalized cursor position
    float norm_cursor_x = -1.0f + 2.0f * ((float)cursor_state.position_x / resolution_x);
    float norm_cursor_y =  1.0f - 2.0f * ((float)cursor_state.position_y / resolution_y);

    // Do cursor input callbacks, walking from topmost to deepest
    dui_cursor_state changable_state = cursor_state;
    int              ever_was_inside = 0;
    if (cache->cursor_input_boxes_count) for (size_t i = cache->cursor_input_boxes_count - 1; ; i--) {
        cursor_input_box* ibox = &cache->cursor_input_boxes[i];
        if (!ibox->handle) continue; // nothing to call

        int cursor_inside  = is_point_in_transformed_box(ibox->box_transform, norm_cursor_x, norm_cursor_y);
        if (ibox->clip_index != -1) {
            cursor_inside &= is_point_in_transformed_box(cache->clipbox_requests[ibox->clip_index].transform, norm_cursor_x, norm_cursor_y);
        }

        ever_was_inside |= cursor_inside;

        ibox->handle(
            get_node_data(ibox->owner.node, ibox->owner.instance),
            auxilary_get_utill(cache, ibox->owner),
            &changable_state, &cursor_state,
            cursor_inside && ever_was_inside, cursor_inside
        );

        if (i == 0) break; // break loop at last element
    }

    // Always relayout
    // Do it after render - then we can trust all nodes have their inserted cache and auxilary slots
    // This is important so hashmap pointers does not get invalidated during passes
    // This means we are one frame behind with layout, but it is not a big deal actually.
    if (1) {
        cache_slot*    root_cache = cache_get_utill(cache, (node_stable_index){root, NULL});
        auxilary_slot* root_auxlr = auxilary_get_utill(cache, (node_stable_index){root, NULL});

        // Give root entire screen
        // Will auto bound to desired at distribute
        root_cache->value_state.given_width  = resolution_x;
        root_cache->value_state.given_height = resolution_y;

        // Find walk order
        caches_walk_order walk_order    = {.cache = cache};
        size_t            root_subtree  = 1; // root itself included
        if (!caches_walk_dfs(&walk_order, root_cache, &root_subtree, NULL)) {
            free_caches_walk_order(&walk_order);
            return;
        }
        
        // Perform all passes
        auxilary_dfs(&walk_order, root_cache, root_auxlr, 0);
        width_measure_dfs(&walk_order, root_cache, root_auxlr, 0);
        width_distribute_dfs(&walk_order, root_cache, root_auxlr, 0);
        height_measure_dfs(&walk_order, root_cache, root_auxlr, 0);
        height_distribute_dfs(&walk_order, root_cache, root_auxlr, 0);
        position_dfs(&walk_order, root_cache, root_auxlr, 0);

        free_caches_walk_order(&walk_order);
    }

    // Garbage collect dead cache entries
    // If entry was not used in render, mark it free
    // Do every 16 frames not to spend to much time on it
    if (cache->frame_index % 16 == 0) {
        cache_hashmap_garbage_collect(cache);
        auxilary_hashmap_garbage_collect(cache);
    }
}

// ===========================
// Rendering Common

#define INITIAL_INSTANCES_BUFFER_SIZE   (1024 * sizeof(gpu_instance))
#define INITIAL_DRAW_ITEM_BUFFER_SIZE   (1024 * sizeof(gpu_draw_item))
#define INITIAL_CLIPBOXES_BUFFER_SIZE   (16 * sizeof(gpu_clipbox))
#define INITIAL_GLYPH_BUFFER_SIZE       (2024 * sizeof(gpu_glyph))
#define GLYPH_STRUCTURE_ALIGN           4

typedef struct gpu_instance {
    uint32_t    item;
    uint32_t    glyph;
} gpu_instance;

typedef struct gpu_draw_item {
    dla_mat2x3  transform;
    dgx_uv_2d   atlas_position;
    int         texture_index;
    uint32_t    clipbox_index;
    uint32_t    shader_index;
    float       r, g, b, a;
} gpu_draw_item;

typedef struct gpu_clipbox {
    dla_mat2x3  transform;
} gpu_clipbox;

typedef struct gpu_glyph {
    dgx_uv_2d   atlas_position;
    float       off_x,  off_y;
    float       size_x, size_y;
} gpu_glyph;

typedef struct gpu_vertex_constants {
    uint32_t    resolution_width;
    uint32_t    resolution_height;
    uint32_t    instances_buffer_index;
    uint32_t    draw_items_buffer_index;
    uint32_t    glyphs_buffer_index;
} gpu_vertex_constants;

typedef struct gpu_pixel_constants {
    uint32_t    resolution_width;
    uint32_t    resolution_height;
    uint32_t    clips_buffer_index;
    uint32_t    sampler_index;
} gpu_pixel_constants;

// ===========================
// Helper Methods

static inline dgx_buffer* create_ssbo(dgx_hardware* hardware, uint64_t bytes) {
    return dgx_create_buffer(hardware, &(dgx_buffer_create_info){
        .bytes  = bytes,
        .access = dgx_memory_access_staging_write,
        .usage  = dgx_buffer_usage_storage
    });
}

// ===========================
// Shared Object

struct dui_shared {
    dgx_hardware*       owning_hardware;
    dgx_sampler*        sampler;
    dgx_pipeline*       pipeline;
    dpr_partitioner*    glyph_buffer_partitioner;
    dgx_buffer*         glyph_buffer;
};

dui_shared* dui_create_shared(dgx_hardware* hardware, const dui_shared_create_info* info) {
    dui_shared* shared = calloc(1, sizeof(dui_shared)); if (!shared) return NULL;
    shared->owning_hardware = hardware;

    // Sampler
    shared->sampler = dgx_create_sampler(hardware, &(dgx_sampler_create_info){
        .mag_filter                 = dgx_sampler_filter_linear,
        .min_filter                 = dgx_sampler_filter_linear,
        .mipmap_filter              = dgx_sampler_filter_linear,
        .x_coord_wrapping           = dgx_sampler_wrapping_repeat,
        .y_coord_wrapping           = dgx_sampler_wrapping_repeat,
        .z_coord_wrapping           = dgx_sampler_wrapping_repeat,
        .unnormalized_coordinates   = 0,
        .min_lod                    = 0,
        .max_lod                    = 1,
        .mip_lod_bias               = 0,
    }); if (!shared->sampler) goto _fail;

    // Glyphs buffer
    shared->glyph_buffer = create_ssbo(hardware, INITIAL_GLYPH_BUFFER_SIZE);
    if (!shared->glyph_buffer) goto _fail;

    // Glyph buffer partitioner
    shared->glyph_buffer_partitioner = dpr_create_partitioner(&(dpr_partitioner_create_info){
        .memory_bytes = INITIAL_GLYPH_BUFFER_SIZE,
        .align_bytes  = GLYPH_STRUCTURE_ALIGN
    }); if (!shared->glyph_buffer_partitioner) goto _fail;

    // Pipeline Shaders
    dgx_shader* vertex_shader = dgx_create_shader(shared->owning_hardware, &info->vertex_shader_info);
    dgx_shader* pixel_shader  = dgx_create_shader(shared->owning_hardware, &info->pixel_shader_info);

    if (!vertex_shader || !pixel_shader) {
        dgx_free_shader(vertex_shader);
        dgx_free_shader(pixel_shader);
        goto _fail;
    }

    // Pipeline
    shared->pipeline = dgx_create_pipeline(shared->owning_hardware, &(dgx_pipeline_create_info){
        .attachment_state = info->attachment_state,
        .shader_stages = {
            .shaders[dgx_shader_stage_vertex]   = vertex_shader,
            .constants[dgx_shader_stage_vertex] = sizeof(gpu_vertex_constants),
            .shaders[dgx_shader_stage_pixel]    = pixel_shader,
            .constants[dgx_shader_stage_pixel]  = sizeof(gpu_pixel_constants)
        },
        .input_assembler_state = {
            .topology = dgx_primitive_topology_triangle_strip
        },
        .rasterizer_state = {
            .scissor_enable     = 0,
            .depth_clamp_enable = 0,
            .fill_mode          = dgx_fill_mode_solid,
            .cull_mode          = dgx_cull_mode_none
        },
        .blend_state = {
            .blend_enable   = 1,
            .blend_op       = dgx_blend_op_add,
            .src_factor     = dgx_blend_factor_src_alpha,
            .dst_factor     = dgx_blend_factor_one_minus_src_alpha,
        },
        .depth_stencil_state = {
            .depth_test_enable      = 0,
            .depth_write_enable     = 0,
            .stencil_test_enable    = 0
        }
    });  dgx_free_shader(vertex_shader); dgx_free_shader(pixel_shader);
    if (!shared->pipeline) goto _fail;

    return shared;

_fail:
    dui_free_shared(shared);
    return NULL;
}

void dui_free_shared(dui_shared* shared) {
    if (!shared) return;
    dgx_free_sampler(shared->sampler);
    dgx_free_pipeline(shared->pipeline);
    dgx_free_buffer(shared->glyph_buffer);
    dpr_free_partitioner(shared->glyph_buffer_partitioner);
    free(shared);
}

// ===========================
// Frames

typedef struct single_frame {
    uint32_t                instances_to_render;
    dgx_buffer*             instances_buffer;
    dgx_buffer*             draw_items_buffer;
    dgx_buffer*             clipboxes_buffer;
    gpu_vertex_constants    vertex_constants;
    gpu_pixel_constants     pixel_constants;
} single_frame;

struct dui_frames {
    dui_shared*     owning_shared;
    uint32_t        count;
    single_frame*   frames;
};

dui_frames* dui_create_frames(dgx_hardware* hardware, const dui_frames_create_info* info) {
    dui_shared* shared = info->shared;

    dui_frames* frames = calloc(1, sizeof(dui_frames));  if (!frames) return NULL;
    frames->owning_shared = shared;
    
    // create frames
    frames->count  = info->count;
    frames->frames = calloc(info->count, sizeof(single_frame));
    if (!frames->frames) goto _fail;

    // populate frames
    for (uint32_t i = 0; i < info->count; i++) {
        single_frame* frame = &frames->frames[i];
        *frame = (single_frame){
            .instances_buffer   = create_ssbo(hardware, INITIAL_INSTANCES_BUFFER_SIZE),
            .draw_items_buffer  = create_ssbo(hardware, INITIAL_DRAW_ITEM_BUFFER_SIZE),
            .clipboxes_buffer   = create_ssbo(hardware, INITIAL_CLIPBOXES_BUFFER_SIZE)
        };

        if (!frame->instances_buffer || !frame->draw_items_buffer || !frame->clipboxes_buffer) goto _fail;
    }

    return frames;

_fail:
    dui_free_frames(frames);
    return NULL;
}

void dui_free_frames(dui_frames* frames) {
    if (!frames) return;
    for (uint32_t i = 0; i < frames->count; i++) {
        single_frame* frame = &frames->frames[i];
        dgx_free_buffer(frame->instances_buffer);
        dgx_free_buffer(frame->draw_items_buffer);
        dgx_free_buffer(frame->clipboxes_buffer);
    }
    free(frames->frames);
    free(frames);
}

// ===========================
// Rendering Functions

typedef struct ui_upload_params {
    uint64_t            count;
    dgs_upload_request* requests;
    dgx_staging_memory* staging;
} ui_upload_params;

static void ui_upload_record(void* raw_params) {
    ui_upload_params* params = raw_params;
    uint64_t offset = 0;
    for (uint64_t i = 0; i < params->count; i++) {
        dgs_upload_request req = params->requests[i];
        dgx_tcmd_copy_staging_memory_to_buffer(
            params->staging, (dgx_buffer*)req.target,
            offset, req.offset, req.bytes
        );
    }
}

void dui_upload_cache(
    dui_cache*          cache,
    dui_shared*         shared,
    dui_frames*         frames,
    uint32_t            frame_idx,
    uint8_t             transfer_work_group_index,
    uint8_t             command_list_allocator_index,
    dgx_staging_memory* staging_memory,
    uint64_t            staging_memory_region_offset,
    uint64_t            staging_memory_region_size,
    dgx_timeline*       signal_timeline,
    uint64_t            signal_value
) {
    dgx_hardware* hardware = shared->owning_hardware;
    single_frame* frame    = &frames->frames[frame_idx];

    // Create segmenter
    dgs_segmenter* segmenter = dgs_create_segmenter(&(dgs_segmenter_create_info){
        .bandwidth = staging_memory_region_size
    }); if (!segmenter) goto _cleanup;

    // Prepare partitions for text draws
    for (size_t i = 0; i < cache->text_requests_count; i++) {
        text_request              req  = cache->text_requests[i];
        text_type_auxilary_state* aux = auxilary_get_utill(cache, req.owning_node)->state_ptr;

        aux->partitioner = shared->glyph_buffer_partitioner;

        // always free owned partition to reduce fragmentation
        if (aux->owned_glyph_buffer_partition) {
            dpr_partitioner_free_partition(shared->glyph_buffer_partitioner, aux->owned_glyph_buffer_partition);
            aux->owned_glyph_buffer_partition = NULL;
        }

        // new text is empty - creation of 0 bytes partition is forbidden
        if (!req.glyphs_count) continue;

        // request new partition
        aux->owned_glyph_buffer_partition = dpr_partitioner_alloc_partition(
            shared->glyph_buffer_partitioner,
            req.glyphs_count * sizeof(gpu_glyph)
        );

        // Failed to create partition - create bigger text buffer
        if (!aux->owned_glyph_buffer_partition) {
            // todo rewrite (must sync)
        }
    }

    // Generate draw regions for texts
    for (size_t i = 0; i < cache->text_requests_count; i++) {
        text_request              req  = cache->text_requests[i];
        text_type_auxilary_state* aux = auxilary_get_utill(cache, req.owning_node)->state_ptr;
        dpr_partition*            prt = aux->owned_glyph_buffer_partition;
        if (!prt) continue; // text empty, nothing to upload

        dgs_segmenter_upload(segmenter, (dgs_upload_request){
            .target = (uint64_t)shared->glyph_buffer,
            .offset = dpr_partition_query_offset(prt),
            .source = req.glyphs,
            .bytes  = req.glyphs_count * sizeof(gpu_draw_item)
        });
    }

    // Prepare draw items, draw instances, draw clipboxes

    uint32_t        items_count; 
    uint64_t        items_bytes;
    gpu_draw_item*  items;

    uint32_t        instances_count = 0;
    uint64_t        instances_bytes = 0;
    gpu_instance*   instances;

    uint32_t        clipboxes_count; 
    uint64_t        clipboxes_bytes;
    gpu_clipbox*    clipboxes;
    
    // Generate GPU Items, findout instances count
    items_count = cache->draw_requests_count;
    items_bytes = cache->draw_requests_count * sizeof(gpu_draw_item);
    items = malloc(items_bytes); if (!items) goto _cleanup;
    for (uint32_t i = 0; i < items_count; i++) {
        draw_request req = cache->draw_requests[i];

        if (req.is_box_not_text) {
            int texture_index = 0; dgx_texture* texture; dgx_uv_2d uv;
            if (req.box_data.image && dui_injection_query_image(req.box_data.image, &texture, &uv)) {
                texture_index = dgx_hardware_resource_bind(hardware, dgx_resource_type_sampled_texture, texture);
            }

            items[i] = (gpu_draw_item){
                .transform      = req.transform,
                .atlas_position = uv,
                .texture_index  = texture_index,
                .clipbox_index  = req.clip_index,
                .shader_index   = req.box_data.shader,
                .r              = (float)req.box_data.tint.r / 255.0f,
                .g              = (float)req.box_data.tint.g / 255.0f,
                .b              = (float)req.box_data.tint.b / 255.0f,
                .a              = (float)req.box_data.tint.a / 255.0f
            };

            instances_count += 1;  // single box
        }
        else {
            auxilary_slot*            slot = auxilary_get_utill(cache, req.text_node);
            text_type_auxilary_state* aux  = slot->state_ptr;
            dpr_partition*            part = aux->owned_glyph_buffer_partition;
            if (!part) continue;

            dui_text_data text_data = *(const dui_text_data*)get_node_data(slot->key.node, slot->key.instance);
            dfont_font* font; if (!dui_injection_query_font(text_data.font, &font)) continue;

            uint32_t texture_index = dgx_hardware_resource_bind(hardware, dgx_resource_type_sampled_texture, dfont_get_texture(font));
            int signed_texture_index = -(int)texture_index; // is font

            items[i] = (gpu_draw_item){
                .transform      = req.transform,
                .atlas_position = (dgx_uv_2d){0, 0, 1, 1},
                .texture_index  = signed_texture_index,
                .clipbox_index  = req.clip_index,
                .shader_index   = text_data.shader,
                .r              = (float)text_data.tint.r / 255.0f,
                .g              = (float)text_data.tint.g / 255.0f,
                .b              = (float)text_data.tint.b / 255.0f,
                .a              = (float)text_data.tint.a / 255.0f,
            };

            instances_count += dpr_partition_query_size(part) / sizeof(gpu_glyph);
        }
    }

    // Generate GPU Instances
    instances_bytes = instances_count * sizeof(gpu_instance);
    instances = malloc(instances_bytes); if (!instances) goto _cleanup;
    uint32_t instance_idx = 0;
    for (int i = 0; i < cache->draw_requests_count; i++) {
        draw_request req = cache->draw_requests[i];
        if (req.is_box_not_text) {
            instances[instance_idx++] = (gpu_instance){
                .item   = i,
                .glyph  = -1
            };
        }
        else {
            auxilary_slot*            slot = auxilary_get_utill(cache, req.text_node);
            text_type_auxilary_state* aux  = slot->state_ptr;
            dpr_partition*            part = aux->owned_glyph_buffer_partition;
            if (!part) continue;
            
            size_t first  = dpr_partition_query_offset(part) / sizeof(gpu_glyph);
            size_t glyphs = dpr_partition_query_size(part) / sizeof(gpu_glyph);
            for (size_t g = 0; g < glyphs; g++) {
                instances[instance_idx++] = (gpu_instance){
                    .item   = i,
                    .glyph  = first + g
                };
            }
        }
    }

    // Generate GPU Clipboxes
    clipboxes_count = cache->clipbox_requests_count;
    clipboxes_bytes = cache->clipbox_requests_count * sizeof(gpu_draw_item);
    clipboxes       = malloc(clipboxes_bytes); if (!clipboxes) goto _cleanup;
    for (uint32_t i = 0; i < clipboxes_count; i++) {
        clipbox_request req = cache->clipbox_requests[i];
        clipboxes[i] = (gpu_clipbox){
            .transform = req.transform
        };
    }

    // Items buffer
    if (dgx_buffer_query_bytes(frame->draw_items_buffer) < items_bytes) {
        dgx_free_buffer(frame->draw_items_buffer);
        frame->draw_items_buffer = create_ssbo(hardware, items_bytes);
    }

    // Instanced buffer
    if (dgx_buffer_query_bytes(frame->instances_buffer) < instances_bytes) {
        dgx_free_buffer(frame->instances_buffer);
        frame->instances_buffer = create_ssbo(hardware, instances_bytes);
    }

    // Clipboxes buffer
    if (dgx_buffer_query_bytes(frame->clipboxes_buffer) < clipboxes_bytes) {
        dgx_free_buffer(frame->clipboxes_buffer);
        frame->clipboxes_buffer = create_ssbo(hardware, clipboxes_bytes);
    }

    // Uploads requests
    dgs_segmenter_upload(segmenter, (dgs_upload_request){
        .target = (uint64_t)frame->draw_items_buffer,
        .offset = 0,
        .source = &items,
        .bytes  = items_count * sizeof(gpu_draw_item)
    });

    dgs_segmenter_upload(segmenter, (dgs_upload_request){
        .target = (uint64_t)frame->clipboxes_buffer,
        .offset = 0,
        .source = &clipboxes,
        .bytes  = clipboxes_count * sizeof(gpu_clipbox)
    });

    dgs_segmenter_upload(segmenter, (dgs_upload_request){
        .target = (uint64_t)frame->instances_buffer,
        .offset = 0,
        .source = &instances,
        .bytes  = instances_count * sizeof(gpu_instance)
    });

    // Perform uploads
    dgx_timeline* internal = NULL;
    uint64_t internal_itr = 0;
    
    while (!dgx_segmenter_query_empty(segmenter)) {
        if (internal) dgx_timeline_wait(internal, internal_itr);
        
        uint64_t count; dgs_upload_request* requests;
        dgs_segmenter_continue(segmenter, &count, &requests);

        int last_upload = dgx_segmenter_query_empty(segmenter);
        if (!last_upload && !internal) {
            internal = dgx_create_timeline(hardware, &(dgx_timeline_create_info){
                .initial_value = 0
            });
        }

        // copy to staging memory
        char* mapped = dgx_staging_memory_map(staging_memory, staging_memory_region_offset, staging_memory_region_size);
        uint64_t offset = 0;
        for (uint64_t i = 0; i < count; i++) {
            dgs_upload_request req = requests[i];
            memcpy(mapped + offset, req.source, req.bytes);
            offset += req.bytes;
        }
        dgx_staging_memory_unmap(staging_memory);

        // record rewrite list
        dgx_command_list* list = dgx_create_command_list(hardware, &(dgx_command_list_create_info){
            .domain = dgx_command_domain_transfer,
            .aindex = command_list_allocator_index,
            .parent = NULL,
            .record = ui_upload_record,
            .params = &(ui_upload_params){
                .count    = count,
                .requests = requests,
                .staging  = staging_memory
            }
        });

        // submit gpu work
        dgx_command_list_submit(list, &(dgx_submit_info){
            .domain_work_group  = transfer_work_group_index,
            .signal_timeline    = last_upload ? signal_timeline : internal,
            .signal_value       = last_upload ? signal_value    : ++internal_itr
        });
    }

    if (internal) dgx_free_timeline(internal);

    // Set render parameters since buffer are ready
    frame->vertex_constants = (gpu_vertex_constants){
        .resolution_width        = cache->resolution_x,
        .resolution_height       = cache->resolution_y,
        .instances_buffer_index  = dgx_hardware_resource_bind(hardware, dgx_resource_type_storage_buffer, frame->instances_buffer),
        .draw_items_buffer_index = dgx_hardware_resource_bind(hardware, dgx_resource_type_storage_buffer, frame->draw_items_buffer),
        .glyphs_buffer_index     = dgx_hardware_resource_bind(hardware, dgx_resource_type_storage_buffer, shared->glyph_buffer),
    };
    frame->pixel_constants = (gpu_pixel_constants){
        .resolution_width   = cache->resolution_x,
        .resolution_height  = cache->resolution_y,
        .clips_buffer_index = dgx_hardware_resource_bind(hardware, dgx_resource_type_storage_buffer, frame->clipboxes_buffer),
        .sampler_index      = dgx_hardware_resource_bind(hardware, dgx_resource_type_sampler, shared->sampler),
    };

    // Mark to render
    frame->instances_to_render = instances_count;

_cleanup: 
    free_cached_text_requests(cache);               // Free text requests
    free(items); free(clipboxes); free(instances);  // Free allocated memory
}

void dui_gcmd_render(
    dui_frames* frames,
    uint32_t    frame_idx
) {
    single_frame* frame = &frames->frames[frame_idx % frames->count];
    if (frame->instances_to_render) {
        dgx_gcmd_bind_graphics_pipeline(frames->owning_shared->pipeline);

        dgx_gcmd_write_constants(
            frames->owning_shared->pipeline, dgx_shader_stage_vertex, 0, sizeof(gpu_vertex_constants), &frame->vertex_constants
        );

        dgx_gcmd_write_constants(
            frames->owning_shared->pipeline, dgx_shader_stage_pixel, 0, sizeof(gpu_pixel_constants), &frame->pixel_constants
        );

        dgx_gcmd_draw(0, 4, 0, frame->instances_to_render);
    }
}

// ===========================
// Text layout generation

void create_text_request(dui_cache* cache, cache_slot* slot, text_type_auxilary_state* aux) {
    const dui_text_data* tdata = get_node_data(slot->key.node, slot->key.instance);
    dfont_font* font; if (!dui_injection_query_font(tdata->font, &font)) return;
    const char* text = tdata->text;

    // If text empty or font invalid
    // Sent empty text request
    if (!text || !font) {
        text_request req = {
            .owning_node  = slot->key,
            .glyphs_count = 0,
            .glyphs       = NULL,
        };
        text_request_cache_push(cache, req);
        return; // overwrite current text buffer with empty text
    }

    // Count glyphs to allocate
    size_t glyph_count = 0; size_t extra_lines_count = 0;
    for (size_t i = 0; text[i] != '\0';) {
        uint32_t cp; i += dfont_utf8_decode(text, i, &cp);
        if (cp != '\n') glyph_count++; 
        else extra_lines_count++;
    }

    // Allocate glyphs buffer
    gpu_glyph* glyphs = glyph_count ? malloc(sizeof(gpu_glyph) * glyph_count) : NULL;
    if (glyph_count && !glyphs) {
        text_request req = { .owning_node = slot->key, .glyphs_count = 0, .glyphs = NULL };
        text_request_cache_push(cache, req); return;
    }

    // Find font scale
    const float font_scale = tdata->size / dfont_get_base_size(font);

    // Populate glyphs buffer
    const float ascent      = dfont_get_base_ascent(font)   * font_scale;
    const float descent     = dfont_get_base_descent(font)  * font_scale;
    const float line_gap    = dfont_get_base_line_gap(font) * font_scale;
    const float line_height = ascent - descent + line_gap;

    float    pen_x      = 0.0f;
    float    pen_y      = 0.0f;
    float    text_width = 0.0f; // max line width across all lines
    size_t   glyph_idx  = 0;
    uint32_t prev_cp    = 0;    // for kerning; 0 = no previous glyph

    for (size_t itr = 0; text[itr] != '\0';) {
        uint32_t cp;
        itr += dfont_utf8_decode(text, itr, &cp);

        // Handle newline
        if (cp == '\n') {
            if (pen_x > text_width) text_width = pen_x;
            pen_x   = 0.0f;
            pen_y  -= line_height;
            prev_cp = 0; // reset kerning across lines
            continue;
        }

        // Kerning between consecutive glyphs on the same line
        if (prev_cp) pen_x += dfont_get_kerning(font, prev_cp, cp);

        // Write glyph
        const dfont_glyph g = dfont_get_glyph(font, cp);
        glyphs[glyph_idx++] = (gpu_glyph){
            .atlas_position = g.atlas_position,
            .off_x          = pen_x + g.bearing_x * font_scale,
            .off_y          = (extra_lines_count * line_height + pen_y) - g.bearing_y * font_scale,
            .size_x         = g.size_x * font_scale,
            .size_y         = g.size_y * font_scale,
        };

        // Advance
        pen_x  += g.advance_x * font_scale;
        prev_cp = cp;
    }

    // Account for final line (no trailing newline)
    if (pen_x > text_width) text_width = pen_x;

    // Total pixel height: baseline of last line + full single-line cap height
    float text_height = -pen_y + ascent;

    // Store text dimensions
    aux->text_width  = text_width;
    aux->text_height = text_height;

    // Request text upload
    text_request req = {
        .owning_node  = slot->key,
        .glyphs_count = glyph_count,
        .glyphs       = glyphs,
    };
    text_request_cache_push(cache, req);
}

#endif // DEMIURG_USER_INTERFACE_IMPL
