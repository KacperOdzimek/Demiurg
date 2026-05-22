/*
----------------------------------------------------------------
Contents:
This file implements lgx rendering pipeline for lui ui system.

----------------------------------------------------------------
Code info:
- luirp prefix
- LIGHT_USER_INTERFACE_RENDERING_PIPELINE_IMPL macro to build
- graphics.h dependant
- user_interface.h dependant
- font.h dependant

----------------------------------------------------------------
Usage:
- Define luirp_injection_query_image and luirp_injection_query_font functions
    for performance, those functions are declared as inline and 
    must be declared in same compilation unit as library implementation 
- Create shared object, and frame contextes per window
- Per frame: upload ui with luirp_upload_ui, then render ui with luirp_gcmd_render_ui

----------------------------------------------------------------
Possible Additions:
- Text Kerning
- Shader dispatch flags

----------------------------------------------------------------
Possible Optimizations:
- Caching of generated text instances
- Rework of texture bindings - cache textures hashmap inside frame, bind only changed
*/

/* 
    Injections:
*/
#ifdef LIGHT_USER_INTERFACE_RENDERING_PIPELINE_IMPL
    #include "light/graphics.h"
    #include "light/user_interface.h"
    #include "light/font.h"

    typedef struct luirp_atlas_position_uv {
        float uv_min_x, uv_min_y;
        float uv_max_x, uv_max_y;
    } luirp_atlas_position_uv;

    // texture can be set to NULL if query fails
    static inline void luirp_injection_query_image(
        const lui_image_data*       data, 
        lgx_texture**               texture,
        luirp_atlas_position_uv*    atlas_position
    );

    // font can be set to NULL if query fails
    static inline void luirp_injection_query_font(
        const lui_text_data*    data,
        lfont**                 font
    );
#endif // LIGHT_USER_INTERFACE_RENDERING_PIPELINE_IMPL

#ifndef LIGHT_USER_INTERFACE_RENDERING_PIPELINE_H
#define LIGHT_USER_INTERFACE_RENDERING_PIPELINE_H

#include "light/graphics.h"
#include "light/user_interface.h"

// UI Rendering Shared Objects
// Make one instance of this object per hardware
// This object contains only read only objects like pipeline, so you can reuse it across
//  multiple windows, frames in flight, etc

typedef struct luirp_shared_create_info {
    lgx_render_target_layout*   pipeline_render_target_layout;

    const char*                 pipeline_vertex_shader_source_code;
    uint32_t                    pipeline_vertex_shader_source_size;

    const char*                 pipeline_pixel_shader_source_code;
    uint32_t                    pipeline_pixel_shader_source_size;

    uint32_t                    additional_pipeline_descriptors_layouts_count;
    lgx_descriptor_layout**     additional_pipeline_descriptors_layouts;
} luirp_shared_create_info;

typedef struct luirp_shared luirp_shared;
luirp_shared* luirp_create_shared(lgx_hardware*, const luirp_shared_create_info* info);
void luirp_free_shared(luirp_shared*);

// UI Rendering Frame Contextes
// Make one instance of this object per ui system you will render at the time
// This object contains all per-frame objects like descriptors. It is also tied to shared object, and must live shorter than it,

typedef struct luirp_frames_contextes_create_info {
    luirp_shared*   shared;
    uint32_t        frames_in_flight_count;
} luirp_frames_contextes_create_info;

typedef struct luirp_frames_contextes luirp_frames_contextes;

luirp_frames_contextes* luirp_create_frames_contextes(lgx_hardware*, const luirp_frames_contextes_create_info* info);
void luirp_free_frames_contextes(luirp_frames_contextes*);

// UI Rendering Functions

void luirp_upload_ui(
    lgx_command_list*       command_list,
    lgx_hardware_queue*     queue_for_uploads,
    luirp_frames_contextes* contextes,
    uint32_t                frame_in_flight_index,
    lgx_staging_memory*     staging_memory,
    uint64_t                staging_memory_region_offset,
    uint64_t                staging_memory_region_size,
    lgx_cpu_signal*         upload_finished_cpu,
    lgx_gpu_signal*         upload_finished_gpu,
    lui_arena*              draws_arena,
    lui_arena*              clips_arena
);

