#ifndef LIGHT_USER_INTERFACE_H
#define LIGHT_USER_INTERFACE_H

#include "/home/kacper/Projects/LightFramework/include/light/graphics.h"
#include "/home/kacper/Projects/LightFramework/include/light/linear_algebra.h"
#include "/home/kacper/Projects/LightFramework/include/light/font.h"

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

typedef void(*lui_node_layout_func)(
    const void*             node_data,          // node data
    lui_node_layout_state*  node_state,         // node own state
    size_t                  children_count,     // node children count
    lui_node_layout_state** children_states     // node children states
);

typedef void(*lui_node_render_func)(
    lui_cache*              cache,              // cache to cache render requests
    const void*             node_data,          // node data
    lla_mat2x3*             transform           // given transform, can be changed
);

typedef struct lui_type {
    // Whether child pointer in node means single node
    // Or and array terminated with LUI_ARRAY_END
    int array_child;

    // First layout stage
    // Generates desired nodes widths, bottom-up
    // If left NULL: width = (min = max(children mins), max = max(children max), flex = 1.0f if min != max, else 0)
    // IN:  [children measured width]
    // OUT: [own measured width]
    lui_node_layout_func    width_measure;

    // Second layout stage
    // Generates actuall nodes widths, top-down
    // If left NULL: children width = parent width, with applied maxes
    // IN:  [width measurements, own given width]
    // OUT: [children given width]
    lui_node_layout_func    width_distribute;

    // Third layout stage
    // Generates desired nodes widths, bottom-up
    // If left NULL: height = (min = max(children mins), max = max(children max), flex = 1.0f if min != max, else 0)
    // IN:  [given widths, children measured heights]
    // OUT: [own measured height]
    lui_node_layout_func    height_measure;

    // Fourth layout stage
    // Generates actuall nodes heights, top-down
    // If left NULL: children height = parent height, with applied maxes
    // IN:  [given widths, measured heights, own given height]
    // OUT  [children given heights]
    lui_node_layout_func    height_distribute;

    // Fifth layout stage
    // Position nodes on screen, top-down
    // If left NULL: childrens are centered in parent
    // IN:  [all widths and heights]
    // OUT: [node offset from ]
    lui_node_layout_func    position;

    // Render functions
    // This pass renders boxes and texts in implementation
    // Allows user to alter children transforms
    // If left NULL: transform passed to children untouched
    // IN:  [complete layout states, own render transform]
    // OUT: [children render transform]
    lui_node_render_func    render;
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
// Predefinied Types

extern const lui_type lui_box_type;
typedef struct lui_box_data {
    lui_color       tint;               // box color
    const char*     image;              // image name/path, may be NULL
    uint32_t        shader;             // shader effect index
} lui_box_data;

extern const lui_type lui_text_type;
typedef struct lui_text_data {
    unsigned int    size;               // font size
    const char*     font;               // font name/path
    const char*     text;               // text pointer
    lui_color       tint;               // text color modyficator
    uint32_t        shader;             // shader effect index
} lui_text_data;

extern const lui_type lui_row_type;
typedef struct lui_row_data {
    float           horizontal_align;   // 0 - align left, 0.5 - align center, 1.0 - align right,  other values also work
    float           vertical_align;     // 0 - align top,  0.5 - align center, 1.0 - align bottom, other values also work
    lui_length      spacing;            // spacing between childrens
} lui_row_data;

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

#include <stdlib.h>

/*
    HELPERS PART
*/

// ===========================
// Math helpers

static inline int min_int(int a, int b) { return a < b ? a : b; }
static inline int max_int(int a, int b) { return a < b ? b : a; }

static inline int clamp_length_in_desire(int length, lui_length limits) {
    if (length > limits.max) length = limits.max;
    if (length < limits.min) length = limits.min;
    return length;
}

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

/*
    CACHE AND PASSES PART
*/

// ===========================
// Cache Object

typedef struct cache_slot cache_slot;
typedef struct draw_request draw_request;

struct lui_cache {
    int             walk_current_resolution_x;  // constant through all passes
    int             walk_current_resolution_y;  // constant through all passes
    const void*     walk_current_instance;      // tracked in every pass
    int             walk_current_depth;         // tracked in render pass
    unsigned char   walk_current_frame_index;   // tracked in render pass

    size_t          cache_capacity;
    size_t          cache_fill;
    cache_slot*     cache_slots;
    
    size_t          draw_request_capacity;
    size_t          draw_requests_count;
    draw_request*   draw_requests;
};

lui_cache* lui_create_cache() {
    lui_cache* cache = calloc(1, sizeof(lui_cache));
    return cache;
}

void lui_free_cache(lui_cache* cache) {
    if (!cache) return;
    free(cache->draw_requests);
    free(cache->cache_slots);
    free(cache);
}

typedef struct node_stable_index {
    const lui_node* node;
    const void*     instance;
} node_stable_index;

typedef struct cache_slot {
    node_stable_index       key;
    size_t                  value_child_count;
    lui_node_layout_state   value_state;

    // 0     - empty cell
    // 1     - just added, not rendered yet
    // 2-255 - rendered
    unsigned char last_frame_used_in_render;  
} cache_slot;

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

static cache_slot* cache_get_or_insert(lui_cache* cache, node_stable_index key);
static void cache_grow(lui_cache* cache) {
    size_t      old_cap   = cache->cache_capacity;
    cache_slot* old_slots = cache->cache_slots;

    size_t new_cap = old_cap ? old_cap * 2 : 64;

    cache->cache_slots = calloc(new_cap, sizeof(*cache->cache_slots));
    cache->cache_capacity = new_cap;
    cache->cache_fill = 0;

    for (size_t i = 0; i < old_cap; ++i) {
        if (!old_slots[i].last_frame_used_in_render) continue;
        cache_slot* dst = cache_get_or_insert(cache, old_slots[i].key);
        *dst = old_slots[i];
    }

    free(old_slots);
}

// gets or creates new cache entry for node
static cache_slot* cache_get_or_insert(lui_cache* cache, node_stable_index key) {
    if ((cache->cache_fill + 1) * 10 >= cache->cache_capacity * 7) cache_grow(cache);

    size_t mask = cache->cache_capacity - 1;
    size_t idx = hash_key(key) & mask;

    for (;;) {
        cache_slot* slot = &cache->cache_slots[idx];

        if (!slot->last_frame_used_in_render) {
            *slot = (cache_slot){
                .key                        = key,
                .last_frame_used_in_render  = 1
            };

            ++cache->cache_fill;
            return slot;
        }

        if (slot->key.node == key.node && slot->key.instance == key.instance) return slot;
        idx = (idx + 1) & mask;
    }
}

struct draw_request {
    lla_mat2x3      transform;
    int             texture_index;
    lgx_uv_2d       texture_atlas;
    int             clip_index;
    int             depth_index;
    unsigned char   r, g, b, a;
    int             shader;
    int             glyph_first;
    int             glyph_count;
};

static inline int helper_draw_requests_greater_depth(const void* av, const void* bv) {
    const draw_request* a = (const draw_request*)av; 
    const draw_request* b = (const draw_request*)bv;
    if (a->depth_index > b->depth_index) return 1;
    return 0;
}

static void cache_push_draw_request(lui_cache* cache, draw_request req) {
    if (cache->draw_requests_count + 1 > cache->draw_request_capacity) {
        size_t          new_cap = cache->draw_request_capacity ? cache->draw_request_capacity * 2 : 64;
        draw_request*   new_req = realloc(cache->draw_requests, new_cap * sizeof(draw_request));
        if (!new_req)   return; // failed to resize

        cache->draw_requests         = new_req;
        cache->draw_request_capacity = new_cap;
    }

    cache->draw_requests[cache->draw_requests_count++] = req;
}

// ===========================
// Cache Update

// Cache Walk Pass
// Called on remeasure
// Computes: 
//  - walk order (the order caches are visited, to avoid hashmaping multiple times)
//  - nodes children count (simplify implementations)

typedef struct caches_walk_order {
    size_t                  capacity;   // in cache_slot pointers
    size_t                  position;   // in cache_slot pointers
    cache_slot**            slots;      // sized capacity
    lui_node_layout_state** states;     // sized capacity
} caches_walk_order;

// returns non-zero at success
static inline int caches_walk_order_push(caches_walk_order* walk_order, cache_slot* slot) {
    if (walk_order->position + 1 >= walk_order->capacity) {
        size_t new_cap = walk_order->capacity ? walk_order->capacity * 2 : 64;
    
        cache_slot**            new_slt = realloc(walk_order->slots,  walk_order->capacity * sizeof(cache_slot*));
        lui_node_layout_state** new_sts = realloc(walk_order->states, walk_order->capacity * sizeof(lui_node_layout_state*));

        if (!new_slt || !new_sts) return 0; // failed to realloc -> failed to ensure space

        walk_order->capacity = new_cap;
        walk_order->slots    = new_slt;
        walk_order->states   = new_sts;
    }

    walk_order->slots[walk_order->position]  = slot;
    walk_order->states[walk_order->position] = &slot->value_state;
    walk_order->position++;

    return 1; // success
}

// pushes all child nodes caches of node to caches_walk_order
// recurse into children left to right
// returns non-zero at success
int caches_walk_dfs(lui_cache* cache, cache_slot* current, caches_walk_order* walk_order, void* instance) {
    const lui_node* node  = current->key.node;
    const lui_node* child = get_node_child(current->key.node, current->key.instance);
    size_t          count = 0;
    int             scc   = 1;

    if (!node->type->array_child && child) {
        cache_slot* child_slot = cache_get_or_insert(cache, (node_stable_index){child, instance});
        scc &= caches_walk_order_push(walk_order, child_slot); count++;
    }
    else if (child) for (const lui_node* cc = child; cc->type != NULL; cc++) {
        cache_slot* child_slot = cache_get_or_insert(cache, (node_stable_index){cc, instance});
        scc &= caches_walk_order_push(walk_order, child_slot); count++;
    }

    // recurse
    size_t begin_pos = walk_order->position - count;
    for (size_t i = 0; i < count; i++) {
        scc &= caches_walk_dfs(cache, walk_order->slots[begin_pos], walk_order, instance);
    }

    current->value_child_count = count;
    return scc;
}

void free_caches_walk_order(caches_walk_order* order) {
    free(order->slots);
}

static inline lui_node_layout_func get_offseted_func_out_of_type(const lui_type* type, size_t type_function_member_offset) {
    return *(lui_node_layout_func*)(((char*)type) + type_function_member_offset);
}

// returns last visited node index
size_t bottom_up_layout_dfs(
    caches_walk_order*      walk_order, 
    cache_slot*             current, 
    size_t                  first_child, 
    size_t                  type_function_member_offset,
    lui_node_layout_func    default_layout
) {
    cache_slot** children        = &walk_order->slots[first_child];
    size_t       last_descendant = first_child + current->value_child_count;
    const void*  data            = get_node_data(current->key.node, current->key.instance);

    // recurse
    for (size_t i = 0; i < current->value_child_count; i++) last_descendant = bottom_up_layout_dfs(
        walk_order, children[i], last_descendant, type_function_member_offset, default_layout
    );

    // do call
    lui_node_layout_func func = get_offseted_func_out_of_type(current->key.node->type, type_function_member_offset);
    if (func == NULL) func = default_layout;
    func(data, &current->value_state, current->value_child_count, &walk_order->states[first_child]);
    
    return last_descendant;
}

size_t top_down_layout_dfs(
    caches_walk_order*      walk_order, 
    cache_slot*             current, 
    size_t                  first_child, 
    size_t                  type_function_member_offset,
    lui_node_layout_func    default_layout
) {
    cache_slot** children        = &walk_order->slots[first_child];
    size_t       last_descendant = first_child + current->value_child_count;
    const void*  data            = get_node_data(current->key.node, current->key.instance);

    // do call
    lui_node_layout_func func = get_offseted_func_out_of_type(current->key.node->type, type_function_member_offset);
    if (func == NULL) func = default_layout;
    func(data, &current->value_state, current->value_child_count, &walk_order->states[first_child]);

    // recurse
    for (size_t i = 0; i < current->value_child_count; i++) last_descendant = bottom_up_layout_dfs(
        walk_order, children[i], last_descendant, type_function_member_offset, default_layout
    );

    return last_descendant;
}

// Renders widget
void render_dfs(lui_cache* cache, const cache_slot* previous, const lui_node* node, lla_mat2x3 transform) {
    // get node data
    const lui_node* child = get_node_child(node, cache->walk_current_instance);
    const void*     data  = get_node_data (node, cache->walk_current_instance);
    cache_slot*     own   = cache_get_or_insert(cache, (node_stable_index){node, cache->walk_current_instance});
    own->last_frame_used_in_render = cache->walk_current_frame_index;   // mark used, no to garbage collect

    // change transform based on node's position and scale
    if (previous) {
        int offset_right = previous->value_state.hori_offset;
        int offset_top   = previous->value_state.vert_offset;

        float off_x   = ((float)offset_right) / cache->walk_current_resolution_x;
        float off_y   = ((float)offset_top)   / cache->walk_current_resolution_y;
        float scale_x = ((float)own->value_state.given_width)  / previous->value_state.given_width;
        float scale_y = ((float)own->value_state.given_height) / previous->value_state.given_height;

        transform = lla_mat2x3_mul(transform, lla_mat2x3_scaling(scale_x, scale_y));
    }

    // do render if method provided
    if (node->type->render) node->type->render(cache, data, &transform);

    // single child
    if (!node->type->array_child && child) render_dfs(cache, own, node, transform);
    // multiple children
    else if (child) for (const lui_node* current_child = child; current_child->type != NULL; current_child++) {
        render_dfs(cache, own, current_child, transform); current_child++;
    }
}

/*
    Garbage collect on text:
    - every frame rewrite occupied regions descripotr
    - if occupiacny not copied inherently free
*/

/*
    Measure passes at exit shall apply flags
    Distribute passes shall apply measurements limits at entry
*/

// Default methods forwards

void default_width_measure
(const void* node_data, lui_node_layout_state* node_state, size_t children_count, lui_node_layout_state** children_states);

void default_width_distribute
(const void* node_data, lui_node_layout_state* node_state, size_t children_count, lui_node_layout_state** children_states);

void default_height_measure
(const void* node_data, lui_node_layout_state* node_state, size_t children_count, lui_node_layout_state** children_states);

void default_height_distribute
(const void* node_data, lui_node_layout_state* node_state, size_t children_count, lui_node_layout_state** children_states);

void default_position
(const void* node_data, lui_node_layout_state* node_state, size_t children_count, lui_node_layout_state** children_states);

void lui_update_cache(
    lui_cache*      cache,
    const lui_node* root,
    int             resolution_x,
    int             resolution_y
) {
    cache->walk_current_resolution_x = resolution_x;
    cache->walk_current_resolution_y = resolution_y;

    // All layout passes
    if (1) {
        cache_slot* root_cache = cache_get_or_insert(cache, (node_stable_index){root, NULL});

        // Give root entire screen
        // Will auto bound to desired at distribute
        root_cache->value_state.given_width  = resolution_x;
        root_cache->value_state.given_height = resolution_y;

        // Find walk order
        caches_walk_order walk_order = {0};
        if (!caches_walk_dfs(cache, root_cache, &walk_order, NULL)) {
            free_caches_walk_order(&walk_order);
            return;
        }
        
        // Perform layout passes
        bottom_up_layout_dfs(&walk_order, root_cache, 0, offsetof(lui_type, width_measure),     default_width_measure);
        top_down_layout_dfs (&walk_order, root_cache, 0, offsetof(lui_type, width_distribute),  default_width_distribute);
        bottom_up_layout_dfs(&walk_order, root_cache, 0, offsetof(lui_type, height_measure),    default_height_measure);
        top_down_layout_dfs (&walk_order, root_cache, 0, offsetof(lui_type, height_distribute), default_height_distribute);
        top_down_layout_dfs (&walk_order, root_cache, 0, offsetof(lui_type, position),          default_position);

        // Could potentialy be cached and used at render
        free_caches_walk_order(&walk_order);
    }

    // Pick next frame index
    cache->walk_current_frame_index++; if (cache->walk_current_frame_index < 2) cache->walk_current_frame_index = 2;

    // Render pass
    cache->draw_requests_count = 0;
    render_dfs(cache, NULL, root, lla_mat2x3_identity());

    // Sort render requests by depth
    stable_sort(cache->draw_requests, cache->draw_requests_count, sizeof(draw_request), helper_draw_requests_greater_depth);

    // Garbage collect dead cache entries
    // If entry was not used in render, mark it free
    // Do every 16 frames not to spend to much time on it
    if (cache->walk_current_frame_index % 16 == 0) {
        for (size_t i = 0; i < cache->cache_capacity; i++) {
            cache_slot* slot = &cache->cache_slots[i];
            if (slot->last_frame_used_in_render != cache->walk_current_frame_index) {
                slot->last_frame_used_in_render = 0; // mark free
            }
        }
    }
}

// ===========================
// Default Methods

void default_width_measure(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states
) {
    (void)node_data; lui_length own = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        lui_length child = children_states[i]->measured_width;
        own.min  = max_int(own.min, child.min);
        own.max  = max_int(own.max, child.max);
    }

    if (own.min != own.max) own.flex = 1.0f;
    node_state->measured_width = own;
}

