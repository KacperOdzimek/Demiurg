/*
----------------------------------------------------------------
Contents

This file provided simple system to render basic shapes: lines, triangles, rectangles, circles.

----------------------------------------------------------------
Code info:
- lshp prefix
- LIGHT_SHAPES_IMPL macro to build
- graphics.h dependant
- linear_algebra.h dependant

----------------------------------------------------------------
Usage

- Create lshp_shared object - it contains shared read-only objects for rendering.
    Create one per hardware. In create info, link shaders. (compile provided default_shader_source).
- Create lshp_frames_contextes - it contains per frame geometry buffers you record with
    drawing methods. Create one per hardware.
- Then you can draw whatever You want it given frame in flight context
- Upload data to gpu with lshp_upload
- Render with graphics command lshp_gcmd_render

----------------------------------------------------------------
Notes

- The implementation has misnamed struct gpu_instance which should be something like
    struct gpu_triangle_render_info, but I am not going to change that now, it is 3 am.
*/

#ifndef LIGHT_SHAPES_H
#define LIGHT_SHAPES_H

#include "graphics.h"
#include "linear_algebra.h"

// Shapes Rendering Shared Object

typedef struct lshp_shared_create_info {
    lgx_render_target_layout*   pipeline_render_target_layout;
    const char*                 pipeline_vertex_shader_source_code;
    uint32_t                    pipeline_vertex_shader_source_size;
    const char*                 pipeline_pixel_shader_source_code;
    uint32_t                    pipeline_pixel_shader_source_size;
} lshp_shared_create_info;

typedef struct lshp_shared lshp_shared;
lshp_shared* lshp_create_shared(lgx_hardware*, const lshp_shared_create_info* info);
void lshp_free_shared(lshp_shared*);

// Shapes Rendering Frame Contextes

typedef struct lshp_frames_contextes_create_info {
    lshp_shared*    shared;
    uint32_t        frames_in_flight_count;
} lshp_frames_contextes_create_info;

typedef struct lshp_frames_contextes lshp_frames_contextes;
lshp_frames_contextes* lshp_create_frames_contextes(lgx_hardware*, const lshp_frames_contextes_create_info* info);
void lshp_free_frames_contextes(lshp_frames_contextes*);

// shorthand not to pass frame in flight to every function
typedef struct lshp_frame_context {
    lshp_frames_contextes*  contextes;
    uint32_t                frame_in_flight;
} lshp_frame_context;

// Actuall Draw Operation

// resets draw requests in context
void lshp_reset(
    lshp_frame_context* context,
    int clear_geometry, 
    int clear_state
);

// uploads draw requests to gpu
void lshp_upload(
    lgx_command_list*   command_list,
    lgx_hardware_queue* queue_for_uploads,
    lshp_frame_context* context,
    lgx_staging_memory* staging_memory,
    uint64_t            staging_memory_region_offset,
    uint64_t            staging_memory_region_size,
    lgx_cpu_signal*     upload_finished_cpu,
    lgx_gpu_signal*     upload_finished_gpu
);

// command to draw from context
// needs to be re recorded every frame as amount of
// drawn primitives may change
// call with render target bound, viewport and scissors set
void lshp_gcmd_render(
    lgx_command_list*   target,
    lshp_frame_context* context
);

// Shapes Draw Function

// set drawn shapes color
void lshp_set_color(
    lshp_frame_context* context,
    float r, float g, float b, float a
);

void lshp_set_line_thickness(
    lshp_frame_context* context,
    float line_thickness
);

void lshp_line(
    lshp_frame_context* context,
    lla_vec2 begin, lla_vec2 end
);

void lshp_triangle(
    lshp_frame_context* context,
    lla_vec2 a, lla_vec2 b, lla_vec2 c
);

void lshp_rect(
    lshp_frame_context* context,
    lla_vec2 first_corner, lla_vec2 second_corner
);

void lshp_circle(
    lshp_frame_context* context,
    lla_vec2 center, float radius
);

#endif // LIGHT_SHAPES_H

#ifdef LIGHT_SHAPES_IMPL

#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
    Config
*/

