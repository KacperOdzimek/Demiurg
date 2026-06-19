
/*
    Depedencies
*/

#include "/home/kacper/Projects/LightFramework/include/light/graphics.h"
#include "/home/kacper/Projects/LightFramework/include/light/linear_algebra.h"
#include "/home/kacper/Projects/LightFramework/include/light/font.h"
#include "/home/kacper/Projects/LightFramework/include/light/partitioner.h"

/*
    Implementation Injections
    Define in same file as user interface implementation
*/

#ifdef LIGHT_USER_INTERFACE_IMPL
    typedef struct lui_length     lui_length;
    typedef struct lui_text_data  lui_text_data;
    typedef struct lui_box_data   lui_box_data;

    // returns non-zero at success (if returned is valid pointer)
    int lui_injection_query_font (const char* font,  lfont**       font_out);
    int lui_injection_query_image(const char* image, lgx_texture** texture_out, lgx_uv_2d* uv_out);
#endif // LIGHT_USER_INTERFACE_IMPL

/*
    Header
*/

#ifndef LIGHT_USER_INTERFACE_H
#define LIGHT_USER_INTERFACE_H

// ===========================
// Forwards

typedef struct lui_type   lui_type;
typedef struct lui_node   lui_node;
typedef struct lui_cache  lui_cache;
typedef struct lui_shared lui_shared;
typedef struct lui_frames lui_frames;

// ===========================
// Layout Length

// variable representing infinte length
// not set to int max, to avoid overflows in implementation
// needs to be increased if you are rendering on a (64+)K screen
const static int lui_inf_length = 64 * 1000;

// structure representing 1d length
// min  - minimal size element can be rendered with
// max  - maximal size element can be rendered with
// flex - relative grow speed, compared to other elements inside an container (row/column)
// each length object is expected to met:
// min  <= max
// flex >= 0
typedef struct lui_length {
    int   min;  // minimum dimension
    int   max;  // maximum dimension
    float flex; // flex ratio
} lui_length;

// ===========================
// Colors

// basic 32 bit color
typedef struct lui_color {
    unsigned char r, g, b, a;
} lui_color;

// runtime hex to lui_color conversion
// letters case does not matter, '#' prefix is required
// if hex[7] is not '\0', then alpha channel is read, else it is set to FF
// LUI_HEX <- compile time alternative
static inline lui_color lui_hex(const char* hex);

// LUI_HEX <- compile time LUI_HEX alternative (definied later in the file)

// ===========================
// Node Typedefs

typedef struct lui_node_layout_state {
    lui_length              measured_width;     // desired width  of this node
    lui_length              measured_height;    // desired height of this node
    int                     given_width;        // received width
    int                     given_height;       // received height
    int                     hori_offset;        // node center horizontal offset from parent center
    int                     vert_offset;        // node center vertical offset from parent center
} lui_node_layout_state;

typedef void (lui_node_auxilary_func_signature)(
    const void*             node_data,          // node data
    void*                   auxilary            // node auxilary buffer if requested by type
);
typedef lui_node_auxilary_func_signature* lui_node_auxilary_func;

typedef void (lui_node_auxilary_destructor_func_signature)(
    void*                   auxilary            // node auxilary buffer - do not free it
);
typedef lui_node_auxilary_destructor_func_signature* lui_node_auxilary_destructor_func;

typedef void(lui_node_layout_func_signature)(
    const void*             node_data,          // node data
    lui_node_layout_state*  node_state,         // node own state
    size_t                  children_count,     // node children count
    lui_node_layout_state** children_states,    // node children states
    void*                   auxilary            // node auxilary buffer if requested by type
);
typedef lui_node_layout_func_signature* lui_node_layout_func;

typedef void(lui_node_render_func_signature)(
    const void*             node_data,          // node data
    lla_mat2x3*             transform,          // given transform, can be changed
    int                     resolution_x,       // screen resolution x
    int                     resolution_y,       // screen resolution y
    void*                   auxilary            // node auxilary buffer if requested by type
);
typedef lui_node_render_func_signature* lui_node_render_func;

typedef struct lui_type {
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
    lui_node_auxilary_destructor_func auxilary_destructor;

    // Auxilary Stage

    // Auxilary stage
    // Allow for node own data changes (eg. caching some preprocessed state)
    // Unless it is convenient, this pass shall not be used, to preserve data-oriented-design,
    // and composability. Used to generate text format for GPU in implementation.
    lui_node_auxilary_func  auxilary;

    // Layout Stages

    // First layout stage
    // Generates desired nodes widths, bottom-up
    // IN:  [children measured width]
    // OUT: [own measured width]
    lui_node_layout_func    width_measure;

    // Second layout stage
    // Generates actuall nodes widths, top-down
    // IN:  [width measurements, own given width]
    // OUT: [children given width]
    lui_node_layout_func    width_distribute;

    // Third layout stage
    // Generates desired nodes widths, bottom-up
    // IN:  [given widths, children measured heights]
    // OUT: [own measured height]
    lui_node_layout_func    height_measure;

    // Fourth layout stage
    // Generates actuall nodes heights, top-down
    // IN:  [given widths, measured heights, own given height]
    // OUT  [children given heights]
    lui_node_layout_func    height_distribute;

    // Fifth layout stage
    // Position nodes on screen, top-down
    // IN:  [all widths and heights]
    // OUT: [node offset from ]
    lui_node_layout_func    position;

    // Rendering Stages

    // First render stage
    // Allow altering children render transforms, top down
    // IN:  [complete layout states, parent render transform]
    // OUT: [own and children render transform]
    lui_node_render_func    transform;
} lui_type;

typedef enum lui_flag {
    lui_flag_instanced_data     = 1 << 0,
    lui_flag_instanced_child    = 1 << 1,
    lui_flag_ignore_min_width   = 1 << 2,
    lui_flag_ignore_min_height  = 1 << 3,
    lui_flag_ignore_max_width   = 1 << 4,
    lui_flag_ignore_max_height  = 1 << 5,
} lui_flag;

typedef struct lui_node {
    const lui_type* type;
    const uint32_t  flags;
    
    union {
        const lui_node* child;
        size_t          child_offset;
    };

    union {
        const void*     data;
        size_t          data_offset;
    };
} lui_node;

// Sentinel value to mark array end
#define LUI_ARRAY_END (lui_node){.type = NULL, .child = NULL, .data = NULL}

// ===========================
// Predefinied Functions
// Those implement basic box/overlay behavior, used by most nodes

// width = (min = max(children mins), max = max(children max), flex = 1.0f if min != max, else 0)
lui_node_layout_func_signature lui_overlay_width_measure_func;

// children width = parent width, with applied maxes
lui_node_layout_func_signature lui_overlay_width_distribute_func;

// height = (min = max(children mins), max = max(children max), flex = 1.0f if min != max, else 0)
lui_node_layout_func_signature lui_overlay_height_measure_func;

// children height = parent height, with applied maxes
lui_node_layout_func_signature lui_overlay_height_distribute_func;

// centers children inside parent
lui_node_layout_func_signature lui_overlay_position_func;

// ===========================
// Architectural Node Types

// Sets instance pointer to own data value
// Data shall be arbitrary pointer (or offset in current instance) to instance structure
extern const lui_type lui_instance_type;

// Layout-rebuild gate for the subtree - children layout will only
// be rebuilt if invalidation node was marked with a proper dirty flag
// No data, single child
extern const lui_type lui_invalidation_type;
typedef enum lui_invalidation_flag {
    lui_invalidation_flag_auxilary          = 63,
    lui_invalidation_flag_width_measure     = 62,
    lui_invalidation_flag_width_distribute  = 60,
    lui_invalidation_flag_height_measure    = 56,
    lui_invalidation_flag_height_distribute = 48,
    lui_invalidation_flag_position          = 32,
    lui_invalidation_flag_none              = 0,
    lui_invalidation_flag_all               = 63,
} lui_invalidation_flag;
typedef struct lui_invalidation_data {
    lui_invalidation_flag flag_consumable;
    lui_invalidation_flag flag_always;
} lui_invalidation_data;

// ===========================
// Layout Node Types

// During layout, overwrites selected fields with provided values
// Data is lui_sizebox_data, single child
extern const lui_type lui_sizebox_type;
typedef enum lui_sizebox_overwrite_flag {
    lui_sizebox_overwrite_none        = 0,
    lui_sizebox_overwrite_all         = 255,
    lui_sizebox_overwrite_all_width   = 7,
    lui_sizebox_overwrite_all_height  = 56,

    lui_sizebox_overwrite_width_min   = 1 << 0,
    lui_sizebox_overwrite_width_max   = 1 << 1,
    lui_sizebox_overwrite_width_flex  = 1 << 2,

    lui_sizebox_overwrite_height_min  = 1 << 3,
    lui_sizebox_overwrite_height_max  = 1 << 4,
    lui_sizebox_overwrite_height_flex = 1 << 5
} lui_sizebox_overwrite_flag;
typedef struct lui_sizebox_data {
    lui_sizebox_overwrite_flag  flag;
    lui_length                  width;
    lui_length                  height;    
} lui_sizebox_data;

// Padds child inside self
// Data is lui_padding_data, single child
extern const lui_type lui_padding_type;
typedef struct lui_padding_data {
    lui_length left, right, top, bottom;
} lui_padding_data;

// Layouts children one on another
// The first child is deepest, rendered first
// No data, array children
extern const lui_type lui_overlay_type;

// Layouts children in a row, left to right
// Data is lui_row_data, array children
extern const lui_type lui_row_type;
typedef struct lui_row_data {
    float           vertical_align;     // 0 - align top,  0.5 - align center, 1.0 - align bottom, other values also work
    lui_length      spacing;            // spacing between children
} lui_row_data;