void default_width_distribute(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states
) {
    (void)node_data;

    for (size_t i = 0; i < children_count; ++i) {
        children_states[i]->given_width = clamp_length_in_desire(node_state->given_width, children_states[i]->measured_width);
    }
}

void default_height_measure(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states
) {
    (void)node_data; lui_length own = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        lui_length child = children_states[i]->measured_height;
        own.min  = max_int(own.min, child.min);
        own.max  = max_int(own.max, child.max);
    }

    if (own.min != own.max) own.flex = 1.0f;
    node_state->measured_height = own;
}

void default_height_distribute(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states
) {
    (void)node_data;

    for (size_t i = 0; i < children_count; ++i) {
        children_states[i]->given_height = clamp_length_in_desire(node_state->given_height, children_states[i]->measured_height);
    }
}

void default_position(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states
) {
    (void)node_data;

    for (size_t i = 0; i < children_count; ++i) {
        children_states[i]->hori_offset = 0;
        children_states[i]->vert_offset = 0;
    }
}

/*
    NODE TYPES PART
*/

// ===========================
// Box Type

void box_render(lui_cache* cache, const void* node_data, lla_mat2x3* transform) {
    lui_box_data* bdata = (lui_box_data*)node_data;

    cache_push_draw_request(cache, (draw_request){
        .transform      = *transform,
        .texture_index  = 0,                // todo
        .texture_atlas  = (lgx_uv_2d){0},   // todo
        .clip_index     = 0,                // todo
        .depth_index    = 0,                // todo
        .r              = bdata->tint.r,
        .g              = bdata->tint.g,
        .b              = bdata->tint.b,
        .a              = bdata->tint.a,
        .shader         = bdata->shader,
        .glyph_first    = -1,
        .glyph_count    = 1
    });
}