static lgx_vertex_input_attribute_info vertex_attributes[] = {
    {   // position : per vertex
        .binding    = 0,
        .location   = 0,
        .offset     = 0,
        .type       = lgx_data_type_vec2f32
    },
};
static const uint64_t gpu_vertex_sizeof = 2 * 4;

static lgx_vertex_input_binding_info vertex_bindings[] = {
    {
        .binding    = 0,
        .input_rate = lgx_vertex_attribute_input_rate_per_vertex,
        .stride     = gpu_vertex_sizeof
    }
};

static lgx_descriptor_binding descriptor_bindings[] = {
    {   // the instances buffer
        .binding = 0,
        .count   = 1,
        .stages  = lgx_shader_stage_pixel,
        .type    = lgx_descriptor_binding_type_storage_buffer
    },
};

typedef struct gpu_instance {
    float r, g, b, a;
    float center_x, center_y;
    float radius;
} gpu_instance;

static uint64_t initial_vertices_buffer_cap  = 128 * 3 * gpu_vertex_sizeof;
static uint64_t initial_instances_buffer_cap = 128 * sizeof(gpu_instance);

/*
    Shared
*/

struct lshp_shared {
    lgx_hardware*                       owning_hardware;
    lgx_descriptor_layout*              descriptor_layout;
    lgx_pipeline_descriptors_layout*    pipeline_layout;
    lgx_pipeline*                       pipeline;
};

lshp_shared* lshp_create_shared(lgx_hardware* hardware, const lshp_shared_create_info* info) {
    lshp_shared* shared = calloc(1, sizeof(lshp_shared)); if (!shared) return NULL;
    shared->owning_hardware = hardware;

    // Descriptor Layout
    shared->descriptor_layout = lgx_create_descriptor_layout(hardware, &(lgx_descriptor_layout_create_info){
        .bindings_count = sizeof(descriptor_bindings) / sizeof(lgx_descriptor_binding),
        .bindings = descriptor_bindings
    }); if (!shared->descriptor_layout) goto _fail;

    // Pipeline Layout
    shared->pipeline_layout = lgx_create_pipeline_descriptors_layout(hardware, &(lgx_pipeline_descriptors_layout_create_info){
        .layouts_count = 1, .layouts = &shared->descriptor_layout
    }); if (!shared->pipeline_layout) goto _fail;
    

    // Pipeline Shaders
    lgx_shader* vertex_shader = lgx_create_shader(shared->owning_hardware, &(lgx_shader_create_info){
        .source_size = info->pipeline_vertex_shader_source_size,
        .source_code = info->pipeline_vertex_shader_source_code
    });

    lgx_shader* pixel_shader = lgx_create_shader(shared->owning_hardware, &(lgx_shader_create_info){
        .source_size = info->pipeline_pixel_shader_source_size,
        .source_code = info->pipeline_pixel_shader_source_code
    });

    if (!vertex_shader || !pixel_shader) {
        lgx_free_shader(vertex_shader);
        lgx_free_shader(pixel_shader);
        goto _fail;
    }

    lgx_pipeline_create_info pip_create_info = {
        .render_target_layout   = info->pipeline_render_target_layout,
        .descriptor_layout      = shared->pipeline_layout,

        .vertex_layout          = {
            .attributes_count   = sizeof(vertex_attributes) / sizeof(lgx_vertex_input_attribute_info),
            .attributes         = vertex_attributes,
            .bindings_count     = sizeof(vertex_bindings) / sizeof(lgx_vertex_input_binding_info),
            .bindings           = vertex_bindings,
        },

        .shader_stages  = {
            .vertex = vertex_shader,
            .pixel  = pixel_shader
        },

        .input_assembly = {
            .topology = lgx_primitive_topology_triangle_list
        },

        .rasterizer = {
            .scissor_enable     = 0,
            .depth_clamp_enable = 0,
            .fill_mode          = lgx_fill_mode_solid,
            .cull_mode          = lgx_cull_mode_none
        },

        .blend = {
            .blend_enable   = 1,
            .blend_op       = lgx_blend_op_add,
            .src_factor     = lgx_blend_factor_src_alpha,
            .dst_factor     = lgx_blend_factor_one_minus_src_alpha,
        },

        .depth_stencil = {
            .depth_test_enable      = 0,
            .depth_write_enable     = 0,
            .stencil_test_enable    = 0
        }
    };
    shared->pipeline = lgx_create_pipeline(shared->owning_hardware, &pip_create_info);

    lgx_free_shader(vertex_shader);
    lgx_free_shader(pixel_shader);

    if (!shared->pipeline) goto _fail;
    return shared;

_fail:
    lshp_free_shared(shared); 
    return NULL;
}

