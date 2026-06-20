/*
----------------------------------------------------------------
Contents

This file provides simple system to render basic shapes: lines, triangles, rectangles, circles.

----------------------------------------------------------------
Code info:
- dshp prefix
- DEMIURG_SHAPES_IMPL macro to build
- graphics.h dependant
- linear_algebra.h dependant

----------------------------------------------------------------
Usage

- Create dshp_shared object - it contains shared read-only objects for rendering.
    Create one per hardware. In create info, link shaders. (compile provided default_shader_source).
- Create dshp_frames - it contains per frame geometry buffers you record with
    drawing methods. Create one per hardware.
- Then you can draw whatever You want it given frame in flight context
- Upload data to gpu with dshp_upload
- Render with graphics command dshp_gcmd_render

----------------------------------------------------------------
Notes
- The implementation has misnamed struct gpu_instance which should be something like
    struct gpu_triangle_render_info, but I am not going to change that now, it is 3 am.
*/

#ifndef DEMIURG_SHAPES_H
#define DEMIURG_SHAPES_H

#include "graphics.h"
#include "linear_algebra.h"

// Shapes Rendering Shared Object

typedef struct dshp_shared_create_info {
    dgx_render_target_layout*   pipeline_render_target_layout;
    const char*                 pipeline_vertex_shader_source_code;
    uint32_t                    pipeline_vertex_shader_source_size;
    const char*                 pipeline_pixel_shader_source_code;
    uint32_t                    pipeline_pixel_shader_source_size;
} dshp_shared_create_info;

typedef struct dshp_shared dshp_shared;
dshp_shared* dshp_create_shared(dgx_hardware*, const dshp_shared_create_info* info);
void dshp_free_shared(dshp_shared*);

// Shapes Rendering Frame Contextes

typedef struct dshp_frames_create_info {
    dshp_shared*    shared;
    uint32_t        frames_in_flight_count;
} dshp_frames_create_info;

typedef struct dshp_frames dshp_frames;
dshp_frames* dshp_create_frames(dgx_hardware*, const dshp_frames_create_info* info);
void dshp_free_frames(dshp_frames*);

// shorthand not to pass frame in flight to every function
typedef struct dshp_context {
    dshp_frames*    contextes;
    uint32_t        frame_in_flight;
    float           r, g, b, a;
    float           line_thickness;
} dshp_context;

// Actuall Draw Operation

// resets draw requests in context
void dshp_reset(
    dshp_context* context,
    int clear_geometry, 
    int clear_state
);

// uploads draw requests to gpu
void dshp_upload(
    dgx_command_list*   command_list,
    dgx_hardware_queue* queue_for_uploads,
    dshp_context* context,
    dgx_staging_memory* staging_memory,
    uint64_t            staging_memory_region_offset,
    uint64_t            staging_memory_region_size,
    dgx_cpu_signal*     upload_finished_cpu,
    dgx_gpu_signal*     upload_finished_gpu
);

// command to draw from context
// needs to be re recorded every frame as amount of
// drawn primitives may change
// call with render target bound, viewport and scissors set
void dshp_gcmd_render(
    dgx_command_list*   target,
    dshp_context* context
);

// Shapes Draw Function

// set drawn shapes color
void dshp_set_color(
    dshp_context* context,
    float r, float g, float b, float a
);

void dshp_set_line_thickness(
    dshp_context* context,
    float line_thickness
);

void dshp_line(
    dshp_context* context,
    dla_vec2 begin, dla_vec2 end
);

void dshp_triangle(
    dshp_context* context,
    dla_vec2 a, dla_vec2 b, dla_vec2 c
);

void dshp_rect(
    dshp_context* context,
    dla_vec2 first_corner, dla_vec2 second_corner
);

void dshp_circle(
    dshp_context* context,
    dla_vec2 center, float radius
);

#endif // DEMIURG_SHAPES_H

#ifdef DEMIURG_SHAPES_IMPL

#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
    Config
*/

static dgx_vertex_input_attribute_info vertex_attributes[] = {
    {   // position : per vertex
        .binding    = 0,
        .location   = 0,
        .offset     = 0,
        .type       = dgx_data_type_vec2f32
    },
};
static const uint64_t gpu_vertex_sizeof = 2 * 4;