const lui_type lui_box_type = {
    .render = box_render
};


// Row

void row_width_measure(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states
) {
    const lui_row_data* data = (const lui_row_data*)node_data;

    lui_length own = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        lui_length child = children_states[i]->measured_width;
        own.min += child.min;
        own.max += child.max;
    }

    size_t spaces = children_count ? children_count - 1 : 0;

    own.min += spaces * data->spacing.min;

    if (own.max != lui_inf_length && data->spacing.max != lui_inf_length) {
        own.max += spaces * data->spacing.max;
    } else {
        own.max = lui_inf_length;
    }

    if (own.min != own.max) {
        own.flex = 1.0f;
    }

    node_state->measured_width = own;
}

void row_width_distribute(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states
) {
    (void)node_data;

    float flexsum = 0.0f;

    for (size_t i = 0; i < children_count; ++i) {
        flexsum += children_states[i]->measured_width.flex;
    }

    int* assigned = calloc(children_count, sizeof(int));

    size_t unsolved = children_count;
    int available   = node_state->given_width;

    while (unsolved && available > 1 && flexsum > 0.0f) {
        unsolved = 0;
        float next_flexsum = 0.0f;

        for (size_t i = 0; i < children_count; ++i) {
            lui_length m = children_states[i]->measured_width;

            if (assigned[i] == m.max)
                continue;

            int gain = (int)(available * (m.flex / flexsum));

            if (assigned[i] + gain < m.min) {
                gain = m.min - assigned[i];
            } else if (assigned[i] + gain > m.max) {
                gain = m.max - assigned[i];
            }

            assigned[i] += gain;

            if (assigned[i] != m.max) {
                next_flexsum += m.flex;
                ++unsolved;
            }
        }

        flexsum = next_flexsum;
    }

    for (size_t i = 0; i < children_count; ++i) {
        children_states[i]->given_width = assigned[i];
    }

    free(assigned);
}