void lshp_free_shared(lshp_shared* shared) {
    if (!shared) return;
    lgx_free_pipeline(shared->pipeline);
    lgx_free_pipeline_descriptors_layout(shared->pipeline_layout);
    lgx_free_descriptor_layout(shared->descriptor_layout);
    free(shared);
}

/*
    An Arena
*/

// arena metrics in bytes
// will be used to hold either only floats or only gpu_instances
// align okay then
typedef struct arena {
    uint64_t    position;
    uint64_t    capacity;
    char*       data;
} arena;

static inline arena alloc_arena(uint64_t cap_bytes) {
    return (arena){
        .position   = 0,
        .capacity   = cap_bytes,
        .data       = malloc(cap_bytes)
    };
}

static inline void free_arena(arena a) {
    free(a.data);
}

// 1 at success, 0 at failure
static inline int ensure_arena_free_space(arena* a, uint64_t req_space) {
    uint64_t left_space = a->capacity - a->position;
    if (left_space < req_space) {
        // try to realloc
        uint64_t new_cap = a->capacity;
        while (new_cap - a->position < req_space) new_cap *= 2;
        char* new_data = realloc(a->data, new_cap);
        if (!new_data) return 0; // realloc failed

        a->capacity = new_cap;
        a->data     = new_data;
    }
    return 1;
}

/*
    Frames
*/

typedef struct frame_context {
    // drawing state
    float           r, g, b, a; // current color
    float           line_thickness;

    // drawing data
    arena           vertex_arena;
    arena           instance_arena;
    uint32_t        triangles_to_draw;

    // gpu
    lgx_descriptor* assigned_descriptor;
    lgx_buffer*     instances_buffer;
    lgx_buffer*     vertices_buffer;
} frame_context;

struct lshp_frames_contextes {
    lgx_hardware*               owning_hardware;
    lshp_shared*                owning_shared;
    lgx_descriptor_allocator*   descriptor_allocator;
    uint32_t                    in_flight;
    frame_context*              frames;
};

lgx_buffer* create_buffer(lgx_hardware* hardware, uint64_t size_bytes, lgx_buffer_usage usage) {
    lgx_buffer_create_info vb_create_info = {
        .usage              = usage,
        .size_bytes         = size_bytes,
        .memory_access      = lgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_write,
        .memory_strategy    = lgx_memory_allocation_strategy_paged
    };
    return lgx_create_buffer(hardware, &vb_create_info);
}

void link_frame_descriptor_with_instance_buffer(lgx_hardware* hardware, frame_context* frame) {
    lgx_descriptor_buffer_write_info binfo = {
        .buffer = frame->instances_buffer,
        .offset = 0,
        .length = lgx_buffer_get_size_bytes(frame->instances_buffer)
    };

    lgx_descriptor_write_info write = {
        .descriptor             = frame->assigned_descriptor,
        .binding_type           = lgx_descriptor_binding_type_storage_buffer,
        .binding_index          = 0,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_buffers      = &binfo
    };

    lgx_descriptors_write(hardware, 1, &write);
}