// with render target bound
// with viewport and scissors set
void luirp_gcmd_render_ui(
    lgx_command_list*       list,
    luirp_frames_contextes* contextes,
    uint32_t                frame_in_flight_index
);

#endif // LIGHT_USER_INTERFACE_RENDERING_PIPELINE_H

#ifdef LIGHT_USER_INTERFACE_RENDERING_PIPELINE_IMPL
#define LIGHT_USER_INTERFACE_RENDERING_PIPELINE_IMPL

#include <stdlib.h>
#include <string.h>

// Constants

static const int initial_instances_buffer_bytes = 1024 * 1024;
static const int initial_clips_buffer_bytes     = 1024;
static const int internal_textures_limit        = 2024;

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
        .stages  = lgx_shader_stage_vertex | lgx_shader_stage_pixel,
        .type    = lgx_descriptor_binding_type_storage_buffer
    },
    {   // the clips buffer
        .binding = 1,
        .count   = 1,
        .stages  = lgx_shader_stage_pixel,
        .type    = lgx_descriptor_binding_type_storage_buffer
    },
    {   // the sampler
        .binding = 2,
        .count   = 1,
        .stages  = lgx_shader_stage_pixel,
        .type    = lgx_descriptor_binding_type_sampler,
    },
    {   // the textures
        .binding = 3,
        .count   = -1, // Needs to be set hardware!
        .stages  = lgx_shader_stage_pixel,
        .type    = lgx_descriptor_binding_type_sampled_texture
    }
};

typedef struct gpu_instance {
    // Box transform affine3x2 matrix
    lui_transform transform;

    // Texture region uv, for atlasing
    luirp_atlas_position_uv    atlas_position;

    // Texture index within descriptor
    // Offseted by one in both dimensions - value of 0 means no texture
    // If positive texture is read as color texture, else as a font atlas
    int texture_index;

    // The clip index within clipbox buffer
    // -1 means no clip
    int clip_index;

    // Tint
    float r, g, b, a;

    // Shader effect index
    uint32_t shader;
} gpu_instance;

typedef struct gpu_clipbox {
    lui_transform clipbox_transform;
} gpu_clipbox;

// Shared Object

struct luirp_shared {
    lgx_hardware*                       owning_hardware;

    lgx_buffer*                         vertex_buffer;
    lgx_sampler*                        sampler;

    uint32_t                            descriptor_textures_array_length;
    lgx_descriptor_layout*              descriptor_layout;
    lgx_pipeline_descriptors_layout*    pipeline_descriptor_layout;

    lgx_pipeline*                       pipeline;
};