void row_height_measure(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states
) {
    (void)node_data;

    lui_length own = {0, 0, 0.0f};

    for (size_t i = 0; i < children_count; ++i) {
        lui_length child = children_states[i]->measured_height;

        own.min = max_int(own.min, child.min);
        own.max = max_int(own.max, child.max);
    }

    if (own.min != own.max) {
        own.flex = 1.0f;
    }

    node_state->measured_height = own;
}

void row_height_distribute(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states
) {
    (void)node_data;

    for (size_t i = 0; i < children_count; ++i) {
        children_states[i]->given_height =
            clamp_length_in_desire(
                node_state->given_height,
                children_states[i]->measured_height
            );
    }
}

void row_position(
    const void*             node_data,
    lui_node_layout_state*  node_state,
    size_t                  children_count,
    lui_node_layout_state** children_states
) {
    const lui_row_data* data = (const lui_row_data*)node_data;

    int total_width = 0;

    for (size_t i = 0; i < children_count; ++i) {
        total_width += children_states[i]->given_width;
    }

    if (children_count > 1) {
        total_width += (int)((children_count - 1) * data->spacing.min);
    }

    int x = (int)((node_state->given_width - total_width) * data->horizontal_align);

    for (size_t i = 0; i < children_count; ++i) {
        lui_node_layout_state* child = children_states[i];
        int y = node_state->vert_offset + (int)((node_state->given_height - child->given_height) * data->vertical_align);

        child->hori_offset = x;
        child->vert_offset  = y;

        x += child->given_width;

        if (i + 1 < children_count) {
            x += data->spacing.min;
        }
    }
}