lshp_frames_contextes* lshp_create_frames_contextes(lgx_hardware* hardware, const lshp_frames_contextes_create_info* info) {
    if (!hardware || !info->shared) goto _fail;

    lshp_frames_contextes* contextes = calloc(1, sizeof(lshp_frames_contextes)); 
    if (!contextes) goto _fail;

    *contextes = (lshp_frames_contextes) {
        .owning_hardware    = hardware,
        .owning_shared      = info->shared,
        .in_flight          = info->frames_in_flight_count
    };

    // Descriptors Allocator
    contextes->descriptor_allocator = lgx_create_descriptor_allocator(hardware, &(lgx_descriptor_allocator_create_info){
        .descriptor_layout = info->shared->descriptor_layout,
        .max_descriptors_allocated = info->frames_in_flight_count
    }); if (!contextes->descriptor_allocator) goto _fail;
    
    // Frames
    contextes->frames = calloc(info->frames_in_flight_count, sizeof(frame_context));
    if (!contextes->frames) goto _fail;
    for (uint32_t i = 0; i < info->frames_in_flight_count; i++) {
        frame_context* frame = &contextes->frames[i];

        *frame = (frame_context){
            .r = 1, .g = 1, .b = 1, .a = 1,
            .line_thickness = 0.01,

            .vertex_arena   = alloc_arena(initial_vertices_buffer_cap),
            .instance_arena = alloc_arena(initial_instances_buffer_cap),
            
            .vertices_buffer     = create_buffer(hardware, initial_vertices_buffer_cap,  lgx_buffer_usage_vertex),
            .instances_buffer    = create_buffer(hardware, initial_instances_buffer_cap, lgx_buffer_usage_storage),

            .assigned_descriptor = lgx_descriptor_allocator_alloc_descriptor(contextes->descriptor_allocator)
        };

        if (
            !frame->vertex_arena.data   || 
            !frame->instance_arena.data ||
            !frame->vertices_buffer     || 
            !frame->instances_buffer    || 
            !frame->assigned_descriptor
        ) goto _fail;
        
        link_frame_descriptor_with_instance_buffer(hardware, frame);
    }

    return contextes;

_fail:
    lshp_free_frames_contextes(contextes);
    return NULL;
}

void lshp_free_frames_contextes(lshp_frames_contextes* contextes) {
    if (!contextes) return;

    for (uint32_t i = 0; i < contextes->in_flight; i++) {
        lgx_free_buffer(contextes->frames[i].vertices_buffer);
        lgx_free_buffer(contextes->frames[i].instances_buffer);
        free_arena(contextes->frames[i].vertex_arena);
        free_arena(contextes->frames[i].instance_arena);
        // descriptors free with allocator
    }

    lgx_free_descriptor_allocator(contextes->descriptor_allocator);
    free(contextes->frames);
    free(contextes);
}

void lshp_reset(lshp_frame_context* context, int clear_geometry, int clear_state) {
    frame_context* frame = &context->contextes->frames[context->frame_in_flight];

    if (clear_geometry) {
        frame->vertex_arena.position = 0;
        frame->instance_arena.position = 0;
        frame->triangles_to_draw = 0;
    }

    if (clear_state) {
        frame->r = 1; frame->g = 1; frame->b = 1; frame->a = 1;
        frame->line_thickness = 0.01;
    }
}

// returns bytes of data that can be copied
// buffer at *buffer may be recreated due to call
uint32_t ensure_buffer_size_pre_upload
(lgx_hardware* hardware, lgx_buffer** buffer, arena* a, lgx_buffer_usage buffer_usage, int* was_buffer_recreated) {
    *was_buffer_recreated = 0;

    if (lgx_buffer_get_size_bytes(*buffer) < a->position) {
        lgx_buffer* new_buffer = create_buffer(hardware, a->capacity, buffer_usage);

        // stick to old buffer, since creation of new failed
        if (!new_buffer) return lgx_buffer_get_size_bytes(*buffer);
        else {
            *was_buffer_recreated = 1;
            lgx_free_buffer(*buffer);
            *buffer = new_buffer;
        }
    }

    return a->position;
}