luirp_shared* luirp_create_shared(lgx_hardware* hardware, const luirp_shared_create_info* info) {
    luirp_shared* shared = calloc(1, sizeof(luirp_shared)); if (!shared) return NULL;
    shared->owning_hardware = hardware;

    // Vertex Buffer
    lgx_buffer_create_info vb_create_info = {
        .usage              = lgx_buffer_usage_vertex,
        .size_bytes         = sizeof(quad_vertices_array),
        .memory_access      = lgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_write,
        .memory_strategy    = lgx_memory_allocation_strategy_dedicated
    };
    shared->vertex_buffer = lgx_create_buffer(hardware, &vb_create_info);
    if (!shared->vertex_buffer) goto _fail;

    lgx_buffer_sync_upload(shared->vertex_buffer, 0, quad_vertices_array, sizeof(quad_vertices_array));

    // Query textures limit
    uint32_t max_textures = lgx_hardware_query_limit(hardware, lgx_hardware_limit_max_descriptor_sampled_images);
    shared->descriptor_textures_array_length = max_textures > internal_textures_limit ? internal_textures_limit : max_textures;

    // Copy descriptor bindings info
    uint32_t bindings_count = sizeof(descriptor_bindings) / sizeof(lgx_descriptor_binding);
    lgx_descriptor_binding* bindings = malloc(bindings_count * sizeof(lgx_descriptor_binding)); 
    if (!bindings) goto _fail;
    memcpy(bindings, descriptor_bindings, sizeof(descriptor_bindings));

    // Overwrite textures limit
    bindings[3].count = shared->descriptor_textures_array_length;

    // Descriptor Layout
    lgx_descriptor_layout_create_info dl_create_info = {
        .bindings_count = bindings_count,
        .bindings       = bindings
    };
    shared->descriptor_layout = lgx_create_descriptor_layout(hardware, &dl_create_info);
    free(bindings);
    if (!shared->descriptor_layout) goto _fail;

    // Pipeline Descriptor Layout
    uint32_t layouts_count = 1 + info->additional_pipeline_descriptors_layouts_count;
    lgx_descriptor_layout** layouts = calloc(layouts_count, sizeof(lgx_descriptor_layout*));

    layouts[0] = shared->descriptor_layout;
    for (uint32_t i = 0; i < info->additional_pipeline_descriptors_layouts_count; i++) {
        layouts[i + 1] = info->additional_pipeline_descriptors_layouts[i];
    }

    lgx_pipeline_descriptors_layout_create_info pdl_create_info = {
        .layouts_count  = layouts_count,
        .layouts        = layouts
    };
    shared->pipeline_descriptor_layout = lgx_create_pipeline_descriptors_layout(hardware, &pdl_create_info);
    free(layouts); if (!shared->pipeline_descriptor_layout) goto _fail;

    // Sampler
    lgx_sampler_create_info s_create_info = {
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
    };
    shared->sampler = lgx_create_sampler(hardware, &s_create_info);
    if (!shared->sampler) goto _fail;

    // Pipeline Shaders
    lgx_shader_create_info sci;
    sci.source_size = info->pipeline_vertex_shader_source_size;
    sci.source_code = info->pipeline_vertex_shader_source_code;
    lgx_shader* vertex_shader = lgx_create_shader(shared->owning_hardware, &sci);

    sci.source_size = info->pipeline_pixel_shader_source_size;
    sci.source_code = info->pipeline_pixel_shader_source_code;
    lgx_shader* pixel_shader = lgx_create_shader(shared->owning_hardware, &sci);

    if (!vertex_shader || !pixel_shader) {
        lgx_free_shader(vertex_shader);
        lgx_free_shader(pixel_shader);
        goto _fail;
    }

    // Pipeline
    lgx_pipeline_create_info pip_create_info = {
        .render_target_layout   = info->pipeline_render_target_layout,
        .descriptor_layout      = shared->pipeline_descriptor_layout,

        .vertex_layout = {
            .attributes_count   = sizeof(vertex_attributes) / sizeof(lgx_vertex_input_attribute_info),
            .attributes         = vertex_attributes,
            .bindings_count     = sizeof(vertex_bindings) / sizeof(lgx_vertex_input_binding_info),
            .bindings           = vertex_bindings,
        },

        .shader_stages = {
            .vertex = vertex_shader,
            .pixel  = pixel_shader
        },

        .input_assembly = {
            .topology = lgx_primitive_topology_triangle_strip
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

    // Free shaders
    lgx_free_shader(vertex_shader);
    lgx_free_shader(pixel_shader);

    if (!shared->pipeline) goto _fail;

    return shared;

_fail:
    luirp_free_shared(shared);
    return NULL;
}

void luirp_free_shared(luirp_shared* shared) {
    if (!shared) return;
    lgx_free_sampler(shared->sampler);
    lgx_free_buffer(shared->vertex_buffer);
    lgx_free_pipeline(shared->pipeline);
    lgx_free_pipeline_descriptors_layout(shared->pipeline_descriptor_layout);
    lgx_free_descriptor_layout(shared->descriptor_layout);
    free(shared);
}

// Frames Contextes Object

typedef struct frame_context {
    uint32_t        instances_to_render;
    lgx_buffer*     clips_buffer;
    lgx_buffer*     instances_buffer;
    lgx_descriptor* descriptor;
} frame_context;

struct luirp_frames_contextes {
    luirp_shared*               owning_shared;
    lgx_descriptor_allocator*   descriptor_allocator;
    uint32_t                    contextes_count;
    frame_context*              contextes;
};

lgx_buffer* create_instance_or_clips_buffer(lgx_hardware* hardware, uint64_t bytes) {
    lgx_buffer_create_info buffer_create_info = {
        .size_bytes         = bytes,
        .usage              = lgx_buffer_usage_storage,
        .memory_strategy    = lgx_memory_allocation_strategy_dedicated,
        .memory_access      = lgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_write
    };

    return lgx_create_buffer(hardware, &buffer_create_info);
}

void link_sampler_to_descriptor(lgx_hardware* hardware, lgx_sampler* shared_sampler, frame_context* frame) {
    lgx_descriptor_write_info writes[1];

    lgx_descriptor_sampler_write_info sinfo = {
        .sampler = shared_sampler
    }; 

    writes[0] = (lgx_descriptor_write_info){
        .descriptor             = frame->descriptor,
        .binding_type           = lgx_descriptor_binding_type_sampler,
        .binding_index          = 2,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_samplers     = &sinfo
    };

    lgx_descriptors_write(hardware, 1, writes);
}

void link_buffers_to_descriptor(lgx_hardware* hardware, frame_context* frame) {
    lgx_descriptor_write_info           writes[2];
    lgx_descriptor_buffer_write_info    binfo[2];

    binfo[0] = (lgx_descriptor_buffer_write_info){
        .buffer = frame->instances_buffer,
        .length = lgx_buffer_get_size_bytes(frame->instances_buffer),
        .offset = 0
    };

    writes[0] = (lgx_descriptor_write_info){
        .descriptor             = frame->descriptor,
        .binding_type           = lgx_descriptor_binding_type_storage_buffer,
        .binding_index          = 0,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_buffers      = &binfo[0]
    };

    binfo[1] = (lgx_descriptor_buffer_write_info){
        .buffer = frame->clips_buffer,
        .length = lgx_buffer_get_size_bytes(frame->clips_buffer),
        .offset = 0
    };

    writes[1] = (lgx_descriptor_write_info){
        .descriptor             = frame->descriptor,
        .binding_type           = lgx_descriptor_binding_type_storage_buffer,
        .binding_index          = 1,
        .array_element_index    = 0,
        .array_elements_count   = 1,
        .infos.for_buffers      = &binfo[1]
    };

    lgx_descriptors_write(hardware, 2, writes);
}

luirp_frames_contextes* luirp_create_frames_contextes
(lgx_hardware* hardware, const luirp_frames_contextes_create_info* info) {
    luirp_shared* shared = info->shared;

    luirp_frames_contextes* contextes = calloc(1, sizeof(luirp_frames_contextes)); 
    if (!contextes) return NULL;
    
    contextes->owning_shared = shared;
    
    // create descriptor allocator
    lgx_descriptor_allocator_create_info create_info = {
        .descriptor_layout          = shared->descriptor_layout,
        .max_descriptors_allocated  = info->frames_in_flight_count
    };
    contextes->descriptor_allocator = lgx_create_descriptor_allocator(hardware, &create_info);
    if (!contextes->descriptor_allocator) goto _fail;

    // create frames
    contextes->contextes_count = info->frames_in_flight_count;
    contextes->contextes = calloc(info->frames_in_flight_count, sizeof(frame_context));
    if (!contextes->contextes) goto _fail;

    // populate frames
    for (uint32_t i = 0; i < info->frames_in_flight_count; i++) {
        frame_context* frame    = &contextes->contextes[i];
        frame->descriptor       = lgx_descriptor_allocator_alloc_descriptor(contextes->descriptor_allocator);
        frame->instances_buffer = create_instance_or_clips_buffer(hardware, initial_instances_buffer_bytes);
        frame->clips_buffer     = create_instance_or_clips_buffer(hardware, initial_clips_buffer_bytes);

        if (!frame->descriptor || !frame->instances_buffer || !frame->clips_buffer) goto _fail;

        link_buffers_to_descriptor(hardware, frame);
        link_sampler_to_descriptor(hardware, shared->sampler, frame);
    }

    return contextes;
_fail:
    luirp_free_frames_contextes(contextes);
    return NULL;
}

void luirp_free_frames_contextes(luirp_frames_contextes* contextes) {
    if (!contextes) return;

    for (uint32_t i = 0; i < contextes->contextes_count; i++) {
        frame_context* frame = &contextes->contextes[i];
        lgx_free_buffer(frame->instances_buffer);
        lgx_free_buffer(frame->clips_buffer);
    }

    // all per-frame descriptors freed with allocator
    lgx_free_descriptor_allocator(contextes->descriptor_allocator);
    free(contextes->contextes);

    free(contextes);
}

// UI upload

typedef struct upload_state {
    // staging constants

    lgx_staging_memory* staging_memory;
    uint32_t            staging_offset;
    uint32_t            staging_size;

    // staging changing

    uint64_t staging_memory_left;

    // clips

    uint32_t clips_cursor;
    uint32_t all_clips;

    uint32_t clips_buffer_offset;
    uint32_t staging_clips_bytes;

    // draws

    uint32_t draws_cursor;
    uint32_t all_draws;

    uint32_t instance_buffer_offset;
    uint32_t staging_instances_bytes;

    uint32_t all_instances_to_draw;

    // images hashmap
    uint32_t      textures_array_length;
    lgx_texture** textures_hashmap;
} upload_state;

int get_texture_index(upload_state* state, lgx_texture* texture, int font) {
    if (texture == NULL) return 0; // safe fallback

    // start search at module of texture pointer, bit shift because pointers may be aligned, will often lead to same slot
    uint64_t hash = ((size_t)texture >> 4) * 11400714819323198485llu;
    uint64_t begin = hash % state->textures_array_length;

    // search for free slot
    int itr = begin;
    do {
        // same texture already assigned, return
        if (state->textures_hashmap[itr] == texture) {
            if (font) return -itr - 1;
            return itr + 1;
        }
        // free slot, assing
        if (state->textures_hashmap[itr] == NULL) {
            state->textures_hashmap[itr] = texture;
            if (font) return -itr - 1;
            return itr + 1;
        }
        // else continue search
        itr = (itr + 1) % state->textures_array_length;
    } while(itr != begin);

    // no empty slots left
    return 0;
}

// returns new mapped (staging memory position), NULL means memory shortage
char* process_clip(char* mapped, upload_state* state, lui_arena* clips) {
    if (state->staging_memory_left < sizeof(gpu_clipbox)) return NULL;

    lui_transform* clipboxes = (lui_transform*)clips->memory;
    *((gpu_clipbox*)(mapped)) = (gpu_clipbox){
        .clipbox_transform = clipboxes[state->clips_cursor]
    };
    
    state->clips_cursor++;
    state->staging_memory_left -= sizeof(gpu_clipbox);
    state->staging_clips_bytes += sizeof(gpu_clipbox);
    return mapped + sizeof(gpu_clipbox);
}

// returns new mapped (staging memory position), NULL means memory shortage
static inline char* push_instance(char* mapped, upload_state* state, gpu_instance inst) {
    if (state->staging_memory_left < sizeof(gpu_instance)) return NULL;
    gpu_instance* as_inst = (gpu_instance*)mapped;

    *as_inst = inst;

    state->all_instances_to_draw++;
    state->staging_memory_left -= sizeof(gpu_instance);
    state->staging_instances_bytes += sizeof(gpu_instance);
    return mapped + sizeof(gpu_instance);
}

// returns new mapped (staging memory position), NULL means memory shortage
char* process_draw_command(char* mapped, upload_state* state, lui_arena* draws) {
    lui_draw_command* cmd = &((lui_draw_command*)(draws->memory))[state->draws_cursor];
    gpu_instance* inst = (gpu_instance*)mapped;
    state->draws_cursor++;

    if (cmd->type == lui_draw_box) {
        return push_instance(mapped, state, (gpu_instance){
            .clip_index = cmd->clipbox_index,
            .transform  = cmd->transform,
            .r = (float)cmd->box_data.color.r / 255.0f,
            .g = (float)cmd->box_data.color.g / 255.0f,
            .b = (float)cmd->box_data.color.b / 255.0f,
            .a = (float)cmd->box_data.color.a / 255.0f,
            .texture_index = 0, // no texture
            .shader = cmd->box_data.shader,
        });
    }
    else if (cmd->type == lui_draw_image) {
        if (state->staging_memory_left < sizeof(gpu_instance)) return NULL;
        
        lgx_texture* the_texture = NULL;
        luirp_atlas_position_uv atlas_position = {
            .uv_min_x = 0, .uv_min_y = 0,
            .uv_max_x = 1, .uv_max_y = 1
        };
        luirp_injection_query_image(
            &cmd->image_data, &the_texture, &atlas_position
        );

        return push_instance(mapped, state, (gpu_instance){
            .clip_index = cmd->clipbox_index,
            .transform  = cmd->transform,
            .r = (float)cmd->image_data.tint.r / 255.0f,
            .g = (float)cmd->image_data.tint.g / 255.0f,
            .b = (float)cmd->image_data.tint.b / 255.0f,
            .a = (float)cmd->image_data.tint.a / 255.0f,
            .texture_index = get_texture_index(state, the_texture, 0),
            .atlas_position = atlas_position,
            .shader = cmd->image_data.shader
        });
    }
    else if (cmd->type == lui_draw_text) {
        lui_text_data data = cmd->text_data;
        if (!data.font || !data.text) return mapped;

        lfont* font = NULL;
        luirp_injection_query_font(
            &data, &font
        );

        if (font == NULL) return mapped;

        gpu_instance default_instance = {
            .clip_index = cmd->clipbox_index,
            .transform  = cmd->transform,
            .r = (float)cmd->text_data.tint.r / 255.0f,
            .g = (float)cmd->text_data.tint.g / 255.0f,
            .b = (float)cmd->text_data.tint.b / 255.0f,
            .a = (float)cmd->text_data.tint.a / 255.0f,
            .texture_index = get_texture_index(state, lfont_get_texture(font), 1),
            .shader = cmd->text_data.shader
        };

        // in below code, we multiply font metrics by two, since
        // normalized coord system spans two units (-1 to 1)

        // translate offsets from base font to our instance of text size
        float font_scale    = (float)data.size / lfont_get_base_size(font);

        float font_line_height  = (lfont_get_base_ascent(font) - lfont_get_base_descent(font)) * font_scale * 2;
        float font_line_gap     = lfont_get_base_line_gap(font) * font_scale * 2;

        float pixel_to_norm_x = 2.0f / cmd->pixels_width;
        float pixel_to_norm_y = 2.0f / cmd->pixels_height;

        // in pixel draw cursors
        float cursor_x = 0;                     // pen
        float cursor_y = font_line_height;  // baseline

        size_t   itr = 0;
        while (data.text[itr] != '\0') {
            uint32_t codepoint; itr += lfont_utf8_decode(data.text, itr, &codepoint);
            lfont_glyph glyph = lfont_get_glyph(font, codepoint);

            if (codepoint == '\n') {
                cursor_x = 0;
                cursor_y += font_line_height;
                cursor_y += font_line_gap;
                continue;
            }

            lui_transform glyph_local_transform = lui_default_trans;

            // scale : full extend to font pixel size
            glyph_local_transform = lui_sca(
                glyph_local_transform,
                (glyph.size_x * font_scale) * pixel_to_norm_x / 2,
                (glyph.size_y * font_scale) * pixel_to_norm_y / 2
            );

            // transform
            // move letter rect to center of rect (cursor_x, cursor_y) -> (cursor_x + glyph.size_x, cursor_y + glyph.size_y)
            glyph_local_transform.tx = -1 + ((float)(cursor_x + glyph.size_x * font_scale) / 2.0f) * pixel_to_norm_x;
            glyph_local_transform.ty =  1 - ((float)(cursor_y + glyph.size_y * font_scale) / 2.0f) * pixel_to_norm_y;

            // offset by bearings
            glyph_local_transform.tx += (glyph.bearing_x * font_scale) * pixel_to_norm_x;
            glyph_local_transform.ty -= (glyph.bearing_y * font_scale) * pixel_to_norm_y;

            gpu_instance glyph_instance = default_instance;

            glyph_instance.atlas_position = (luirp_atlas_position_uv){
                glyph.uv_min_x, glyph.uv_max_y,
                glyph.uv_max_x, glyph.uv_min_y // swapped for now flip font on load
            };

            glyph_instance.transform = lui_mul(
                cmd->transform,
                glyph_local_transform
            );

            mapped = push_instance(mapped, state, glyph_instance);
            if (!mapped) return NULL;

            cursor_x += (glyph.advance_x * font_scale) * 2;
        }

        return mapped;
    }

    // omit invalid draws
    return mapped;
}

void record_upload(lgx_command_list* list, frame_context* frame, upload_state* state) {
    lgx_begin_command_list_recording(list);

    if (state->staging_clips_bytes) {
        lgx_cmd_copy_staging_memory_to_buffer(
            list, state->staging_memory, frame->clips_buffer,
            state->staging_offset, state->clips_buffer_offset, state->staging_clips_bytes
        );

        state->clips_buffer_offset += state->staging_clips_bytes;
    }
    
    if (state->staging_instances_bytes) {
        lgx_cmd_copy_staging_memory_to_buffer(
            list, state->staging_memory, frame->instances_buffer,
            state->staging_offset + state->staging_clips_bytes, state->instance_buffer_offset, state->staging_instances_bytes
        );

        state->instance_buffer_offset += state->staging_instances_bytes;
    }

    state->staging_memory_left = state->staging_size;
    state->staging_instances_bytes = 0;
    state->staging_clips_bytes = 0;

    lgx_finish_command_list_recording(list);
}

void luirp_upload_ui(
    lgx_command_list*       command_list,
    lgx_hardware_queue*     queue_for_uploads,
    luirp_frames_contextes* contextes,
    uint32_t                frame_in_flight_index,
    lgx_staging_memory*     staging_memory,
    uint64_t                staging_memory_region_offset,
    uint64_t                staging_memory_region_size,
    lgx_cpu_signal*         upload_finished_cpu,
    lgx_gpu_signal*         upload_finished_gpu,
    lui_arena*              draws_arena,
    lui_arena*              clips_arena
) {
    if (upload_finished_cpu) lgx_cpu_signal_reset(upload_finished_cpu);
    
    int      provided_cpu_signal   = 1 && upload_finished_cpu;
    uint32_t textures_array_length = contextes->owning_shared->descriptor_textures_array_length;

    frame_context* frame = &contextes->contextes[frame_in_flight_index % contextes->contextes_count];
    upload_state state = {
        .staging_memory = staging_memory,
        .staging_offset = staging_memory_region_offset,
        .staging_size   = staging_memory_region_size,

        .staging_memory_left = staging_memory_region_size,

        .staging_clips_bytes = 0,
        .staging_instances_bytes = 0,

        .clips_cursor = 0,
        .draws_cursor = 0,

        .all_clips = clips_arena->position / sizeof(lui_transform),
        .all_draws = draws_arena->position / sizeof(lui_draw_command),
        
        .all_instances_to_draw = 0,

        .textures_array_length  = textures_array_length,
        .textures_hashmap       = calloc(textures_array_length, sizeof(lgx_texture*))
    };

    // upload instances and clips
    while (1) {
        char* mapped = lgx_staging_memory_map(staging_memory, staging_memory_region_offset, staging_memory_region_size);

        while (state.clips_cursor < state.all_clips) {
            mapped = process_clip(mapped, &state, clips_arena);
            if (!mapped) break;
        }

        while (state.draws_cursor < state.all_draws) {
            mapped = process_draw_command(mapped, &state, draws_arena);
            if (!mapped) break;
        }

        lgx_staging_memory_unmap(staging_memory);

        int final_batch = !(state.clips_cursor < state.all_clips || state.draws_cursor < state.all_draws);
        record_upload(command_list, frame, &state);

        if (!final_batch && upload_finished_cpu == NULL) {
            upload_finished_cpu = lgx_create_cpu_signal(
                contextes->owning_shared->owning_hardware, &(lgx_cpu_signal_create_info){.initialy_signaled = 0}
            );
        }

        lgx_submit_info submit = {
            .command_lists_count        = 1,
            .command_lists              = &command_list,
            .cpu_signal                 = upload_finished_cpu,
            .signal_gpu_signals_count   = (final_batch && upload_finished_gpu != NULL) ? 1 : 0,
            .signal_gpu_signals         = &upload_finished_gpu
        };

        lgx_submit_command_list(queue_for_uploads, &submit);

        if (!final_batch) {
            lgx_cpu_signal_wait(upload_finished_cpu);
            lgx_cpu_signal_reset(upload_finished_cpu);
            continue;
        }

        break;
    }

    // TODO : UPDATE ONLY CHANGED
    uint32_t writes_count = 0;
    lgx_descriptor_write_info                   writes[textures_array_length];
    lgx_descriptor_sampled_texture_write_info   tinfo [textures_array_length];
    for (int i = 0; i < textures_array_length; i++) {
        if (state.textures_hashmap[i] == NULL) continue;

        tinfo[writes_count] = (lgx_descriptor_sampled_texture_write_info){
            .sampled_texture = state.textures_hashmap[i]
        };

        writes[writes_count] = (lgx_descriptor_write_info){
            .descriptor                 = frame->descriptor,
            .binding_type               = lgx_descriptor_binding_type_sampled_texture,
            .binding_index              = 3,
            .array_element_index        = i,
            .array_elements_count       = 1,
            .infos.for_sampled_textures = &tinfo[writes_count]
        };

        writes_count++;
    }
    lgx_descriptors_write(contextes->owning_shared->owning_hardware, writes_count, writes);

    // cleanup
    if (!provided_cpu_signal && upload_finished_cpu) lgx_free_cpu_signal(upload_finished_cpu);
    free(state.textures_hashmap);

    frame->instances_to_render = state.all_instances_to_draw;
}

void luirp_gcmd_render_ui(
    lgx_command_list*                   list,
    luirp_frames_contextes*    contextes,
    uint32_t                            frame_in_flight_index
) {
    frame_context* frame = &contextes->contextes[frame_in_flight_index % contextes->contextes_count];
    lgx_gcmd_bind_graphics_pipeline(list, contextes->owning_shared->pipeline);
    lgx_gcmd_bind_graphics_pipeline_descriptors(
        list, 
        contextes->owning_shared->pipeline_descriptor_layout, 
        0, 1, 
        &frame->descriptor
    );
    lgx_gcmd_bind_graphics_pipeline_vertex_buffer(list, contextes->owning_shared->vertex_buffer, 0, 0);
    lgx_gcmd_draw_vertices(list, 4, 0, frame->instances_to_render, 0);
}

static inline void lui_injection_measure_sized_image(
    const lui_image_data* image, 
    lui_length* width_target, lui_length* height_target,
    void* user_context
) {
    lgx_texture*                        image_tex;
    luirp_atlas_position_uv    atlas;
    luirp_injection_query_image(image, &image_tex, &atlas);

    if (!image_tex) {
        *width_target  = (lui_length){0, 0, 0};
        *height_target = (lui_length){0, 0, 0};
        return;
    }

    lgx_texture_dimensions dim = lgx_texture_get_dimensions(image_tex);
    float width  = dim.x * (atlas.uv_max_x - atlas.uv_min_x);
    float height = dim.y * (atlas.uv_max_y - atlas.uv_min_y);

    *width_target  = (lui_length){width,  width,  0};
    *height_target = (lui_length){height, height, 0};
};

static inline void lui_injection_measure_text(
    const lui_text_data* data,
    lui_length* width_target, lui_length* height_target,
    void* user_context
) {
    if (!data->font || !data->text) {
        *width_target  = (lui_length){0, 0, 0};
        *height_target = (lui_length){0, 0, 0};
        return;
    }

    lfont* font = NULL; luirp_injection_query_font(data, &font);
    if (!font) {
        *width_target  = (lui_length){0, 0, 0};
        *height_target = (lui_length){0, 0, 0};
        return;
    }

    float font_line_height = lfont_get_base_ascent(font) - lfont_get_base_descent(font);  // The line height
    float font_line_gap    = lfont_get_base_line_gap(font);                                  // Extra spacing between lines

    float max_width     = 0;
    float current_width = 0;
    float height        = font_line_height;

    float previous_size_x    = 0;
    float previous_advance_x = 0;

    size_t itr = 0;
    while (data->text[itr] != '\0') {
        uint32_t codepoint; itr   += lfont_utf8_decode(data->text, itr, &codepoint);

        if (codepoint == '\n') {
            // Instead of advance use size on last glyph in line
            // to ensure entirety of it is in box
            if (itr != 0) {
                current_width -= previous_advance_x;
                current_width += previous_size_x;
            }

            // Add new line
            height += font_line_gap;
            height += font_line_height;

            // Box width = max width over lines
            max_width = current_width > max_width ? current_width : max_width;
            current_width = 0;

            continue;
        }

        lfont_glyph glyph = lfont_get_glyph(font, codepoint);
        current_width       += glyph.advance_x;
        previous_size_x     = glyph.size_x;
        previous_advance_x  = glyph.advance_x;
    }

    // Box width = max width over lines
    max_width = current_width > max_width ? current_width : max_width;

    // Scale box acording to this text font scale
    float font_scale = (float)data->size / lfont_get_base_size(font);
    max_width *= font_scale;
    height    *= font_scale;

    *width_target  = (lui_length){max_width, max_width, 0};
    *height_target = (lui_length){height,    height,    0};
}

#endif // LIGHT_USER_INTERFACE_RENDERING_PIPELINE_IMPL