const lui_type lui_row_type = {
    .array_child        = 1,
    .width_measure      = row_width_measure,
    .width_distribute   = row_width_distribute,
    .height_measure     = row_height_measure,
    .height_distribute  = row_height_distribute,
    .position           = row_position
};

/*
    RENDERING PART
*/

// ===========================
// Rendering Common

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
    lla_mat2x3 clip;
} gpu_clipbox;

typedef struct gpu_glyph {
    lgx_uv_2d   atlas_position;
    int         off_x,  off_y;
    int         size_x, size_y;
} gpu_glyph;

lgx_buffer* create_ssbo(lgx_hardware* hardware, uint64_t bytes) {
    return lgx_create_buffer(hardware, &(lgx_buffer_create_info){
        .size_bytes         = bytes,
        .usage              = lgx_buffer_usage_storage,
        .memory_strategy    = lgx_memory_allocation_strategy_dedicated,
        .memory_access      = lgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_write
    });
}

static const uint32_t internal_textures_limit       = 1024;
static const uint64_t initial_instances_buffer_size = 1024 * sizeof(gpu_instance);
static const uint64_t initial_draw_item_buffer_size = 1024 * sizeof(gpu_draw_item);
static const uint64_t initial_clipboxes_buffer_size =   16 * sizeof(gpu_clipbox);

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
    {   // the instances buffer
        .binding = 0,
        .count   = 1,
        .stages  = lgx_shader_stage_vertex,
        .type    = lgx_descriptor_binding_type_storage_buffer
    },
    {   // the draw items buffer
        .binding = 1,
        .count   = 1,
        .stages  = lgx_shader_stage_vertex,
        .type    = lgx_descriptor_binding_type_storage_buffer
    },
    {   // the clips buffer
        .binding = 2,
        .count   = 1,
        .stages  = lgx_shader_stage_pixel,
        .type    = lgx_descriptor_binding_type_storage_buffer
    },
    {   // the sampler
        .binding = 3,
        .count   = 1,
        .stages  = lgx_shader_stage_pixel,
        .type    = lgx_descriptor_binding_type_sampler,
    },
    {   // the textures
        .binding = 4,
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
    shared->descriptor_textures_array_length = max_textures > internal_textures_limit ? internal_textures_limit : max_textures;

    // Copy descriptor bindings info
    uint32_t bindings_count = sizeof(descriptor_bindings) / sizeof(lgx_descriptor_binding);
    lgx_descriptor_binding* bindings = malloc(bindings_count * sizeof(lgx_descriptor_binding)); 
    if (!bindings) goto _fail; memcpy(bindings, descriptor_bindings, sizeof(descriptor_bindings));

    // Overwrite textures limit
    bindings[4].count = shared->descriptor_textures_array_length;

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
    free(shared);
}