void lshp_upload(
    lgx_command_list*   command_list,
    lgx_hardware_queue* queue_for_uploads,
    lshp_frame_context* context,
    lgx_staging_memory* staging_memory,
    uint64_t            staging_memory_region_offset,
    uint64_t            staging_memory_region_size,
    lgx_cpu_signal*     upload_finished_cpu,
    lgx_gpu_signal*     upload_finished_gpu
) {
    int provided_cpu_signal = upload_finished_cpu && 1;

    lgx_hardware*  hardware = context->contextes->owning_hardware;
    frame_context* frame = &context->contextes->frames[context->frame_in_flight];

    int v_buffer_recreated = 0;
    uint32_t v_to_copy = ensure_buffer_size_pre_upload(
        hardware, &frame->vertices_buffer, &frame->vertex_arena, lgx_buffer_usage_vertex, &v_buffer_recreated
    );

    int i_buffer_recreated = 0;
    uint32_t i_to_copy = ensure_buffer_size_pre_upload(
        hardware, &frame->instances_buffer, &frame->instance_arena, lgx_buffer_usage_storage, &i_buffer_recreated
    ); if (i_buffer_recreated) link_frame_descriptor_with_instance_buffer(hardware, frame);

    uint32_t v_copy_position = 0;
    uint32_t i_copy_position = 0;
    
    char* v_data = (char*)frame->vertex_arena.data;
    char* i_data = (char*)frame->instance_arena.data;

    while (v_copy_position < v_to_copy || i_copy_position < i_to_copy) {
        uint32_t v_bytes = 0;
        uint32_t i_bytes = 0;

        // Copy memory to staging
        char* mapped = lgx_staging_memory_map(staging_memory, staging_memory_region_offset, staging_memory_region_size);
            // Copy as much vertex data as possible
            v_bytes = v_to_copy - v_copy_position;
            if (v_bytes > staging_memory_region_size) v_bytes = staging_memory_region_size;
            memcpy(mapped, v_data + v_copy_position, v_bytes);

            // Copy as much instance data as possible
            i_bytes = i_to_copy - i_copy_position;
            if (i_bytes > staging_memory_region_size - v_bytes) i_bytes = staging_memory_region_size - v_bytes;
            memcpy(mapped + v_bytes, i_data + i_copy_position, i_bytes);
        lgx_staging_memory_unmap(staging_memory);

        // Record command list for target buffer rewrite
        lgx_begin_command_list_recording(command_list);
            if (v_bytes) lgx_cmd_copy_staging_memory_to_buffer(
                command_list, 
                staging_memory, 
                frame->vertices_buffer, 
                staging_memory_region_offset + 0, 
                v_copy_position, 
                v_bytes
            );
            
            if (i_bytes) lgx_cmd_copy_staging_memory_to_buffer(
                command_list, 
                staging_memory, 
                frame->instances_buffer, 
                staging_memory_region_offset + v_bytes,
                i_copy_position, 
                i_bytes
            );
        lgx_finish_command_list_recording(command_list);

        // Advance and find out whether is last batch
        v_copy_position += v_bytes;
        i_copy_position += i_bytes;
        int is_final_batch = !(v_copy_position < v_to_copy || i_copy_position < i_to_copy);

        // If not final batch, but multiple have to be sent
        // Ensure we have cpu signal
        if (!is_final_batch && upload_finished_cpu == NULL) {
            upload_finished_cpu = lgx_create_cpu_signal(hardware, &(lgx_cpu_signal_create_info){.initialy_signaled = 0});
        }

        // Submit
        lgx_submit_info submit = {
            .command_lists_count        = 1,
            .command_lists              = &command_list,
            .cpu_signal                 = upload_finished_cpu,
            .signal_gpu_signals_count   = (is_final_batch && upload_finished_gpu != NULL) ? 1 : 0,
            .signal_gpu_signals         = &upload_finished_gpu
        };

        lgx_submit_command_list(queue_for_uploads, &submit);

        if (!is_final_batch) {
            lgx_cpu_signal_wait(upload_finished_cpu);
            lgx_cpu_signal_reset(upload_finished_cpu);
            continue;
        }
    }

    // Cleanup - free allocated cpu signal if exist
    if (!provided_cpu_signal && upload_finished_cpu) {
        lgx_cpu_signal_wait(upload_finished_cpu);
        lgx_free_cpu_signal(upload_finished_cpu);
    }
}

void lshp_gcmd_render(
    lgx_command_list*           target,
    lshp_frame_context*   context
) {
    frame_context* frame = &context->contextes->frames[context->frame_in_flight];
    if (frame->triangles_to_draw) {
        lgx_gcmd_bind_graphics_pipeline(target, context->contextes->owning_shared->pipeline);
        lgx_gcmd_bind_graphics_pipeline_vertex_buffer(target, frame->vertices_buffer, 0, 0);
        lgx_gcmd_bind_graphics_pipeline_descriptors(
            target,
            context->contextes->owning_shared->pipeline_layout,
            0, 1, &frame->assigned_descriptor
        );
        lgx_gcmd_draw_vertices(target, frame->triangles_to_draw * 3, 0, 1, 0);
    }
}

// Methods