// Layouts children in a column, top to down
// Data is lui_column_data, array children
extern const lui_type lui_column_type;
typedef struct lui_column_data {
    float           horizontal_align;   // 0 - align left,  0.5 - align center, 1.0 - align right, other values also work
    lui_length      spacing;            // spacing between children
} lui_column_data;

// ===========================
// Rendering Node Types

// Constrains rendering to own dimensions
// No data, single child
extern const lui_type lui_clipbox_type;

// Adds node depth offset
// Decreasing depth means going 'into' the screen
// Data is lui_depth_data, ingle childed
extern const lui_type lui_depth_type;
typedef struct lui_depth_data {
    short depth_change;
} lui_depth_data;

// Box render primitive
// Data is lui_box_data, single child
extern const lui_type lui_box_type;
typedef struct lui_box_data {
    lui_color       tint;               // box color
    const char*     image;              // image name/path, may be NULL
    uint32_t        shader;             // shader effect index
} lui_box_data;

// Text render primitive
// Data is lui_text_data, single child
extern const lui_type lui_text_type;
typedef struct lui_text_data {
    unsigned int    size;               // font size
    const char*     font;               // font name/path
    const char*     text;               // text pointer
    lui_color       tint;               // text color modyficator
    uint32_t        shader;             // shader effect index
} lui_text_data;

// ===========================
// Cache

lui_cache* lui_create_cache();
void lui_free_cache(lui_cache*);

void lui_update_cache(
    lui_cache*      cache,
    const lui_node* root,
    int             resolution_x,
    int             resolution_y
);

// ===========================
// Rendering API

typedef struct lui_shared_create_info {
    lgx_render_target_layout*   pipeline_render_target_layout;
    uint32_t                    additional_pipeline_descriptors_layouts_count;
    lgx_descriptor_layout**     additional_pipeline_descriptors_layouts;
    lgx_shader*                 pipeline_vertex_shader;
    lgx_shader*                 pipeline_pixel_shader;
} lui_shared_create_info;

lui_shared* lui_create_shared(lgx_hardware*, const lui_shared_create_info*);
void lui_free_shared(lui_shared*);

typedef struct lui_frames_create_info {
    lui_shared* shared;
    uint32_t    frames_in_flight_count;
} lui_frames_create_info;

lui_frames* lui_create_frames(lgx_hardware*, const lui_frames_create_info*);
void lui_free_frames(lui_frames*);

void lui_upload_cache(
    lui_cache*          cache,
    lui_shared*         shared,
    lui_frames*         frames,
    uint32_t            frame_idx,
    lgx_command_list*   command_list,
    lgx_hardware_queue* queue_for_uploads,
    lgx_staging_memory* staging_memory,
    uint64_t            staging_memory_region_offset,
    uint64_t            staging_memory_region_size,
    lgx_cpu_signal*     upload_finished_cpu,
    lgx_gpu_signal*     upload_finished_gpu
);

void lui_gcmd_render(
    lgx_command_list*   target,
    lui_frames*         frames,
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

static inline lui_color lui_hex(const char* hex) {
    lui_color result;
    result.r = LUI_HEX_BYTE(hex[1], hex[2]);
    result.g = LUI_HEX_BYTE(hex[3], hex[4]);
    result.b = LUI_HEX_BYTE(hex[5], hex[6]);

    // if 8 digits after #, read alpha
    if (hex[7] != '\0' && hex[8] != '\0') result.a = LUI_HEX_BYTE(hex[7], hex[8]);
    else result.a = 0xFF;

    return result;
}

// compile time lui_color from hex builder
// allows both lower and upper case letters
// may include alpha (8 hex digits) or not (6 hex digits)
// '#' prefix required
#define LUI_HEX(s) (lui_color){ \
    LUI_HEX_BYTE(s[1], s[2]), \
    LUI_HEX_BYTE(s[3], s[4]), \
    LUI_HEX_BYTE(s[5], s[6]), \
    (sizeof(s) > 8 ? LUI_HEX_BYTE(s[7], s[8]) : 0xFF) \
}

#endif // LIGHT_USER_INTERFACE_H

#ifdef LIGHT_USER_INTERFACE_IMPL

// Implementation Notes:
// 1 - last_frame_used_in_render values reference
//  last_frame_used_in_render is used to clear hashmap from dead nodes
/*
    0     - empty cell
    1     - imposible value, to force garbage collection on all
    2     - just added, not rendered yet
    2-255 - rendered at frame of index
*/

#include <stdlib.h>

// ===========================
// Math helpers

static inline int min_int(int a, int b) { return a < b ? a : b; }
static inline int max_int(int a, int b) { return a < b ? b : a; }

static inline int limit_length(int length, lui_length limits) {
    if (length > limits.max) length = limits.max;
    if (length < limits.min) length = limits.min;
    return length;
}

static inline int limit_length_gain(int current, lui_length limit, int proposed) {
    if (current + proposed < limit.min) return limit.min - current;
    if (current + proposed > limit.max) return limit.max - current;
    return proposed;
}

static inline lla_mat2x3 mat2x3_scale(lla_mat2x3 m, float sx, float sy) {
    m.m[0][0] *= sx;  m.m[0][1] *= sy;
    m.m[1][0] *= sx;  m.m[1][1] *= sy;
    return m;
}

static inline lla_mat2x3 mat2x3_offset(lla_mat2x3 m, float ox, float oy) {
    m.m[2][0] += ox; m.m[2][1] += oy;
    return m;
}

// ===========================
// Types helper

#define box_behavior_type (lui_type){                           \
    .array_child        = 0,                                    \
    .auxilary_bytes     = 0,                                    \
    .auxilary           = NULL,                                 \
    .width_measure      = lui_overlay_width_measure_func,       \
    .width_distribute   = lui_overlay_width_distribute_func,    \
    .height_measure     = lui_overlay_height_measure_func,      \
    .height_distribute  = lui_overlay_height_distribute_func,   \
    .position           = lui_overlay_position_func,            \
    .transform          = NULL                                  \
} 

// ===========================
// Instance type
// This type is specially handled in pass implementation
const lui_type lui_instance_type = box_behavior_type;

// ===========================
// Invalidation type
// This type is specially handled in pass implementation
const lui_type lui_invalidation_type = box_behavior_type;

// ===========================
// Overlay Type

void lui_overlay_width_measure_func(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)node_data; (void)auxilary; lui_length own = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        lui_length child = children_states[i]->measured_width;
        own.min  = max_int(own.min, child.min);
        own.max  = max_int(own.max, child.max);
    }

    if (own.min != own.max) own.flex = 1.0f;
    node_state->measured_width = own;
}

void lui_overlay_width_distribute_func(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)node_data; (void)auxilary;

    for (size_t i = 0; i < children_count; ++i) {
        children_states[i]->given_width = limit_length(node_state->given_width, children_states[i]->measured_width);
    }
}

void lui_overlay_height_measure_func(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)node_data; (void)auxilary; lui_length own = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        lui_length child = children_states[i]->measured_height;
        own.min  = max_int(own.min, child.min);
        own.max  = max_int(own.max, child.max);
    }

    if (own.min != own.max) own.flex = 1.0f;
    node_state->measured_height = own;
}

void lui_overlay_height_distribute_func(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)node_data; (void)auxilary;

    for (size_t i = 0; i < children_count; ++i) {
        children_states[i]->given_height = limit_length(node_state->given_height, children_states[i]->measured_height);
    }
}

void lui_overlay_position_func(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)node_data; (void)auxilary;

    for (size_t i = 0; i < children_count; ++i) {
        children_states[i]->hori_offset = 0;
        children_states[i]->vert_offset = 0;
    }
}

const lui_type lui_overlay_type = {
    .array_child        = 1,
    .auxilary_bytes     = 0,
    .auxilary           = NULL,
    .width_measure      = lui_overlay_width_measure_func,
    .width_distribute   = lui_overlay_width_distribute_func,
    .height_measure     = lui_overlay_height_measure_func,
    .height_distribute  = lui_overlay_height_distribute_func,
    .position           = lui_overlay_position_func,
    .transform          = NULL
};

// ===========================
// Sizebox Type

void sizebox_width_measure(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    const lui_sizebox_data* data = node_data;
    lui_overlay_width_measure_func(node_data, node_state, children_count, children_states, auxilary);
    if (data->flag & lui_sizebox_overwrite_width_min)   node_state->measured_width.min   = data->width.min;
    if (data->flag & lui_sizebox_overwrite_width_max)   node_state->measured_width.max   = data->width.max;
    if (data->flag & lui_sizebox_overwrite_width_flex)  node_state->measured_width.flex  = data->width.flex;
}

void sizebox_height_measure(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    const lui_sizebox_data* data = node_data;
    lui_overlay_height_measure_func(node_data, node_state, children_count, children_states, auxilary);
    if (data->flag & lui_sizebox_overwrite_height_min)  node_state->measured_height.min  = data->height.min;
    if (data->flag & lui_sizebox_overwrite_height_max)  node_state->measured_height.max  = data->height.max;
    if (data->flag & lui_sizebox_overwrite_height_flex) node_state->measured_height.flex = data->height.flex;
}

const lui_type lui_sizebox_type = {
    .array_child        = 0,
    .auxilary_bytes     = 0,
    .auxilary           = NULL,
    .width_measure      = sizebox_width_measure,
    .width_distribute   = lui_overlay_width_distribute_func,
    .height_measure     = sizebox_height_measure,
    .height_distribute  = lui_overlay_height_distribute_func,
    .position           = lui_overlay_position_func,
    .transform          = NULL
};

// ===========================
// Padding Type