static dgx_vertex_input_binding_info vertex_bindings[] = {
    {
        .binding    = 0,
        .input_rate = dgx_vertex_attribute_input_rate_per_vertex,
        .stride     = gpu_vertex_sizeof
    }
};

static dgx_descriptor_binding descriptor_bindings[] = {
    {   // the instances buffer
        .binding = 0,
        .count   = 1,
        .stages  = dgx_shader_stage_pixel,
        .type    = dgx_descriptor_binding_type_storage_buffer
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

struct dshp_shared {
    dgx_hardware*                       owning_hardware;
    dgx_descriptor_layout*              descriptor_layout;
    dgx_pipeline_descriptors_layout*    pipeline_layout;
    dgx_pipeline*                       pipeline;
};

dshp_shared* dshp_create_shared(dgx_hardware* hardware, const dshp_shared_create_info* info) {
    dshp_shared* shared = calloc(1, sizeof(dshp_shared)); if (!shared) return NULL;
    shared->owning_hardware = hardware;

    // Descriptor Layout
    shared->descriptor_layout = dgx_create_descriptor_layout(hardware, &(dgx_descriptor_layout_create_info){
        .bindings_count = sizeof(descriptor_bindings) / sizeof(dgx_descriptor_binding),
        .bindings = descriptor_bindings
    }); if (!shared->descriptor_layout) goto _fail;

    // Pipeline Layout
    shared->pipeline_layout = dgx_create_pipeline_descriptors_layout(hardware, &(dgx_pipeline_descriptors_layout_create_info){
        .layouts_count = 1, .layouts = &shared->descriptor_layout
    }); if (!shared->pipeline_layout) goto _fail;
    

    // Pipeline Shaders
    dgx_shader* vertex_shader = dgx_create_shader(shared->owning_hardware, &(dgx_shader_create_info){
        .source_size = info->pipeline_vertex_shader_source_size,
        .source_code = info->pipeline_vertex_shader_source_code
    });

    dgx_shader* pixel_shader = dgx_create_shader(shared->owning_hardware, &(dgx_shader_create_info){
        .source_size = info->pipeline_pixel_shader_source_size,
        .source_code = info->pipeline_pixel_shader_source_code
    });

    if (!vertex_shader || !pixel_shader) {
        dgx_free_shader(vertex_shader);
        dgx_free_shader(pixel_shader);
        goto _fail;
    }

    dgx_pipeline_create_info pip_create_info = {
        .render_target_layout   = info->pipeline_render_target_layout,
        .descriptor_layout      = shared->pipeline_layout,

        .vertex_layout          = {
            .attributes_count   = sizeof(vertex_attributes) / sizeof(dgx_vertex_input_attribute_info),
            .attributes         = vertex_attributes,
            .bindings_count     = sizeof(vertex_bindings) / sizeof(dgx_vertex_input_binding_info),
            .bindings           = vertex_bindings,
        },

        .shader_stages  = {
            .vertex = vertex_shader,
            .pixel  = pixel_shader
        },

        .input_assembly = {
            .topology = dgx_primitive_topology_triangle_list
        },

        .rasterizer = {
            .scissor_enable     = 0,
            .depth_clamp_enable = 0,
            .fill_mode          = dgx_fill_mode_solid,
            .cull_mode          = dgx_cull_mode_none
        },

        .blend = {
            .blend_enable   = 1,
            .blend_op       = dgx_blend_op_add,
            .src_factor     = dgx_blend_factor_src_alpha,
            .dst_factor     = dgx_blend_factor_one_minus_src_alpha,
        },

        .depth_stencil = {
            .depth_test_enable      = 0,
            .depth_write_enable     = 0,
            .stencil_test_enable    = 0
        }
    };
    shared->pipeline = dgx_create_pipeline(shared->owning_hardware, &pip_create_info);

    dgx_free_shader(vertex_shader);
    dgx_free_shader(pixel_shader);

    if (!shared->pipeline) goto _fail;
    return shared;

_fail:
    dshp_free_shared(shared); 
    return NULL;
}

void dshp_free_shared(dshp_shared* shared) {
    if (!shared) return;
    dgx_free_pipeline(shared->pipeline);
    dgx_free_pipeline_descriptors_layout(shared->pipeline_layout);
    dgx_free_descriptor_layout(shared->descriptor_layout);
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

typedef struct single_frame {
    // drawing data
    arena           vertex_arena;
    arena           instance_arena;
    uint32_t        triangles_to_draw;

    // gpu
    dgx_descriptor* assigned_descriptor;
    dgx_buffer*     instances_buffer;
    dgx_buffer*     vertices_buffer;
} single_frame;

struct dshp_frames {
    dgx_hardware*               owning_hardware;
    dshp_shared*                owning_shared;
    dgx_descriptor_allocator*   descriptor_allocator;
    uint32_t                    in_flight;
    single_frame*               frames;
};

dgx_buffer* create_buffer(dgx_hardware* hardware, uint64_t size_bytes, dgx_buffer_usage usage) {
    dgx_buffer_create_info vb_create_info = {
        .usage              = usage,
        .size_bytes         = size_bytes,
        .memory_access      = dgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_write,
        .memory_strategy    = dgx_memory_allocation_strategy_paged
    };
    return dgx_create_buffer(hardware, &vb_create_info);
}

void link_frame_descriptor_with_instance_buffer(dgx_hardware* hardware, single_frame* frame) {
    dgx_descriptor_buffer_write_info binfo = {
        .buffer = frame->instances_buffer,
        .offset = 0,
        .length = dgx_buffer_get_size_bytes(frame->instances_buffer)
    };

    dgx_descriptor_write_info write = {
        .descriptor             = frame->assigned_descriptor,
        .binding_type           = dgx_descriptor_binding_type_storage_buffer,
        .binding_index          = 0,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_buffers      = &binfo
    };

    dgx_descriptors_write(hardware, 1, &write);
}

dshp_frames* dshp_create_frames(dgx_hardware* hardware, const dshp_frames_create_info* info) {
    if (!hardware || !info->shared) goto _fail;

    dshp_frames* contextes = calloc(1, sizeof(dshp_frames)); 
    if (!contextes) goto _fail;

    *contextes = (dshp_frames) {
        .owning_hardware    = hardware,
        .owning_shared      = info->shared,
        .in_flight          = info->frames_in_flight_count
    };

    // Descriptors Allocator
    contextes->descriptor_allocator = dgx_create_descriptor_allocator(hardware, &(dgx_descriptor_allocator_create_info){
        .descriptor_layout = info->shared->descriptor_layout,
        .max_descriptors_allocated = info->frames_in_flight_count
    }); if (!contextes->descriptor_allocator) goto _fail;
    
    // Frames
    contextes->frames = calloc(info->frames_in_flight_count, sizeof(single_frame));
    if (!contextes->frames) goto _fail;
    for (uint32_t i = 0; i < info->frames_in_flight_count; i++) {
        single_frame* frame = &contextes->frames[i];

        *frame = (single_frame){
            .vertex_arena   = alloc_arena(initial_vertices_buffer_cap),
            .instance_arena = alloc_arena(initial_instances_buffer_cap),
            
            .vertices_buffer     = create_buffer(hardware, initial_vertices_buffer_cap,  dgx_buffer_usage_vertex),
            .instances_buffer    = create_buffer(hardware, initial_instances_buffer_cap, dgx_buffer_usage_storage),

            .assigned_descriptor = dgx_descriptor_allocator_alloc_descriptor(contextes->descriptor_allocator)
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
    dshp_free_frames(contextes);
    return NULL;
}

void dshp_free_frames(dshp_frames* contextes) {
    if (!contextes) return;

    for (uint32_t i = 0; i < contextes->in_flight; i++) {
        dgx_free_buffer(contextes->frames[i].vertices_buffer);
        dgx_free_buffer(contextes->frames[i].instances_buffer);
        free_arena(contextes->frames[i].vertex_arena);
        free_arena(contextes->frames[i].instance_arena);
        // descriptors free with allocator
    }

    dgx_free_descriptor_allocator(contextes->descriptor_allocator);
    free(contextes->frames);
    free(contextes);
}

void dshp_reset(dshp_context* context, int clear_geometry, int clear_state) {
    single_frame* frame = &context->contextes->frames[context->frame_in_flight];

    if (clear_geometry) {
        frame->vertex_arena.position = 0;
        frame->instance_arena.position = 0;
        frame->triangles_to_draw = 0;
    }

    if (clear_state) {
        context->r = 1; context->g = 1; context->b = 1; context->a = 1;
        context->line_thickness = 0.01;
    }
}

// returns bytes of data that can be copied
// buffer at *buffer may be recreated due to call
uint32_t ensure_buffer_size_pre_upload
(dgx_hardware* hardware, dgx_buffer** buffer, arena* a, dgx_buffer_usage buffer_usage, int* was_buffer_recreated) {
    *was_buffer_recreated = 0;

    if (dgx_buffer_get_size_bytes(*buffer) < a->position) {
        dgx_buffer* new_buffer = create_buffer(hardware, a->capacity, buffer_usage);

        // stick to old buffer, since creation of new failed
        if (!new_buffer) return dgx_buffer_get_size_bytes(*buffer);
        else {
            *was_buffer_recreated = 1;
            dgx_free_buffer(*buffer);
            *buffer = new_buffer;
        }
    }

    return a->position;
}

void dshp_upload(
    dgx_command_list*   command_list,
    dgx_hardware_queue* queue_for_uploads,
    dshp_context* context,
    dgx_staging_memory* staging_memory,
    uint64_t            staging_memory_region_offset,
    uint64_t            staging_memory_region_size,
    dgx_cpu_signal*     upload_finished_cpu,
    dgx_gpu_signal*     upload_finished_gpu
) {
    int provided_cpu_signal = upload_finished_cpu && 1;

    dgx_hardware*  hardware = context->contextes->owning_hardware;
    single_frame* frame = &context->contextes->frames[context->frame_in_flight];

    int v_buffer_recreated = 0;
    uint32_t v_to_copy = ensure_buffer_size_pre_upload(
        hardware, &frame->vertices_buffer, &frame->vertex_arena, dgx_buffer_usage_vertex, &v_buffer_recreated
    );

    int i_buffer_recreated = 0;
    uint32_t i_to_copy = ensure_buffer_size_pre_upload(
        hardware, &frame->instances_buffer, &frame->instance_arena, dgx_buffer_usage_storage, &i_buffer_recreated
    ); if (i_buffer_recreated) link_frame_descriptor_with_instance_buffer(hardware, frame);

    uint32_t v_copy_position = 0;
    uint32_t i_copy_position = 0;
    
    char* v_data = (char*)frame->vertex_arena.data;
    char* i_data = (char*)frame->instance_arena.data;

    while (v_copy_position < v_to_copy || i_copy_position < i_to_copy) {
        uint32_t v_bytes = 0;
        uint32_t i_bytes = 0;

        // Copy memory to staging
        char* mapped = dgx_staging_memory_map(staging_memory, staging_memory_region_offset, staging_memory_region_size);
            // Copy as much vertex data as possible
            v_bytes = v_to_copy - v_copy_position;
            if (v_bytes > staging_memory_region_size) v_bytes = staging_memory_region_size;
            memcpy(mapped, v_data + v_copy_position, v_bytes);

            // Copy as much instance data as possible
            i_bytes = i_to_copy - i_copy_position;
            if (i_bytes > staging_memory_region_size - v_bytes) i_bytes = staging_memory_region_size - v_bytes;
            memcpy(mapped + v_bytes, i_data + i_copy_position, i_bytes);
        dgx_staging_memory_unmap(staging_memory);

        // Record command list for target buffer rewrite
        dgx_begin_command_list_recording(command_list);
            if (v_bytes) dgx_cmd_copy_staging_memory_to_buffer(
                command_list, 
                staging_memory, 
                frame->vertices_buffer, 
                staging_memory_region_offset + 0, 
                v_copy_position, 
                v_bytes
            );
            
            if (i_bytes) dgx_cmd_copy_staging_memory_to_buffer(
                command_list, 
                staging_memory, 
                frame->instances_buffer, 
                staging_memory_region_offset + v_bytes,
                i_copy_position, 
                i_bytes
            );
        dgx_finish_command_list_recording(command_list);

        // Advance and find out whether is last batch
        v_copy_position += v_bytes;
        i_copy_position += i_bytes;
        int is_final_batch = !(v_copy_position < v_to_copy || i_copy_position < i_to_copy);

        // If not final batch, but multiple have to be sent
        // Ensure we have cpu signal
        if (!is_final_batch && upload_finished_cpu == NULL) {
            upload_finished_cpu = dgx_create_cpu_signal(hardware, &(dgx_cpu_signal_create_info){.initialy_signaled = 0});
        }

        // Submit
        dgx_submit_info submit = {
            .command_lists_count        = 1,
            .command_lists              = &command_list,
            .cpu_signal                 = upload_finished_cpu,
            .signal_gpu_signals_count   = (is_final_batch && upload_finished_gpu != NULL) ? 1 : 0,
            .signal_gpu_signals         = &upload_finished_gpu
        };

        dgx_submit_command_list(queue_for_uploads, &submit);

        if (!is_final_batch) {
            dgx_cpu_signal_wait(upload_finished_cpu);
            dgx_cpu_signal_reset(upload_finished_cpu);
            continue;
        }
    }

    // Cleanup - free allocated cpu signal if exist
    if (!provided_cpu_signal && upload_finished_cpu) {
        dgx_cpu_signal_wait(upload_finished_cpu);
        dgx_free_cpu_signal(upload_finished_cpu);
    }
}

void dshp_gcmd_render(
    dgx_command_list*           target,
    dshp_context*   context
) {
    single_frame* frame = &context->contextes->frames[context->frame_in_flight];
    if (frame->triangles_to_draw) {
        dgx_gcmd_bind_graphics_pipeline(target, context->contextes->owning_shared->pipeline);
        dgx_gcmd_bind_graphics_pipeline_vertex_buffer(target, frame->vertices_buffer, 0, 0);
        dgx_gcmd_bind_graphics_pipeline_descriptors(
            target,
            context->contextes->owning_shared->pipeline_layout,
            0, 1, &frame->assigned_descriptor
        );
        dgx_gcmd_draw_vertices(target, frame->triangles_to_draw * 3, 0, 1, 0);
    }
}

// Methods

static inline void emit_triangle(
    dshp_context* context, 
    float x0,   float y0,
    float x1,   float y1,
    float x2,   float y2,
    float rcx,  float rcy, // radius center
    float radius
) {
    single_frame* frame = &context->contextes->frames[context->frame_in_flight];

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
        .r          = context->r, 
        .g          = context->g, 
        .b          = context->b, 
        .a          = context->a,
        .center_x   = rcx, 
        .center_y   = rcy,
        .radius     = radius,
    };

    // Emit triangle
    frame->triangles_to_draw++;
};

void dshp_set_color(
    dshp_context* context,
    float r, float g, float b, float a
) {
    context->r = r; context->g = g; context->b = b; context->a = a;
}

void dshp_set_line_thickness(
    dshp_context* context,
    float line_thickness
) {
    context->line_thickness = line_thickness;
}

void dshp_line(
    dshp_context* context,
    dla_vec2 begin, dla_vec2 end
) {
    single_frame* frame = &context->contextes->frames[context->frame_in_flight];

    float dx = end.x - begin.x;
    float dy = end.y - begin.y;

    float len = sqrtf(dx * dx + dy * dy);
    if (len == 0.0f) return;

    float nx = -dy / len;
    float ny =  dx / len;

    float ox = nx * context->line_thickness * 0.5f;
    float oy = ny * context->line_thickness * 0.5f;

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

void dshp_triangle(
    dshp_context* context,
    dla_vec2 a, dla_vec2 b, dla_vec2 c
) {
    emit_triangle(context, a.x, a.y, b.x, b.y, c.x, c.y, 0, 0, -1.0f); // unrounded
}

void dshp_rect(
    dshp_context* context,
    dla_vec2 first_corner, dla_vec2 second_corner
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

void dshp_circle(
    dshp_context* context,
    dla_vec2 center, float radius
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

#endif // DEMIURG_SHAPES_IMPL