// ===========================
// Frames

typedef struct single_frame {
    uint32_t                    instances_to_render;
    lgx_buffer*                 instances_buffer;
    lgx_buffer*                 draw_items_buffer;
    lgx_buffer*                 clipboxes_buffer;
    lgx_descriptor*             descriptor;
} single_frame;

struct lui_frames {
    lui_shared*                 owning_shared;
    lgx_descriptor_allocator*   descriptor_allocator;
    uint32_t                    frames_count;
    single_frame*               frames;
};

void frame_descriptor_bind_buffers_and_sampler(lgx_hardware* hardware, single_frame* frame, lgx_sampler* sampler) {
    lgx_descriptor_write_info           writes[4];
    lgx_descriptor_buffer_write_info    binfos[3];
    lgx_descriptor_sampler_write_info   sinfos[1];

    binfos[0] = (lgx_descriptor_buffer_write_info){
        .buffer = frame->instances_buffer,
        .offset = 0,
        .length = lgx_buffer_get_size_bytes(frame->instances_buffer)
    };

    binfos[1] = (lgx_descriptor_buffer_write_info){
        .buffer = frame->draw_items_buffer,
        .offset = 0,
        .length = lgx_buffer_get_size_bytes(frame->draw_items_buffer)
    };

    binfos[2] = (lgx_descriptor_buffer_write_info){
        .buffer = frame->clipboxes_buffer,
        .offset = 0,
        .length = lgx_buffer_get_size_bytes(frame->clipboxes_buffer)
    };

    sinfos[0] = (lgx_descriptor_sampler_write_info){
        .sampler = sampler
    };

    writes[0] = (lgx_descriptor_write_info){
        .descriptor             = frame->descriptor,
        .binding_type           = lgx_descriptor_binding_type_storage_buffer,
        .binding_index          = 0,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_buffers      = &binfos[0]
    };

    writes[1] = (lgx_descriptor_write_info){
        .descriptor             = frame->descriptor,
        .binding_type           = lgx_descriptor_binding_type_storage_buffer,
        .binding_index          = 1,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_buffers      = &binfos[1]
    };

    writes[2] = (lgx_descriptor_write_info){
        .descriptor             = frame->descriptor,
        .binding_type           = lgx_descriptor_binding_type_storage_buffer,
        .binding_index          = 2,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_buffers      = &binfos[2]
    };

    writes[3] = (lgx_descriptor_write_info){
        .descriptor             = frame->descriptor,
        .binding_type           = lgx_descriptor_binding_type_sampler,
        .binding_index          = 3,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_samplers     = &sinfos[0]
    };
    
    lgx_descriptors_write(hardware, 4, writes);
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
            .descriptor        = lgx_descriptor_allocator_alloc_descriptor(frames->descriptor_allocator),
            .instances_buffer  = create_ssbo(hardware, initial_instances_buffer_size),
            .draw_items_buffer = create_ssbo(hardware, initial_draw_item_buffer_size),
            .clipboxes_buffer  = create_ssbo(hardware, initial_clipboxes_buffer_size),
        };

        if (!frame->descriptor || !frame->instances_buffer || !frame->draw_items_buffer || !frame->clipboxes_buffer) goto _fail;
        frame_descriptor_bind_buffers_and_sampler(hardware, frame, shared->sampler);
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
        lgx_free_buffer(frame->instances_buffer);
        lgx_free_buffer(frame->draw_items_buffer);
        lgx_free_buffer(frame->clipboxes_buffer);
    }

    // all per-frame descriptors freed with allocator
    lgx_free_descriptor_allocator(frames->descriptor_allocator);
    free(frames->frames);

    free(frames);
}