static inline int padding_distribute_length(
    int* a, lui_length al,
    int* b, lui_length bl,
    int* c, lui_length cl,
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
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary;
    const lui_padding_data* data = node_data;
    lui_length own = {0, 0, 0.0f};
    
    int child_min = 0, child_max = 0;
    if (children_count > 0) {
        child_min = children_states[0]->measured_width.min;
        child_max = children_states[0]->measured_width.max;
    }

    int w_min = data->left.min + child_min + data->right.min;
    int w_max = data->left.max + child_max + data->right.max;

    node_state->measured_width = (lui_length){
        .min  = w_min,
        .max  = w_max,
        .flex = (w_min != w_max) ? 1.0f : 0.0f,
    };
}

void padding_width_distribute(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary; if (children_count == 0) return;
    const lui_padding_data* data = node_data;
    lui_node_layout_state* child = children_states[0];

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
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary; const lui_padding_data* data = node_data;

    int child_min = 0, child_max = 0;
    if (children_count > 0) {
        child_min = children_states[0]->measured_height.min;
        child_max = children_states[0]->measured_height.max;
    }

    int h_min = data->top.min + child_min + data->bottom.min;
    int h_max = data->top.max + child_max + data->bottom.max;

    node_state->measured_height = (lui_length){
        .min  = h_min,
        .max  = h_max,
        .flex = (h_min != h_max) ? 1.0f : 0.0f,
    };
}

void padding_height_distribute(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary; if (children_count == 0) return;
    const lui_padding_data* data = node_data;
    lui_node_layout_state* child = children_states[0];

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

const lui_type lui_padding_type = {
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
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary; 
    const lui_row_data* data = (const lui_row_data*)node_data;
    lui_length          own  = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        lui_length child = children_states[i]->measured_width;
        own.min += child.min; own.max += child.max;
    }

    size_t spaces = children_count ? children_count - 1 : 0;
    own.min += spaces * data->spacing.min;

    if (own.max != lui_inf_length && data->spacing.max != lui_inf_length) own.max += spaces * data->spacing.max;
    else own.max = lui_inf_length;

    if (own.min != own.max) own.flex = 1.0f;
    node_state->measured_width = own;
}

void row_width_distribute(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary; const lui_row_data* data = (const lui_row_data*)node_data;

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
            lui_length  m = children_states[i]->measured_width;
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
        lui_node_layout_state* child = children_states[i];
        cursor_x += child->given_width / 2;
        child->hori_offset = cursor_x;
        cursor_x += child->given_width / 2 + spacing;
    }
}

void row_position(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary; const lui_row_data* data = (const lui_row_data*)node_data;

    // Position children in vertial axis
    for (size_t i = 0; i < children_count; ++i) {
        lui_node_layout_state* child = children_states[i];
        child->vert_offset = (node_state->given_height - child->given_height) * (0.5f - data->vertical_align);
    }
}

const lui_type lui_row_type = {
    .array_child        = 1,
    .auxilary_bytes     = 0,
    .auxilary           = NULL,
    .width_measure      = row_width_measure,
    .width_distribute   = row_width_distribute,
    .height_measure     = lui_overlay_height_measure_func,
    .height_distribute  = lui_overlay_height_distribute_func,
    .position           = row_position,
    .transform          = NULL
};

// ===========================
// Column Type

void column_height_measure(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary;
    const lui_column_data* data = (const lui_column_data*)node_data;
    lui_length             own  = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        lui_length child = children_states[i]->measured_height;
        own.min += child.min; own.max += child.max;
    }

    size_t spaces = children_count ? children_count - 1 : 0;
    own.min += spaces * data->spacing.min;

    if (own.max != lui_inf_length && data->spacing.max != lui_inf_length) own.max += spaces * data->spacing.max;
    else own.max = lui_inf_length;

    if (own.min != own.max) own.flex = 1.0f;
    node_state->measured_height = own;
}

void column_height_distribute(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary;
    const lui_column_data* data = (const lui_column_data*)node_data;

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
            lui_length  m = children_states[i]->measured_height;
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
        lui_node_layout_state* child = children_states[i];
        cursor_y -= child->given_height / 2;
        child->vert_offset = cursor_y;
        cursor_y -= child->given_height / 2 + spacing;
    }
}

void column_position(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    (void)auxilary; const lui_column_data* data = (const lui_column_data*)node_data;

    // Position children in horizontal axis
    for (size_t i = 0; i < children_count; ++i) {
        lui_node_layout_state* child = children_states[i];
        child->hori_offset = (node_state->given_width  - child->given_width)  * (data->horizontal_align - 0.5f);
    }
}

const lui_type lui_column_type = {
    .array_child        = 1,
    .auxilary_bytes     = 0,
    .auxilary           = NULL,
    .width_measure      = lui_overlay_width_measure_func,
    .width_distribute   = lui_overlay_width_distribute_func,
    .height_measure     = column_height_measure,
    .height_distribute  = column_height_distribute,
    .position           = column_position,
    .transform          = NULL
};

// ===========================
// Clipbox Type
// This type is specially handled in pass implementation
const lui_type lui_clipbox_type = box_behavior_type;

// ===========================
// Depth Type
// This type is specially handled in pass implementation
const lui_type lui_depth_type = box_behavior_type;

// ===========================
// Box Type
// This type is specially handled in pass implementation
const lui_type lui_box_type = box_behavior_type;

// ===========================
// Text type
// This type is specially handled in pass implementation

typedef struct text_type_auxilary_state {
    lpr_partitioner*    partitioner;
    lpr_partition*      owned_glyph_buffer_partition;
    float               text_width;
    float               text_height;
} text_type_auxilary_state;

void text_auxilary_destructor(void* auxilary) {
    text_type_auxilary_state* aux = auxilary;
    if (aux->owned_glyph_buffer_partition) lpr_partitioner_free_partition(aux->partitioner, aux->owned_glyph_buffer_partition);
    aux->owned_glyph_buffer_partition = NULL;
}

void text_width_measure(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    lui_overlay_width_measure_func(node_data, node_state, children_count, children_states, auxilary);
    text_type_auxilary_state* aux = auxilary;
    node_state->measured_width.min = max_int(node_state->measured_width.min, (int)aux->text_width);
    node_state->measured_width.max = max_int(node_state->measured_width.max, (int)aux->text_width);
    if (node_state->measured_width.min != node_state->measured_width.max) node_state->measured_width.flex = 1.0f;
}

void text_height_measure(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states,
    void*                   auxilary
) {
    lui_overlay_height_measure_func(node_data, node_state, children_count, children_states, auxilary);
    text_type_auxilary_state* aux = auxilary;
    node_state->measured_height.min = max_int(node_state->measured_height.min, (int)aux->text_height);
    node_state->measured_height.max = max_int(node_state->measured_height.max, (int)aux->text_height);
    if (node_state->measured_height.min != node_state->measured_height.max) node_state->measured_height.flex = 1.0f;
}

const lui_type lui_text_type = {
    .array_child            = 0,
    .auxilary_bytes         = sizeof(text_type_auxilary_state),
    .auxilary               = NULL,
    .auxilary_destructor    = text_auxilary_destructor,
    .width_measure          = text_width_measure,
    .width_distribute       = lui_overlay_width_distribute_func,
    .height_measure         = text_height_measure,
    .height_distribute      = lui_overlay_height_distribute_func,
    .position               = lui_overlay_position_func,
    .transform              = NULL
};

// ===========================
// Node fields reads

static inline const void* get_node_data(const lui_node* node, const char* instance) {
    if (node->flags & lui_flag_instanced_data) return (void*)(instance + node->data_offset);
    return node->data;
}

static inline const lui_node* get_node_child(const lui_node* node, const char* instance) {
    if (node->flags & lui_flag_instanced_child) return (const lui_node*)(instance + node->child_offset);
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

struct lui_cache {
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
};

lui_cache* lui_create_cache() {
    lui_cache* cache = calloc(1, sizeof(lui_cache));
    return cache;
}

static void auxilary_hashmap_garbage_collect(lui_cache* cache);
static void free_cached_text_requests(lui_cache* cache);
void lui_free_cache(lui_cache* cache) {
    if (!cache) return;

    // Free all auxilary slots by using impossible value
    cache->frame_index = 1;
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
    const lui_node* node;
    const void*     instance;
} node_stable_index;

typedef struct cache_slot {
    node_stable_index       key;
    size_t                  value_child_count;
    lui_node_layout_state   value_state;
    unsigned char           last_frame_used_in_render;  
} cache_slot;

typedef struct auxilary_slot {
    node_stable_index       key;
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
// void       PREFIX##_hashmap_grow             (lui_cache* cache);
// SLOT_TYPE* PREFIX##_hashmap_get              (lui_cache* cache, node_stable_index key, int insert_if_none)
// void       PREFIX##_hashmap_garbage_collect  (lui_cache* cache) 
// Define HASHMAP_SLOT_INITIALIZER to define default slot value
// Define HASHMAP_SLOT_DESTRUCTOR(slot ptr) to set garbage collector slot free method
#define DEFINE_HASHMAP_FUNCS(PREFIX, SLOT_TYPE, SLOTS_FIELD, CAP_FIELD, FILL_FIELD) \
\
static SLOT_TYPE* PREFIX##_hashmap_get                                          \
(lui_cache* cache, node_stable_index key, int insert_if_none);                  \
\
static void PREFIX##_hashmap_grow(lui_cache* cache) {                           \
    size_t old_cap = cache->CAP_FIELD;                                          \
    SLOT_TYPE* old_slots = cache->SLOTS_FIELD;                                  \
\
    size_t new_cap = old_cap ? old_cap * 2 : 64;                                \
\
    cache->SLOTS_FIELD = calloc(new_cap, sizeof(*cache->SLOTS_FIELD));          \
    cache->CAP_FIELD = new_cap;                                                 \
    cache->FILL_FIELD = 0;                                                      \
\
    for (size_t i = 0; i < old_cap; ++i) {                                      \
        if (!old_slots[i].last_frame_used_in_render) continue;                  \
        SLOT_TYPE* dst = PREFIX##_hashmap_get(cache, old_slots[i].key, 1);      \
        *dst = old_slots[i];                                                    \
    }                                                                           \
\
    free(old_slots);                                                            \
}                                                                               \
\
static SLOT_TYPE* PREFIX##_hashmap_get(                                         \
    lui_cache* cache, node_stable_index key, int insert_if_none) {              \
    if ((cache->FILL_FIELD + 1) * 10 >= cache->CAP_FIELD * 7) {                 \
        PREFIX##_hashmap_grow(cache);                                           \
    }                                                                           \
\
    size_t mask = cache->CAP_FIELD - 1;                                         \
    size_t idx  = hash_key(key) & mask;                                         \
\
    for (;;) {                                                                  \
        SLOT_TYPE* slot = &cache->SLOTS_FIELD[idx];                             \
\
        if (!slot->last_frame_used_in_render) {                                 \
            if (insert_if_none) {                                               \
                *slot = (SLOT_TYPE)HASHMAP_SLOT_INITIALIZER;                    \
                ++cache->FILL_FIELD;                                            \
                return slot;                                                    \
            }                                                                   \
            else return NULL;                                                   \
        }                                                                       \
\
        if (slot->key.node == key.node && slot->key.instance == key.instance) { \
            return slot;                                                        \
        }                                                                       \
\
        idx = (idx + 1) & mask;                                                 \
    }                                                                           \
}                                                                               \
\
static void PREFIX##_hashmap_garbage_collect(lui_cache* cache) {                \
    for (size_t i = 0; i < cache->CAP_FIELD; i++) {                             \
        SLOT_TYPE*     slot = &cache->SLOTS_FIELD[i];                           \
        unsigned char* time = &slot->last_frame_used_in_render;                 \
        if (*time && *time != cache->frame_index) {                             \
            HASHMAP_SLOT_DESTRUCTOR(slot);                                      \
            cache->FILL_FIELD--; *time = 0;                                     \
        }                                                                       \
    }                                                                           \
}