static inline void emit_triangle(
    lshp_frame_context* context, 
    float x0,   float y0,
    float x1,   float y1,
    float x2,   float y2,
    float rcx,  float rcy, // radius center
    float radius
) {
    frame_context* frame = &context->contextes->frames[context->frame_in_flight];

    // Ensure arena space
    if (!ensure_arena_free_space(&frame->vertex_arena, 3 * gpu_vertex_sizeof))      return;
    if (!ensure_arena_free_space(&frame->instance_arena, 1 * sizeof(gpu_instance))) return;
    
    // Write vertex arena
    float* varena = (float*)(frame->vertex_arena.data + frame->vertex_arena.position);
    frame->vertex_arena.position += 3 * gpu_vertex_sizeof;
    varena[0] = x0; varena[1] = y0;
    varena[2] = x1; varena[3] = y1;
    varena[4] = x2; varena[5] = y2;

    // Write instance arena
    gpu_instance* iarena = (gpu_instance*)(frame->instance_arena.data + frame->instance_arena.position);
    frame->instance_arena.position += 1 * sizeof(gpu_instance);
    iarena[0] = (gpu_instance){
        .r          = frame->r, 
        .g          = frame->g, 
        .b          = frame->b, 
        .a          = frame->a,
        .center_x   = rcx, 
        .center_y   = rcy,
        .radius     = radius,
    };

    // Emit triangle
    frame->triangles_to_draw++;
};

void lshp_set_color(
    lshp_frame_context* context,
    float r, float g, float b, float a
) {
    frame_context* frame = &context->contextes->frames[context->frame_in_flight];
    frame->r = r; frame->g = g; frame->b = b; frame->a = a;
}

void lshp_set_line_thickness(
    lshp_frame_context* context,
    float line_thickness
) {
    frame_context* frame = &context->contextes->frames[context->frame_in_flight];
    frame->line_thickness = line_thickness;
}

void lshp_line(
    lshp_frame_context* context,
    lla_vec2 begin, lla_vec2 end
) {
    frame_context* frame = &context->contextes->frames[context->frame_in_flight];

    float dx = end.x - begin.x;
    float dy = end.y - begin.y;

    float len = sqrtf(dx * dx + dy * dy);
    if (len == 0.0f) return;

    float nx = -dy / len;
    float ny =  dx / len;

    float ox = nx * frame->line_thickness * 0.5f;
    float oy = ny * frame->line_thickness * 0.5f;

    emit_triangle(context,
        begin.x + ox, begin.y + oy,
        begin.x - ox, begin.y - oy,
        end.x   + ox, end.y   + oy,
        -1.0f, -1.0f, -1.0f // unrounded
    );

    emit_triangle(context,
        end.x   - ox, end.y   - oy,
        begin.x - ox, begin.y - oy,
        end.x   + ox, end.y   + oy,
        -1.0f, -1.0f, -1.0f // unrounded
    );
}

void lshp_triangle(
    lshp_frame_context* context,
    lla_vec2 a, lla_vec2 b, lla_vec2 c
) {
    emit_triangle(context, a.x, a.y, b.x, b.y, c.x, c.y, 0, 0, -1.0f); // unrounded
}

void lshp_rect(
    lshp_frame_context* context,
    lla_vec2 first_corner, lla_vec2 second_corner
) {
    emit_triangle(
        context,
        first_corner.x,  first_corner.y,
        second_corner.x, first_corner.y,
        second_corner.x, second_corner.y,
        0.0f, 0.0f, 0.0f // unrounded
    );

    emit_triangle(
        context,
        first_corner.x,  first_corner.y,
        second_corner.x, second_corner.y,
        first_corner.x,  second_corner.y,
        0.0f, 0.0f, 0.0f // unrounded
    );
}

void lshp_circle(
    lshp_frame_context* context,
    lla_vec2 center, float radius
) {
    emit_triangle(context, 
        center.x - radius, center.y - radius, 
        center.x - radius, center.y + radius, 
        center.x + radius, center.y - radius,
        center.x, center.y, radius
    );

    emit_triangle(context, 
        center.x + radius, center.y + radius, 
        center.x - radius, center.y + radius, 
        center.x + radius, center.y - radius,
        center.x, center.y, radius
    );
}

#endif // LIGHT_SHAPES_IMPL