// ===========================
// Rendering Functions

// Walk and remeasure
// Walk and generate render desires
// Sort render desires by depth
// Upload
// Problematic upload : text - automatically issue previous to free after new allocated

void lui_upload_cache(
    lui_cache*          cache,
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
    lgx_hardware*   hardware = frames->owning_shared->owning_hardware;

    uint32_t        item_count; 
    uint64_t        item_bytes;
    gpu_draw_item*  items;

    uint32_t        instances_count = 0;
    uint64_t        instances_bytes = 0;
    gpu_instance*   instances;
    
    // Generate GPU Items, findout instances count
    item_count = cache->draw_requests_count;
    item_bytes = cache->draw_requests_count * sizeof(gpu_draw_item);
    items = malloc(item_bytes);

    for (uint32_t i = 0; i < item_count; i++) {
        draw_request req = cache->draw_requests[i];

        items[i] = (gpu_draw_item){
            .transform      = req.transform,
            .texture_index  = req.texture_index,
            .clipbox_index  = req.clip_index,
            .r              = req.r,
            .g              = req.g,
            .b              = req.b,
            .a              = req.a,
            .shader         = req.shader
        };

        instances_count += req.glyph_count;
    }

    // Generate GPU Instances
    instances_bytes = instances_count * sizeof(gpu_instance);
    instances = malloc(instances_bytes);
    uint32_t instance_idx = 0;
    for (int i = 0; i < cache->draw_requests_count; i++) {
        draw_request req = cache->draw_requests[i];
        for (int g = 0; g < req.glyph_count; g++) {
            instances[instance_idx++] = (gpu_instance){
                .item   = i,
                .glyph  = g + req.glyph_first
            };
        }
    }

    // Generate GPU Clipboxes
    // Todo

    // Ensure Buffer Sizes
    int rebind = 0;
    single_frame* frame = &frames->frames[frame_idx];

    if (lgx_buffer_get_size_bytes(frame->draw_items_buffer) < item_bytes) {
        lgx_free_buffer(frame->draw_items_buffer);
        frame->draw_items_buffer = create_ssbo(hardware, item_bytes);
        rebind = 1;
    }

    if (lgx_buffer_get_size_bytes(frame->instances_buffer) < instances_bytes) {
        lgx_free_buffer(frame->instances_buffer);
        frame->instances_buffer = create_ssbo(hardware, instances_bytes);
        rebind = 1;
    }

    // If resize failed return
    if (!frame->draw_items_buffer || !frame->instances_buffer) return;
    if (rebind) frame_descriptor_bind_buffers_and_sampler(hardware, frame, frames->owning_shared->sampler);

    // Upload
    lgx_buffer_multi_upload_region regions[] = {
        {
            .buffer         = frame->draw_items_buffer,
            .buffer_offset  = 0,
            .source_data    = items,
            .source_bytes   = item_count * sizeof(gpu_draw_item)
        },
        {
            .buffer         = frame->instances_buffer,
            .buffer_offset  = 0,
            .source_data    = instances,
            .source_bytes   = instances_count * sizeof(gpu_instance)
        }
    };

    lgx_buffer_multi_upload(
        regions,
        sizeof(regions) / sizeof(lgx_buffer_multi_upload_region),
        command_list, 
        queue_for_uploads,
        staging_memory,
        staging_memory_region_offset,
        staging_memory_region_size,
        upload_finished_cpu,
        upload_finished_gpu
    );

    // Mark to render
    frame->instances_to_render = instances_count;
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

#endif // LIGHT_USER_INTERFACE_IMPL