#define HASHMAP_SLOT_INITIALIZER {.key = key, .last_frame_used_in_render = 2}
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

#define HASHMAP_SLOT_INITIALIZER {.key = key, .state_ptr = NULL, .last_frame_used_in_render = 2}
#define HASHMAP_SLOT_DESTRUCTOR(slot_ptr) auxilary_hashmap_slot_destructor(slot_ptr)
DEFINE_HASHMAP_FUNCS(
    auxilary, auxilary_slot, auxilary_slots, auxilary_capacity, auxilary_fill
);

// Gets slot, always inserts, as cache must always exist for node
static inline cache_slot* cache_get_utill(lui_cache* cache, node_stable_index index) {
    return cache_hashmap_get(cache, index, 1);
}

// Gets slot, inserts if type size != 0, only then slot must exist, allocs memory if needed
static inline auxilary_slot* auxilary_get_utill(lui_cache* cache, node_stable_index index) {
    if (!index.node->type->auxilary_bytes) return NULL; // none desired
    auxilary_slot* slot = auxilary_hashmap_get(cache, index, 1);
    if (!slot->state_ptr) slot->state_ptr = calloc(1, index.node->type->auxilary_bytes);
    return slot;
}

// ===========================
// Cache dynamic arrays

struct draw_request {
    lla_mat2x3              transform;
    int                     clip_index;
    short                   depth_index;
    char                    is_box_not_text;
    union {
        lui_box_data        box_data;
        node_stable_index   text_node;
    };
};

struct text_request {
    node_stable_index       owning_node;
    size_t                  glyphs_count;
    struct gpu_glyph*       glyphs;
};

struct clipbox_request {
    lla_mat2x3              transform;
};

// Definies one function:
// void PREFIX##_cache_push (lui_cache* cache, ELEMENT_TYPE element);
#define DEFINE_DYNAMIC_ARRAY_FUNCS(PREFIX, ELEMENT_TYPE, ARRAY_FIELD, CAP_FIELD, CNT_FIELD)     \
static int PREFIX##_cache_push(lui_cache* cache, ELEMENT_TYPE element) {                        \
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

static inline void free_cached_text_requests(lui_cache* cache) {
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
    if (node_slot->key.node->type != &lui_invalidation_type) return 1;
    lui_invalidation_data* inv_data = (lui_invalidation_data*)data; // special case where const may be discarded

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
    lui_cache*              cache;      // cache owning cache slots
    size_t                  capacity;   // in cache_slot pointers
    size_t                  position;   // in cache_slot pointers
    cache_slot**            slots;      // sized capacity, node cache slots in enter order
    lui_node_layout_state** states;     // sized capacity, node layout states in children oreder
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
        lui_node_layout_state** new_sts = realloc(walk_order->states,   new_cap * sizeof(lui_node_layout_state*));
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
    const lui_node* node  = current->key.node;
    const lui_node* child = get_node_child(current->key.node, current->key.instance);
    size_t          count = 0;
    int             scc   = 1;

    // change instance for subtree
    if (node->type == &lui_instance_type) {
        instance = get_node_data(current->key.node, instance);
    }

    if (!node->type->array_child && child) {
        cache_slot*     child_slot = cache_get_utill(walk_order->cache, (node_stable_index){child, instance});
        auxilary_slot*  auxlr_slot = auxilary_get_utill(walk_order->cache, (node_stable_index){child, instance});
        scc &= caches_walk_order_push(walk_order, child_slot, auxlr_slot); count++;
    }
    else if (child) for (const lui_node* cc = child; cc->type != NULL; cc++) {
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

// Auxilary pass
// Top-down pass, allowing for own data rebuild
// Builds text

void create_text_request(lui_cache* cache, cache_slot* slot, text_type_auxilary_state* aux);
void auxilary_dfs(
    lui_cache*          cache,
    caches_walk_order*  walk_order,
    cache_slot*         current,
    auxilary_slot*      auxilary,
    size_t              first_child
) {
    cache_slot**    children    = &walk_order->slots[first_child];
    auxilary_slot** auxilaries  = &walk_order->auxilary[first_child];
    size_t*         subtrees    = &walk_order->subtree[first_child];
    const void*     data        = get_node_data(current->key.node, current->key.instance);

    // special case for text - update GPU glyphs buffer
    if (current->key.node->type == &lui_text_type) {
        create_text_request(cache, current, (text_type_auxilary_state*)auxilary->state_ptr);
    }

    // do call
    lui_node_auxilary_func func = current->key.node->type->auxilary;
    if (func != NULL) func(data, auxilary ? auxilary->state_ptr : NULL);

    // recurse
    if (find_shall_recurse(current, data, invalidation_flag_only_auxilary)) {
        size_t child_first_child = first_child + current->value_child_count;
        for (size_t i = 0; i < current->value_child_count; i++) {
            auxilary_dfs(cache, walk_order, children[i], auxilaries[i], child_first_child);
            child_first_child += subtrees[i] - 1;
        }
    }
}

// Generic layout dfs generation macros

// Definies function:
// void PREFIX##_dfs(caches_walk_order* walk_order, cache_slot* current, auxilary_slot* auxilary, size_t first_child)
// VA ARGS is code appended in recursive function after own call and recurse
#define LAYOUT_BOTTOM_UP_DFS(PREFIX, TYPE_FUNC_NAME, INV_PASS_ONLY_FLAG, ...)                       \
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
    lui_node_layout_func func = current->key.node->type->TYPE_FUNC_NAME;                            \
    if (func != NULL) func(                                                                         \
        data, &current->value_state, current->value_child_count, &walk_order->states[first_child],  \
        auxilary ? auxilary->state_ptr : NULL                                                       \
    );                                                                                              \
\
    __VA_ARGS__                                                                                     \
}

// Definies function:
// void PREFIX##_dfs(caches_walk_order* walk_order, cache_slot* current, auxilary_slot* auxilary, size_t first_child)
// VA ARGS is code appended in recursive function before own call and recurse
#define LAYOUT_TOP_DOWN_DFS(PREFIX, TYPE_FUNC_NAME, INV_PASS_ONLY_FLAG, ...)                        \
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
    lui_node_layout_func func = current->key.node->type->TYPE_FUNC_NAME;                            \
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

// additionaly handle ignore flags
LAYOUT_BOTTOM_UP_DFS(
    width_measure, width_measure, invalidation_flag_only_width_measure,
    if (current->key.node->flags & lui_flag_ignore_min_width) {
        current->value_state.measured_width.min = 0;
        current->value_state.measured_width.flex = 1.0f;
    }
    if (current->key.node->flags & lui_flag_ignore_max_width) {
        current->value_state.measured_width.max  = lui_inf_length;
        current->value_state.measured_width.flex = 1.0f;
    }
);

// additionaly ensure received width
// is within node measured limits
LAYOUT_TOP_DOWN_DFS(
    width_distribute, width_distribute, invalidation_flag_only_width_distribute,
    current->value_state.given_width = limit_length(
        current->value_state.given_width,
        current->value_state.measured_width
    );
);

// additionaly handle ignore flags
LAYOUT_BOTTOM_UP_DFS(
    height_measure, height_measure, invalidation_flag_only_height_measure,
    if (current->key.node->flags & lui_flag_ignore_min_height) {
        current->value_state.measured_height.min = 0;
        current->value_state.measured_height.flex = 1.0f;
    }
    if (current->key.node->flags & lui_flag_ignore_max_height) {
        current->value_state.measured_height.max  = lui_inf_length;
        current->value_state.measured_height.flex = 1.0f;
    }
);

// additionaly ensure received height 
// is within node measured limits
LAYOUT_TOP_DOWN_DFS(
    height_distribute, height_distribute, invalidation_flag_only_height_distribute,
    current->value_state.given_height = limit_length(
        current->value_state.given_height,
        current->value_state.measured_height
    );
);

// no additional code
LAYOUT_TOP_DOWN_DFS(
    position, position, invalidation_flag_only_position
);

// Renders widget
// Issues rendering of ui primitives
// Render pass is safe in terms of hashmap pointers invalidation
// Since it refers on the pointer only on enter - after visiting any child it is not used

void render_dfs(
    lui_cache*          cache, 
    int                 previous_width,
    int                 previous_height, 
    const lui_node*     node,
    lla_mat2x3          transform, 
    const void*         instance,
    short               depth_index,
    int                 clipbox_index
) {
    node_stable_index index = {node, instance};

    // get node data
    const lui_node* child = get_node_child(node, instance);
    const void*     data  = get_node_data (node, instance);
    cache_slot*     own   = cache_get_utill(cache, index);
    auxilary_slot*  aux   = auxilary_get_utill(cache, index);

    // mark used, to avoid garbage collect
    own->last_frame_used_in_render = cache->frame_index;
    if (aux) aux->last_frame_used_in_render = cache->frame_index;

    // change instance for subtree
    if (node->type == &lui_instance_type) instance = data;

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
    
    // special nodes (box, text, depth, clipbox)
    if (node->type == &lui_box_type){
        const lui_box_data* bdata = data;
        draw_request_cache_push(cache, (draw_request){
            .transform          = transform,
            .clip_index         = clipbox_index,
            .depth_index        = depth_index,
            .is_box_not_text    = 1,
            .box_data           = *bdata
        });
    }
    else if (node->type == &lui_text_type) {
        const lui_text_data*            tdata = data;
        const text_type_auxilary_state* taux  = aux->state_ptr;

        draw_request_cache_push(cache, (draw_request){
            .transform          = transform,
            .clip_index         = clipbox_index,
            .depth_index        = depth_index,
            .is_box_not_text    = 0,
            .text_node          = index
        });
    }
    else if (node->type == &lui_depth_type) {
        const lui_depth_data* ddata = data;
        depth_index += ddata->depth_change;
    }
    else if (node->type == &lui_clipbox_type) {
        clipbox_index = clipbox_request_cache_push(cache, (clipbox_request){
            .transform = transform
        });
    }

    // back node dimensions to avoid reading own slot after visiting child
    int own_width  = own->value_state.given_width;
    int own_height = own->value_state.given_height;

    // single child
    if (!node->type->array_child && child) {
        render_dfs(cache, own_width, own_height, child, transform, instance, depth_index, clipbox_index);
    }
    // multiple children
    else if (child) for (const lui_node* current_child = child; current_child->type != NULL; current_child++) {
        render_dfs(cache, own_width, own_height, current_child, transform, instance, depth_index, clipbox_index);
    }
}

// Helper for draw requests depth sorting
static inline int helper_draw_requests_greater_depth(const void* av, const void* bv) {
    const draw_request* a = (const draw_request*)av; 
    const draw_request* b = (const draw_request*)bv;
    if (a->depth_index > b->depth_index) return 1;
    return 0;
}

// Main update function
// Calls passes

void lui_update_cache(
    lui_cache*      cache,
    const lui_node* root,
    int             resolution_x,
    int             resolution_y
) {
    // Init state
    cache->resolution_x = resolution_x;
    cache->resolution_y = resolution_y;
    cache->draw_requests_count      = 0;
    cache->text_requests_count      = 0;
    cache->clipbox_requests_count   = 0;

    // Pick next frame index
    cache->frame_index++; if (cache->frame_index < 3) cache->frame_index = 3;

    // Render pass
    render_dfs(cache, cache->resolution_x, cache->resolution_y, root, lla_mat2x3_identity(), NULL, 0, -1);

    // Sort render requests by depth
    stable_sort(cache->draw_requests, cache->draw_requests_count, sizeof(draw_request), helper_draw_requests_greater_depth);

    // Relayout if needed
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
        
        // Perform layout passes
        auxilary_dfs(cache, &walk_order, root_cache, root_auxlr, 0);

        width_measure_dfs(&walk_order, root_cache, root_auxlr, 0);
        width_distribute_dfs(&walk_order, root_cache, root_auxlr, 0);
        height_measure_dfs(&walk_order, root_cache, root_auxlr, 0);
        height_distribute_dfs(&walk_order, root_cache, root_auxlr, 0);

        position_dfs(&walk_order, root_cache, root_auxlr, 0);

        // Could potentialy be cached and used at render
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

#define INTERNAL_TEXTURES_LIMIT                 1024

#define INITIAL_INSTANCES_BUFFER_SIZE           (1024 * sizeof(gpu_instance))
#define INITIAL_DRAW_ITEM_BUFFER_SIZE           (1024 * sizeof(gpu_draw_item))
#define INITIAL_CLIPBOXES_BUFFER_SIZE           (16 * sizeof(gpu_clipbox))
#define INITIAL_GLYPH_BUFFER_SIZE               (2024 * sizeof(gpu_glyph))

#define GLYPH_STRUCTURE_ALIGN                   4

#define PARAMETERS_BUFFER_DESCRIPTOR_BINDING    0
#define INSTANCES_BUFFER_DESCRIPTOR_BINDING     1
#define DRAW_ITEM_BUFFER_DESCRIPTOR_BINDING     2
#define GLYPH_BUFFER_DESCRIPTOR_BINDING         3
#define CLIPBOXES_BUFFER_DESCRIPTOR_BINDING     4
#define SAMPLER_DESCRIPTOR_BINDING              5
#define TEXTURES_ARRAY_DESCRIPTOR_BINDING       6

typedef struct gpu_instance {
    int item;
    int glyph;
} gpu_instance;

typedef struct gpu_draw_item {
    lla_mat2x3  transform;
    lgx_uv_2d   atlas_position;
    int         texture_index;
    int         clipbox_index;
    float       r, g, b, a;
    int         shader;
} gpu_draw_item;

typedef struct gpu_clipbox {
    lla_mat2x3 transform;
} gpu_clipbox;

typedef struct gpu_glyph {
    lgx_uv_2d   atlas_position;
    float       off_x,  off_y;
    float       size_x, size_y;
} gpu_glyph;

typedef struct gpu_parameters {
    int         resolution_x;
    int         resolution_y;
} gpu_parameters;

static inline lgx_buffer* create_ubo(lgx_hardware* hardware, uint64_t bytes) {
    return lgx_create_buffer(hardware, &(lgx_buffer_create_info){
        .size_bytes         = bytes,
        .usage              = lgx_buffer_usage_uniform,
        .memory_strategy    = lgx_memory_allocation_strategy_dedicated,
        .memory_access      = lgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_write
    });
}

static inline lgx_buffer* create_ssbo(lgx_hardware* hardware, uint64_t bytes) {
    return lgx_create_buffer(hardware, &(lgx_buffer_create_info){
        .size_bytes         = bytes,
        .usage              = lgx_buffer_usage_storage,
        .memory_strategy    = lgx_memory_allocation_strategy_dedicated,
        .memory_access      = lgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_write
    });
}

// vec2 position, vec2 uv
static const float quad_vertices_array[] = {
    -1.0f, -1.0f, 0.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
     1.0f,  1.0f, 1.0f, 1.0f,
};

static lgx_vertex_input_attribute_info vertex_attributes[] = {
    {   // position : per vertex
        .binding    = 0,
        .location   = 0,
        .offset     = 0,
        .type       = lgx_data_type_vec2f32
    },
    {   // uv : per vertex
        .binding    = 0,
        .location   = 1,
        .offset     = 2 * 4,
        .type       = lgx_data_type_vec2f32
    }
};

static lgx_vertex_input_binding_info vertex_bindings[] = {
    {
        .binding    = 0,
        .input_rate = lgx_vertex_attribute_input_rate_per_vertex,
        .stride     = 4 * 4
    }
};

static lgx_descriptor_binding descriptor_bindings[] = {
    {   // the parameters buffer
        .binding = PARAMETERS_BUFFER_DESCRIPTOR_BINDING,
        .count   = 1,
        .stages  = lgx_shader_stage_vertex,
        .type    = lgx_descriptor_binding_type_uniform_buffer
    },
    {   // the instances buffer
        .binding = INSTANCES_BUFFER_DESCRIPTOR_BINDING,
        .count   = 1,
        .stages  = lgx_shader_stage_vertex,
        .type    = lgx_descriptor_binding_type_storage_buffer
    },
    {   // the draw items buffer
        .binding = DRAW_ITEM_BUFFER_DESCRIPTOR_BINDING,
        .count   = 1,
        .stages  = lgx_shader_stage_vertex,
        .type    = lgx_descriptor_binding_type_storage_buffer
    },
    {   // the glyphs buffer
        .binding = GLYPH_BUFFER_DESCRIPTOR_BINDING,
        .count   = 1,
        .stages  = lgx_shader_stage_vertex,
        .type    = lgx_descriptor_binding_type_storage_buffer
    },
    {   // the clips buffer
        .binding = CLIPBOXES_BUFFER_DESCRIPTOR_BINDING,
        .count   = 1,
        .stages  = lgx_shader_stage_pixel,
        .type    = lgx_descriptor_binding_type_storage_buffer
    },
    {   // the sampler
        .binding = SAMPLER_DESCRIPTOR_BINDING,
        .count   = 1,
        .stages  = lgx_shader_stage_pixel,
        .type    = lgx_descriptor_binding_type_sampler,
    },
    {   // the textures
        .binding = TEXTURES_ARRAY_DESCRIPTOR_BINDING,
        .count   = -1, // Needs to be set per hardware!
        .stages  = lgx_shader_stage_pixel,
        .type    = lgx_descriptor_binding_type_sampled_texture
    }
};

// ===========================
// Shared Object

struct lui_shared {
    lgx_hardware*                       owning_hardware;

    lgx_buffer*                         vertex_buffer;
    lgx_sampler*                        sampler;

    uint32_t                            descriptor_textures_array_length;
    lgx_descriptor_layout*              descriptor_layout;
    lgx_pipeline_descriptors_layout*    pipeline_descriptor_layout;
    lgx_pipeline*                       pipeline;

    lpr_partitioner*                    glyph_buffer_partitioner;
    lgx_buffer*                         glyph_buffer;
};

lui_shared* lui_create_shared(lgx_hardware* hardware, const lui_shared_create_info* info) {
    lui_shared* shared = calloc(1, sizeof(lui_shared)); if (!shared) return NULL;
    shared->owning_hardware = hardware;

    // Vertex Buffer
    shared->vertex_buffer = lgx_create_buffer(hardware, &(lgx_buffer_create_info){
        .usage              = lgx_buffer_usage_vertex,
        .size_bytes         = sizeof(quad_vertices_array),
        .memory_access      = lgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_write,
        .memory_strategy    = lgx_memory_allocation_strategy_dedicated
    });
    if (!shared->vertex_buffer) goto _fail;
    lgx_buffer_sync_upload(shared->vertex_buffer, 0, quad_vertices_array, sizeof(quad_vertices_array));

    // Query textures limit
    uint32_t max_textures = lgx_hardware_query_limit(hardware, lgx_hardware_limit_max_descriptor_sampled_images);
    shared->descriptor_textures_array_length = max_textures > INTERNAL_TEXTURES_LIMIT ? INTERNAL_TEXTURES_LIMIT : max_textures;

    // Copy descriptor bindings info
    uint32_t bindings_count = sizeof(descriptor_bindings) / sizeof(lgx_descriptor_binding);
    lgx_descriptor_binding* bindings = malloc(bindings_count * sizeof(lgx_descriptor_binding)); 
    if (!bindings) goto _fail; memcpy(bindings, descriptor_bindings, sizeof(descriptor_bindings));

    // Overwrite textures limit
    bindings[TEXTURES_ARRAY_DESCRIPTOR_BINDING].count = shared->descriptor_textures_array_length;

    // Descriptor Layout
    
    shared->descriptor_layout = lgx_create_descriptor_layout(hardware, &(lgx_descriptor_layout_create_info){
        .bindings_count = bindings_count,
        .bindings       = bindings
    }); free(bindings); if (!shared->descriptor_layout) goto _fail;

    // Pipeline Descriptor Layout
    uint32_t layouts_count = 1 + info->additional_pipeline_descriptors_layouts_count;
    lgx_descriptor_layout** layouts = calloc(layouts_count, sizeof(lgx_descriptor_layout*));

    layouts[0] = shared->descriptor_layout;
    for (uint32_t i = 0; i < info->additional_pipeline_descriptors_layouts_count; i++) {
        layouts[i + 1] = info->additional_pipeline_descriptors_layouts[i];
    }

    shared->pipeline_descriptor_layout = lgx_create_pipeline_descriptors_layout(hardware, &(lgx_pipeline_descriptors_layout_create_info){
        .layouts_count  = layouts_count,
        .layouts        = layouts
    }); free(layouts); if (!shared->pipeline_descriptor_layout) goto _fail;

    // Sampler
    shared->sampler = lgx_create_sampler(hardware, &(lgx_sampler_create_info){
        .mag_filter                 = lgx_sampler_filter_linear,
        .min_filter                 = lgx_sampler_filter_linear,
        .mipmap_filter              = lgx_sampler_filter_linear,

        .x_coord_wrapping           = lgx_sampler_wrapping_repeat,
        .y_coord_wrapping           = lgx_sampler_wrapping_repeat,
        .z_coord_wrapping           = lgx_sampler_wrapping_repeat,
        .unnormalized_coordinates   = 0,

        .min_lod                    = 0,
        .max_lod                    = 1,
        .mip_lod_bias               = 0,
    }); if (!shared->sampler) goto _fail;

    // Glyphs buffer
    shared->glyph_buffer = create_ssbo(hardware, INITIAL_GLYPH_BUFFER_SIZE);
    if (!shared->glyph_buffer) goto _fail;

    // Glyph buffer partitioner
    shared->glyph_buffer_partitioner = lpr_create_partitioner(&(lpr_partitioner_create_info){
        .memory_bytes = INITIAL_GLYPH_BUFFER_SIZE,
        .align_bytes  = GLYPH_STRUCTURE_ALIGN
    }); if (!shared->glyph_buffer_partitioner) goto _fail;

    // Pipeline Shaders
    if (!info->pipeline_vertex_shader || !info->pipeline_pixel_shader) goto _fail;

    // Pipeline
    shared->pipeline = lgx_create_pipeline(shared->owning_hardware, &(lgx_pipeline_create_info){
        .render_target_layout       = info->pipeline_render_target_layout,
        .descriptor_layout          = shared->pipeline_descriptor_layout,
        .vertex_layout = {
            .attributes_count       = sizeof(vertex_attributes) / sizeof(lgx_vertex_input_attribute_info),
            .attributes             = vertex_attributes,
            .bindings_count         = sizeof(vertex_bindings) / sizeof(lgx_vertex_input_binding_info),
            .bindings               = vertex_bindings,
        },
        .shader_stages = {
            .vertex                 = info->pipeline_vertex_shader,
            .pixel                  = info->pipeline_pixel_shader
        },
        .input_assembly = {
            .topology               = lgx_primitive_topology_triangle_strip
        },
        .rasterizer = {
            .scissor_enable         = 0,
            .depth_clamp_enable     = 0,
            .fill_mode              = lgx_fill_mode_solid,
            .cull_mode              = lgx_cull_mode_none
        },
        .blend = {
            .blend_enable           = 1,
            .blend_op               = lgx_blend_op_add,
            .src_factor             = lgx_blend_factor_src_alpha,
            .dst_factor             = lgx_blend_factor_one_minus_src_alpha,
        },
        .depth_stencil = {
            .depth_test_enable      = 0,
            .depth_write_enable     = 0,
            .stencil_test_enable    = 0
        }
    }); if (!shared->pipeline) goto _fail;

    return shared;

_fail:
    lui_free_shared(shared);
    return NULL;
}

void lui_free_shared(lui_shared* shared) {
    if (!shared) return;
    lgx_free_buffer(shared->vertex_buffer);
    lgx_free_sampler(shared->sampler);
    lgx_free_pipeline(shared->pipeline);
    lgx_free_pipeline_descriptors_layout(shared->pipeline_descriptor_layout);
    lgx_free_descriptor_layout(shared->descriptor_layout);
    lgx_free_buffer(shared->glyph_buffer);
    lpr_free_partitioner(shared->glyph_buffer_partitioner);
    free(shared);
}

// ===========================
// Frames

typedef struct single_frame {
    uint32_t                    instances_to_render;
    lgx_buffer*                 parameters_buffer;
    lgx_buffer*                 instances_buffer;
    lgx_buffer*                 draw_items_buffer;
    lgx_buffer*                 clipboxes_buffer;
    lgx_descriptor*             descriptor;
    int                         bound_1_recent;
    lgx_texture**               descriptor_bound_textures_1;
    lgx_texture**               descriptor_bound_textures_2;
} single_frame;

struct lui_frames {
    lui_shared*                 owning_shared;
    lgx_descriptor_allocator*   descriptor_allocator;
    uint32_t                    frames_count;
    single_frame*               frames;
};

void frame_descriptor_bind_buffers_and_sampler(lgx_hardware* hardware, lui_shared* shared, single_frame* frame) {
    lgx_descriptor_write_info           writes[6];
    lgx_descriptor_buffer_write_info    binfos[5];
    lgx_descriptor_sampler_write_info   sinfos[1];

    binfos[0] = (lgx_descriptor_buffer_write_info){
        .buffer = frame->parameters_buffer,
        .offset = 0,
        .length = lgx_buffer_get_size_bytes(frame->parameters_buffer)
    };

    binfos[1] = (lgx_descriptor_buffer_write_info){
        .buffer = frame->instances_buffer,
        .offset = 0,
        .length = lgx_buffer_get_size_bytes(frame->instances_buffer)
    };

    binfos[2] = (lgx_descriptor_buffer_write_info){
        .buffer = frame->draw_items_buffer,
        .offset = 0,
        .length = lgx_buffer_get_size_bytes(frame->draw_items_buffer)
    };

    binfos[3] = (lgx_descriptor_buffer_write_info){
        .buffer = shared->glyph_buffer,
        .offset = 0,
        .length = lgx_buffer_get_size_bytes(shared->glyph_buffer)
    };

    binfos[4] = (lgx_descriptor_buffer_write_info){
        .buffer = frame->clipboxes_buffer,
        .offset = 0,
        .length = lgx_buffer_get_size_bytes(frame->clipboxes_buffer)
    };

    sinfos[0] = (lgx_descriptor_sampler_write_info){
        .sampler = shared->sampler
    };

    writes[0] = (lgx_descriptor_write_info){
        .descriptor             = frame->descriptor,
        .binding_type           = lgx_descriptor_binding_type_uniform_buffer,
        .binding_index          = PARAMETERS_BUFFER_DESCRIPTOR_BINDING,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_buffers      = &binfos[0]
    };

    writes[1] = (lgx_descriptor_write_info){
        .descriptor             = frame->descriptor,
        .binding_type           = lgx_descriptor_binding_type_storage_buffer,
        .binding_index          = INSTANCES_BUFFER_DESCRIPTOR_BINDING,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_buffers      = &binfos[1]
    };

    writes[2] = (lgx_descriptor_write_info){
        .descriptor             = frame->descriptor,
        .binding_type           = lgx_descriptor_binding_type_storage_buffer,
        .binding_index          = DRAW_ITEM_BUFFER_DESCRIPTOR_BINDING,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_buffers      = &binfos[2]
    };

    writes[3] = (lgx_descriptor_write_info){
        .descriptor             = frame->descriptor,
        .binding_type           = lgx_descriptor_binding_type_storage_buffer,
        .binding_index          = GLYPH_BUFFER_DESCRIPTOR_BINDING,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_buffers      = &binfos[3]
    };
    
    writes[4] = (lgx_descriptor_write_info){
        .descriptor             = frame->descriptor,
        .binding_type           = lgx_descriptor_binding_type_storage_buffer,
        .binding_index          = CLIPBOXES_BUFFER_DESCRIPTOR_BINDING,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_buffers      = &binfos[4]
    };

    writes[5] = (lgx_descriptor_write_info){
        .descriptor             = frame->descriptor,
        .binding_type           = lgx_descriptor_binding_type_sampler,
        .binding_index          = SAMPLER_DESCRIPTOR_BINDING,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_samplers     = &sinfos[0]
    };
    
    lgx_descriptors_write(hardware, 6, writes);
}

lui_frames* lui_create_frames(lgx_hardware* hardware, const lui_frames_create_info* info) {
    lui_shared* shared = info->shared;

    lui_frames* frames = calloc(1, sizeof(lui_frames)); 
    if (!frames) return NULL;
    
    frames->owning_shared = shared;
    
    // create descriptor allocator
    frames->descriptor_allocator = lgx_create_descriptor_allocator(hardware, &(lgx_descriptor_allocator_create_info){
        .descriptor_layout          = shared->descriptor_layout,
        .max_descriptors_allocated  = info->frames_in_flight_count
    }); if (!frames->descriptor_allocator) goto _fail;

    // create frames
    frames->frames_count = info->frames_in_flight_count;
    frames->frames = calloc(info->frames_in_flight_count, sizeof(single_frame));
    if (!frames->frames) goto _fail;

    // populate frames
    for (uint32_t i = 0; i < info->frames_in_flight_count; i++) {
        single_frame* frame = &frames->frames[i];
        *frame = (single_frame){
            .descriptor                  = lgx_descriptor_allocator_alloc_descriptor(frames->descriptor_allocator),
            .parameters_buffer           = create_ubo (hardware, sizeof(gpu_parameters)),
            .instances_buffer            = create_ssbo(hardware, INITIAL_INSTANCES_BUFFER_SIZE),
            .draw_items_buffer           = create_ssbo(hardware, INITIAL_DRAW_ITEM_BUFFER_SIZE),
            .clipboxes_buffer            = create_ssbo(hardware, INITIAL_CLIPBOXES_BUFFER_SIZE),
            .descriptor_bound_textures_1 = calloc(shared->descriptor_textures_array_length, sizeof(lgx_texture*)),
            .descriptor_bound_textures_2 = calloc(shared->descriptor_textures_array_length, sizeof(lgx_texture*))
        };

        if (!frame->descriptor || !frame->parameters_buffer || !frame->instances_buffer || !frame->draw_items_buffer || 
            !frame->clipboxes_buffer || !frame->descriptor_bound_textures_1 || !frame->descriptor_bound_textures_2) goto _fail;
        frame_descriptor_bind_buffers_and_sampler(hardware, shared, frame);
    }

    return frames;

_fail:
    lui_free_frames(frames);
    return NULL;
}

void lui_free_frames(lui_frames* frames) {
    if (!frames) return;

    for (uint32_t i = 0; i < frames->frames_count; i++) {
        single_frame* frame = &frames->frames[i];
        lgx_free_buffer(frame->parameters_buffer);
        lgx_free_buffer(frame->instances_buffer);
        lgx_free_buffer(frame->draw_items_buffer);
        lgx_free_buffer(frame->clipboxes_buffer);
        free(frame->descriptor_bound_textures_1);
        free(frame->descriptor_bound_textures_2);
    }

    // all per-frame descriptors freed with allocator
    lgx_free_descriptor_allocator(frames->descriptor_allocator);
    free(frames->frames);

    free(frames);
}

// ===========================
// Rendering Functions

typedef struct texture_writes_dynamic_array {
    lgx_descriptor*                             descriptor;
    size_t                                      capacity;
    size_t                                      position;
    lgx_descriptor_write_info*                  writes;
    lgx_descriptor_sampled_texture_write_info*  infos;
} texture_writes_dynamic_array;

static void free_texture_writes_dynamic_array(texture_writes_dynamic_array writes) {
    free(writes.writes); free(writes.infos);
}

static void push_texture_writes_dynamic_array(texture_writes_dynamic_array* writes, lgx_texture* texture, uint32_t slot) {
    if (writes->position + 1 > writes->capacity) {
        size_t new_capacity = writes->capacity ? writes->capacity * 2 : 16;

        lgx_descriptor_write_info* new_writes 
            = realloc(writes->writes, new_capacity * sizeof(lgx_descriptor_write_info));

        lgx_descriptor_sampled_texture_write_info* new_infos 
            = realloc(writes->infos,  new_capacity * sizeof(lgx_descriptor_sampled_texture_write_info));

        if (!new_writes || !new_infos) {
            free(new_writes); free(new_infos);
            return; // do not store write, no storage
        }

        writes->capacity = new_capacity;
        writes->writes   = new_writes;
        writes->infos    = new_infos;
    }

    writes->writes[writes->position] = (lgx_descriptor_write_info){
        .descriptor                 = writes->descriptor,
        .binding_type               = lgx_descriptor_binding_type_sampled_texture,
        .binding_index              = TEXTURES_ARRAY_DESCRIPTOR_BINDING,
        .array_element_index        = slot,
        .array_elements_count       = 1,
        // cannot link info now, since infos can be reallocated
    };

    writes->infos[writes->position] = (lgx_descriptor_sampled_texture_write_info){
        .sampled_texture = texture
    };

    writes->position++;
}

static int clear_write
(uint32_t texture_slots_count, single_frame* frame) {
    lgx_texture** write  = frame->bound_1_recent ? frame->descriptor_bound_textures_2 : frame->descriptor_bound_textures_1;
    for (uint32_t i = 0; i < texture_slots_count; i++) write[i] = NULL;
}

static int push_texture
(texture_writes_dynamic_array* writes, uint32_t texture_slots_count, single_frame* frame, lgx_texture* texture, int is_font) {
    if (texture == NULL) return 0;

    lgx_texture** recent = frame->bound_1_recent ? frame->descriptor_bound_textures_1 : frame->descriptor_bound_textures_2;
    lgx_texture** write  = frame->bound_1_recent ? frame->descriptor_bound_textures_2 : frame->descriptor_bound_textures_1;

    // start search at module of texture pointer,
    // bit shift because pointers may be aligned, will often lead to same slot
    uint64_t hash = ((size_t)texture >> 4) * 11400714819323198485llu;
    uint64_t begin = hash % texture_slots_count;

    // search for free slot
    int itr = begin;
    do {
        // same texture already assigned in previous frame generation,
        // or write alredy requested -> return
        if (recent[itr] == texture || write[itr] == texture) {
            write[itr] = texture; // ensure space reserved
            if (is_font) return -itr - 1;
            return itr + 1;
        }
        // free slot, assing
        if (write[itr] == NULL) {
            write[itr] = texture;
            push_texture_writes_dynamic_array(writes, texture, (uint32_t)itr);
            if (is_font) return -itr - 1;
            return itr + 1;
        }
        // else continue search
        itr = (itr + 1) % texture_slots_count;
    } while(itr != begin);

    // no empty slots left
    return 0;
}

void lui_upload_cache(
    lui_cache*          cache,
    lui_shared*         shared,
    lui_frames*         frames,
    uint32_t            frame_idx,
    lgx_command_list*   command_list,
    lgx_hardware_queue* queue_for_uploads,
    lgx_staging_memory* staging_memory,
    uint64_t            staging_memory_region_offset,
    uint64_t            staging_memory_region_size,
    lgx_cpu_signal*     upload_finished_cpu,
    lgx_gpu_signal*     upload_finished_gpu
) {
    lgx_hardware* hardware = shared->owning_hardware;
    single_frame* frame    = &frames->frames[frame_idx];

    // Prepare render parameters uniform

    gpu_parameters parameters = {
        .resolution_x = cache->resolution_x,
        .resolution_y = cache->resolution_y
    };

    // Prepare texture descriptor update structure

    texture_writes_dynamic_array texture_writes_array = (texture_writes_dynamic_array){
        .descriptor = frame->descriptor
    };
    clear_write(shared->descriptor_textures_array_length, frame);

    // Allocate upload regions descriptors array

    size_t upload_regions_count    = cache->text_requests_count + 4;
    size_t upload_regions_position = 0;
    lgx_buffer_multi_upload_region* upload_regions = malloc(upload_regions_count * sizeof(lgx_buffer_multi_upload_region));

    // Prepare partitions for text draws

    for (size_t i = 0; i < cache->text_requests_count; i++) {
        text_request              req  = cache->text_requests[i];
        text_type_auxilary_state* aux = auxilary_get_utill(cache, req.owning_node)->state_ptr;

        aux->partitioner = shared->glyph_buffer_partitioner;

        // always free owned partition to reduce fragmentation
        if (aux->owned_glyph_buffer_partition) {
            lpr_partitioner_free_partition(shared->glyph_buffer_partitioner, aux->owned_glyph_buffer_partition);
            aux->owned_glyph_buffer_partition = NULL;
        }

        // new text is empty - creation of 0 bytes partition is forbidden
        if (!req.glyphs_count) continue;

        // request new partition
        aux->owned_glyph_buffer_partition = lpr_partitioner_alloc_partition(
            shared->glyph_buffer_partitioner,
            req.glyphs_count * sizeof(gpu_glyph)
        );

        // failed to create partition - create bigger text buffer
        if (!aux->owned_glyph_buffer_partition) {
            // todo
        }
    }

    // Generate draw regions for texts

    for (size_t i = 0; i < cache->text_requests_count; i++) {
        text_request              req  = cache->text_requests[i];
        text_type_auxilary_state* aux = auxilary_get_utill(cache, req.owning_node)->state_ptr;
        lpr_partition*            prt = aux->owned_glyph_buffer_partition;
        if (!prt) continue; // text empty, nothing to upload

        upload_regions[upload_regions_position++] = (lgx_buffer_multi_upload_region){
            .buffer         = shared->glyph_buffer,
            .buffer_offset  = lpr_partition_query_offset(prt),
            .source_data    = req.glyphs,
            .source_bytes   = req.glyphs_count * sizeof(gpu_draw_item)
        };
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
    items = malloc(items_bytes);
    for (uint32_t i = 0; i < items_count; i++) {
        draw_request req = cache->draw_requests[i];

        if (req.is_box_not_text) {
            int texture_index = 0; lgx_uv_2d uv;
            if (req.box_data.image) {
                lgx_texture* texture; if (lui_injection_query_image(req.box_data.image, &texture, &uv)) {
                    texture_index = push_texture(
                        &texture_writes_array, shared->descriptor_textures_array_length, 
                        frame, texture, 0
                    );
                }
            }

            items[i] = (gpu_draw_item){
                .transform      = req.transform,
                .clipbox_index  = req.clip_index,
                .texture_index  = texture_index,
                .atlas_position = uv,
                .r              = (float)req.box_data.tint.r / 255.0f,
                .g              = (float)req.box_data.tint.g / 255.0f,
                .b              = (float)req.box_data.tint.b / 255.0f,
                .a              = (float)req.box_data.tint.a / 255.0f,
                .shader         = req.box_data.shader
            };

            instances_count += 1;  // single box
        }
        else {
            auxilary_slot*            slot = auxilary_get_utill(cache, req.text_node);
            text_type_auxilary_state* aux  = slot->state_ptr;
            lpr_partition*            part = aux->owned_glyph_buffer_partition;
            if (!part) continue;

            lui_text_data text_data = *(const lui_text_data*)get_node_data(slot->key.node, slot->key.instance);
            lfont* font; if (!lui_injection_query_font(text_data.font, &font)) continue;

            int texture_index = push_texture(
                &texture_writes_array, shared->descriptor_textures_array_length, 
                frame, lfont_get_texture(font), 1
            );

            items[i] = (gpu_draw_item){
                .transform      = req.transform,
                .clipbox_index  = req.clip_index,
                .texture_index  = texture_index,
                .atlas_position = (lgx_uv_2d){0, 0, 1, 1},
                .r              = (float)text_data.tint.r / 255.0f,
                .g              = (float)text_data.tint.g / 255.0f,
                .b              = (float)text_data.tint.b / 255.0f,
                .a              = (float)text_data.tint.a / 255.0f,
                .shader         = text_data.shader
            };

            instances_count += lpr_partition_query_size(part) / sizeof(gpu_glyph);
        }
    }

    // Generate GPU Instances
    instances_bytes = instances_count * sizeof(gpu_instance);
    instances = malloc(instances_bytes);
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
            lpr_partition*            part = aux->owned_glyph_buffer_partition;
            if (!part) continue;
            
            size_t first  = lpr_partition_query_offset(part) / sizeof(gpu_glyph);
            size_t glyphs = lpr_partition_query_size(part) / sizeof(gpu_glyph);
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
    clipboxes       = malloc(clipboxes_bytes);
    for (uint32_t i = 0; i < clipboxes_count; i++) {
        clipbox_request req = cache->clipbox_requests[i];
        clipboxes[i] = (gpu_clipbox){
            .transform = req.transform
        };
    }

    // Ensure Buffer Sizes
    int rebind = 0;

    // Items buffer
    if (lgx_buffer_get_size_bytes(frame->draw_items_buffer) < items_bytes) {
        lgx_free_buffer(frame->draw_items_buffer);
        frame->draw_items_buffer = create_ssbo(hardware, items_bytes);
        rebind = 1;
    }

    // Instanced buffer
    if (lgx_buffer_get_size_bytes(frame->instances_buffer) < instances_bytes) {
        lgx_free_buffer(frame->instances_buffer);
        frame->instances_buffer = create_ssbo(hardware, instances_bytes);
        rebind = 1;
    }

    // Clipboxes buffer
    if (lgx_buffer_get_size_bytes(frame->clipboxes_buffer) < clipboxes_bytes) {
        lgx_free_buffer(frame->clipboxes_buffer);
        frame->clipboxes_buffer = create_ssbo(hardware, clipboxes_bytes);
        rebind = 1;
    }

    // If resize failed return
    if (!frame->draw_items_buffer || !frame->instances_buffer) return;
    if (rebind) frame_descriptor_bind_buffers_and_sampler(hardware, frames->owning_shared, frame);

    // Upload

    upload_regions[upload_regions_position++] = (lgx_buffer_multi_upload_region){
        .buffer         = frame->parameters_buffer,
        .buffer_offset  = 0,
        .source_data    = &parameters,
        .source_bytes   = sizeof(gpu_parameters)
    };

    upload_regions[upload_regions_position++] = (lgx_buffer_multi_upload_region){
        .buffer         = frame->draw_items_buffer,
        .buffer_offset  = 0,
        .source_data    = items,
        .source_bytes   = items_count * sizeof(gpu_draw_item)
    };

    upload_regions[upload_regions_position++] = (lgx_buffer_multi_upload_region){
        .buffer         = frame->clipboxes_buffer,
        .buffer_offset  = 0,
        .source_data    = clipboxes,
        .source_bytes   = clipboxes_count * sizeof(gpu_clipbox)
    };

    upload_regions[upload_regions_position++] = (lgx_buffer_multi_upload_region){
        .buffer         = frame->instances_buffer,
        .buffer_offset  = 0,
        .source_data    = instances,
        .source_bytes   = instances_count * sizeof(gpu_instance)
    };

    // Write all buffers
    lgx_buffer_multi_upload(
        upload_regions,
        upload_regions_position,
        command_list, 
        queue_for_uploads,
        staging_memory,
        staging_memory_region_offset,
        staging_memory_region_size,
        upload_finished_cpu,
        upload_finished_gpu
    );

    // Walk and link texture descriptor writes
    // Do it here, since earlier those could have been reallocated
    for (size_t i = 0; i < texture_writes_array.position; i++) {
        texture_writes_array.writes[i].infos.for_sampled_textures = &texture_writes_array.infos[i];
    }

    // Update textures descriptor    
    lgx_descriptors_write(
        hardware, texture_writes_array.position, texture_writes_array.writes
    );

    // Mark to render
    frame->instances_to_render = instances_count;

    // Free text requests
    free_cached_text_requests(cache);

    // Free allocated memory
    free(items); free(clipboxes); free(instances); free(upload_regions);
    free_texture_writes_dynamic_array(texture_writes_array);

    // Toggle recent bound in frame
    frame->bound_1_recent = !frame->bound_1_recent;
}

void lui_gcmd_render(
    lgx_command_list*   target,
    lui_frames*         frames,
    uint32_t            frame_idx
) {
    single_frame* frame = &frames->frames[frame_idx % frames->frames_count];
    if (frame->instances_to_render) {
        lgx_gcmd_bind_graphics_pipeline(target, frames->owning_shared->pipeline);
        lgx_gcmd_bind_graphics_pipeline_descriptors(
            target, 
            frames->owning_shared->pipeline_descriptor_layout, 
            0, 1, &frame->descriptor
        );
        lgx_gcmd_bind_graphics_pipeline_vertex_buffer(target, frames->owning_shared->vertex_buffer, 0, 0);
        lgx_gcmd_draw_vertices(target, 4, 0, frame->instances_to_render, 0);
    }
}

// ===========================
// Text layout generation

void create_text_request(lui_cache* cache, cache_slot* slot, text_type_auxilary_state* aux) {
    const lui_text_data* tdata = get_node_data(slot->key.node, slot->key.instance);
    lfont* font; if (!lui_injection_query_font(tdata->font, &font)) return;
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
        uint32_t cp; i += lfont_utf8_decode(text, i, &cp);
        if (cp != '\n') glyph_count++; 
        else extra_lines_count++;
    }

    // Allocate glyphs buffer
    gpu_glyph* glyphs = glyph_count ? malloc(sizeof(gpu_glyph) * glyph_count) : NULL;
    if (glyph_count && !glyphs) {
        text_request req = { .owning_node = slot->key, .glyphs_count = 0, .glyphs = NULL };
        text_request_cache_push(cache, req);
        return;
    }

    // Find font scale
    const float font_scale = tdata->size / lfont_get_base_size(font);

    // Populate glyphs buffer
    const float ascent      = lfont_get_base_ascent(font)   * font_scale;
    const float descent     = lfont_get_base_descent(font)  * font_scale;
    const float line_gap    = lfont_get_base_line_gap(font) * font_scale;
    const float line_height = ascent - descent + line_gap;

    float    pen_x      = 0.0f;
    float    pen_y      = 0.0f;
    float    text_width = 0.0f; // max line width across all lines
    size_t   glyph_idx  = 0;
    uint32_t prev_cp    = 0;    // for kerning; 0 = no previous glyph

    for (size_t itr = 0; text[itr] != '\0';) {
        uint32_t cp;
        itr += lfont_utf8_decode(text, itr, &cp);

        // Handle newline
        if (cp == '\n') {
            if (pen_x > text_width) text_width = pen_x;
            pen_x   = 0.0f;
            pen_y  -= line_height;
            prev_cp = 0; // reset kerning across lines
            continue;
        }

        // Kerning between consecutive glyphs on the same line
        if (prev_cp) pen_x += lfont_get_kerning(font, prev_cp, cp);

        // Write glyph
        const lfont_glyph g = lfont_get_glyph(font, cp);
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
    float text_height = -pen_y + (ascent - descent);

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

#endif // LIGHT_USER_INTERFACE_IMPL
