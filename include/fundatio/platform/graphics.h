
#ifndef FUNDATIO_GRAPHICS_H
#define FUNDATIO_GRAPHICS_H

// ===========================
// Dependency

#include <stdint.h>

// ===========================
// Enumerations

typedef enum fnd_gfx_memory_access {
    fnd_gfx_memory_access_rendering_internal,       // resource can be used by gpu processes only
    fnd_gfx_memory_access_staging_read,             // resource can also be copied to staging buffer
    fnd_gfx_memory_access_staging_write,            // resource can also be written from staging buffer
    fnd_gfx_memory_access_staging_read_and_write    // resource can also be copied and written with staging buffer
} fnd_gfx_memory_access;

typedef enum fnd_gfx_command_domain {
    fnd_gfx_command_domain_transfer,    // transfer commands
    fnd_gfx_command_domain_compute,     // transfer and compute commands
    fnd_gfx_command_domain_graphics,    // transfer, compute and graphics commands
    fnd_gfx_command_domain_count
} fnd_gfx_command_domain;

typedef enum fnd_gfx_resource_type {
    fnd_gfx_resource_type_uniform_buffer,
    fnd_gfx_resource_type_storage_buffer,
    fnd_gfx_resource_type_sampled_texture,
    fnd_gfx_resource_type_storage_texture,
    fnd_gfx_resource_type_sampler,
    fnd_gfx_resource_type_count
} fnd_gfx_resource_type;

// ===========================
// Utility Structs

typedef struct fnd_gfx_color {
    float r, g, b, a;
} fnd_gfx_color;

typedef struct fnd_gfx_uv_2d {
    float min_x, min_y;
    float max_x, max_y;
} fnd_gfx_uv_2d;

// ===========================
// Library

typedef struct fnd_gfx_library_create_info {
} fnd_gfx_library_create_info;

typedef struct fnd_gfx_library fnd_gfx_library;
fnd_gfx_library* fnd_gfx_create_library(const fnd_gfx_library_create_info*);
void fnd_gfx_free_library(fnd_gfx_library*);

// detected devices count
uint32_t fnd_gfx_library_query_hardware_count(fnd_gfx_library*);

// name must be freed
char*    fnd_gfx_library_query_hardware_name(fnd_gfx_library*, uint32_t index); 

// whether this rhi support this hardware
int      fnd_gfx_library_query_hardware_supported(fnd_gfx_library*, uint32_t index);  

// whether can create and render to windows      
int      fnd_gfx_library_query_hardware_windowing_support(fnd_gfx_library*, uint32_t index);

// how many will truly work simultaneously
uint8_t  fnd_gfx_library_query_hardware_concurrent_work_groups(fnd_gfx_library*, uint32_t index, fnd_gfx_command_domain domain);

// ===========================
// Hardware

typedef enum fnd_gfx_hardware_type {
    fnd_gfx_hardware_type_dont_mind = 0,
    fnd_gfx_hardware_type_discrete,
    fnd_gfx_hardware_type_integrated,
} fnd_gfx_hardware_type;

typedef struct fnd_gfx_hardware_create_info {
    uint32_t    hardware_index;                                     // library detected hardware index
    int         enable_windowing;                                   // enable windowing on this hardware
    uint8_t     work_groups_per_domain[fnd_gfx_command_domain_count];   // created abstract work groups per command domain
    uint32_t    shader_resources_limit[fnd_gfx_resource_type_count];    // maximum objects of type accessed by shader
} fnd_gfx_hardware_create_info;

typedef struct fnd_gfx_hardware fnd_gfx_hardware;
fnd_gfx_hardware* fnd_gfx_create_hardware(fnd_gfx_library*, const fnd_gfx_hardware_create_info* info);
void fnd_gfx_free_hardware(fnd_gfx_hardware*);

// Wait till all hardware have no work at all
void fnd_gfx_hardware_wait_idle(fnd_gfx_hardware*);

// ===========================
// Timeline

typedef struct fnd_gfx_timeline_create_info {
    uint64_t initial_value;
} fnd_gfx_timeline_create_info;

typedef struct fnd_gfx_timeline fnd_gfx_timeline;
fnd_gfx_timeline* fnd_gfx_create_timeline(fnd_gfx_hardware*, const fnd_gfx_timeline_create_info* info);
void fnd_gfx_free_timeline(fnd_gfx_timeline*);

void     fnd_gfx_timeline_signal     (fnd_gfx_timeline*, uint64_t value);   // Set timeline value from CPU
void     fnd_gfx_timeline_wait       (fnd_gfx_timeline*, uint64_t value);   // CPU wait until completed value >= value
int      fnd_gfx_timeline_is_after   (fnd_gfx_timeline*, uint64_t value);   // Optional non-blocking check
uint64_t fnd_gfx_timeline_get_value  (fnd_gfx_timeline*);                   // Query highest completed value

// ===========================
// Command List

typedef void (*fnd_gfx_commands_recorder_func)(void* params);
typedef struct fnd_gfx_commands fnd_gfx_commands;

typedef struct fnd_gfx_commands_create_info {
    fnd_gfx_command_domain          domain; // allowed command types for this command list
    uint8_t                         aindex; // selects allocator; create/free on the same (domain, aindex) must be externally synchronized
    fnd_gfx_commands*               parent; // previous version of list, from same group, consumed here to save resources (do not free later)
    fnd_gfx_commands_recorder_func  record; // recorder callback function pointer
    void*                           params; // recorder callback parameters pointer
} fnd_gfx_commands_create_info;

fnd_gfx_commands* fnd_gfx_create_commands(fnd_gfx_hardware*, const fnd_gfx_commands_create_info* info);
void fnd_gfx_free_commands(fnd_gfx_commands*);

typedef struct fnd_gfx_submit_info {
    uint8_t          domain_work_group;   // target work group

    uint32_t         wait_count;
    fnd_gfx_timeline**   wait_timelines;
    uint64_t*        wait_values;

    uint32_t         signal_count;
    fnd_gfx_timeline**   signal_timelines;
    uint64_t*        signal_values;
} fnd_gfx_submit_info;

void fnd_gfx_commands_submit(uint32_t count, fnd_gfx_commands** lists, const fnd_gfx_submit_info* info);

// ==========================
// Staging

typedef struct fnd_gfx_staging_create_info {
    uint64_t bytes;
} fnd_gfx_staging_create_info;

typedef struct fnd_gfx_staging fnd_gfx_staging;
fnd_gfx_staging* fnd_gfx_create_staging(fnd_gfx_hardware*, const fnd_gfx_staging_create_info*);
void fnd_gfx_free_staging(fnd_gfx_staging*);

void* fnd_gfx_staging_map(fnd_gfx_staging*, uint64_t region_offset, uint64_t region_size);
void fnd_gfx_staging_unmap(fnd_gfx_staging*);

// ===========================
// Buffer

typedef enum fnd_gfx_buffer_usage {
    fnd_gfx_buffer_usage_uniform,
    fnd_gfx_buffer_usage_storage
} fnd_gfx_buffer_usage;

typedef struct fnd_gfx_buffer_create_info {
    uint64_t            bytes;
    fnd_gfx_buffer_usage    usage;
    fnd_gfx_memory_access   access;
} fnd_gfx_buffer_create_info;

typedef struct fnd_gfx_buffer fnd_gfx_buffer;
fnd_gfx_buffer* fnd_gfx_create_buffer(fnd_gfx_hardware*, const fnd_gfx_buffer_create_info* info);
void fnd_gfx_free_buffer(fnd_gfx_buffer*);

uint64_t fnd_gfx_buffer_query_bytes(fnd_gfx_buffer*);

// ==========================
// Texture

typedef enum fnd_gfx_texture_usage {
    fnd_gfx_texture_usage_sampled,
    fnd_gfx_texture_usage_storage,
    fnd_gfx_texture_usage_color_attachment,
    fnd_gfx_texture_usage_depth_stencil_attachment,
} fnd_gfx_texture_usage;

typedef enum fnd_gfx_texture_type {
    fnd_gfx_texture_type_1d,
    fnd_gfx_texture_type_2d,
    fnd_gfx_texture_type_3d,
    fnd_gfx_texture_type_cubemap,
} fnd_gfx_texture_type;

typedef enum fnd_gfx_texture_format {
    fnd_gfx_texture_format_undefined = 0,

    // 8-bit
    fnd_gfx_texture_format_r8_unorm,
    fnd_gfx_texture_format_rg8_unorm,
    fnd_gfx_texture_format_rgba8_unorm,
    fnd_gfx_texture_format_rgba8_srgb,
    fnd_gfx_texture_format_bgra8_unorm,
    fnd_gfx_texture_format_bgra8_srgb,

    // 16-bit float
    fnd_gfx_texture_format_r16_float,
    fnd_gfx_texture_format_rg16_float,
    fnd_gfx_texture_format_rgba16_float,

    // 32-bit float
    fnd_gfx_texture_format_r32_float,
    fnd_gfx_texture_format_rg32_float,
    fnd_gfx_texture_format_rgba32_float,

    // Depth / stencil
    fnd_gfx_texture_format_depth16_unorm,
    fnd_gfx_texture_format_depth24_unorm_stencil8,
    fnd_gfx_texture_format_depth32_float,
} fnd_gfx_texture_format;

typedef struct fnd_gfx_texture_dimensions {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
} fnd_gfx_texture_dimensions;

typedef struct fnd_gfx_texture_create_info {
    fnd_gfx_texture_type        type;
    fnd_gfx_texture_usage       usage;
    fnd_gfx_texture_format      format;
    fnd_gfx_texture_dimensions  dimensions;
    uint32_t                    array_length;
    uint32_t                    sample_count;
    uint32_t                    mipmap_layers;
    fnd_gfx_memory_access       memory_access;
} fnd_gfx_texture_create_info;

typedef struct fnd_gfx_texture fnd_gfx_texture;
fnd_gfx_texture* fnd_gfx_create_texture(fnd_gfx_hardware*, const fnd_gfx_texture_create_info* info);
void fnd_gfx_free_texture(fnd_gfx_texture* texture);

fnd_gfx_texture_dimensions fnd_gfx_texture_query_dimensions(fnd_gfx_texture*);
fnd_gfx_texture_format     fnd_gfx_texture_query_format(fnd_gfx_texture*);

// ==========================
// Sampler

typedef enum fnd_gfx_sampler_filter {
    fnd_gfx_sampler_filter_nearest,
    fnd_gfx_sampler_filter_linear,
} fnd_gfx_sampler_filter;

typedef enum fnd_gfx_sampler_wrapping {
    fnd_gfx_sampler_wrapping_repeat,
    fnd_gfx_sampler_wrapping_repeat_mirrored,
    fnd_gfx_sampler_wrapping_repeat_clamp_coordinates,
    fnd_gfx_sampler_wrapping_repeat_clamp_texture
} fnd_gfx_sampler_wrapping;

typedef struct fnd_gfx_sampler_create_info {
    fnd_gfx_sampler_filter      mag_filter;
    fnd_gfx_sampler_filter      min_filter;
    fnd_gfx_sampler_filter      mipmap_filter;

    fnd_gfx_sampler_wrapping    x_coord_wrapping;
    fnd_gfx_sampler_wrapping    y_coord_wrapping;
    fnd_gfx_sampler_wrapping    z_coord_wrapping;
    int                         unnormalized_coordinates;

    float                       min_lod, max_lod;
    float                       mip_lod_bias;
} fnd_gfx_sampler_create_info;

typedef struct fnd_gfx_sampler fnd_gfx_sampler;
fnd_gfx_sampler* fnd_gfx_create_sampler(fnd_gfx_hardware*, const fnd_gfx_sampler_create_info* info);
void fnd_gfx_free_sampler(fnd_gfx_sampler*);

// ===========================
// Shader

typedef enum fnd_gfx_shader_stage {
    fnd_gfx_shader_stage_vertex,
    fnd_gfx_shader_stage_geometry,
    fnd_gfx_shader_stage_pixel,
    fnd_gfx_shader_stage_count
} fnd_gfx_shader_stage;

typedef struct fnd_gfx_shader_create_info {
    const char* source_code;
    uint32_t    source_size;
} fnd_gfx_shader_create_info;

typedef struct fnd_gfx_shader fnd_gfx_shader;
fnd_gfx_shader* fnd_gfx_create_shader(fnd_gfx_hardware*, const fnd_gfx_shader_create_info* info);
void fnd_gfx_free_shader(fnd_gfx_shader* shader);

// Returns handle user can use to access resource in shader
// success value will be and'ed with 1 in case of success and 0 zero otherwise
uint32_t fnd_gfx_shader_resource_bind(fnd_gfx_hardware* hardware, fnd_gfx_resource_type type, void* resource, int* success);

// ===========================
// Graphics Pipeline

// Shader Stages

typedef struct fnd_gfx_pipeline_shader_stages {
    fnd_gfx_shader* shaders  [fnd_gfx_shader_stage_count];  // shader per stage
    uint32_t        constants[fnd_gfx_shader_stage_count];  // size of constants range per stage
} fnd_gfx_pipeline_shader_stages;

// Attachments

typedef enum fnd_gfx_load_op {
    fnd_gfx_load_op_load,       // keep previous contents
    fnd_gfx_load_op_clear,      // clear at start
    fnd_gfx_load_op_dont_care   // undefined (fast)
} fnd_gfx_load_op;

typedef enum fnd_gfx_store_op {
    fnd_gfx_store_op_store,     // keep result
    fnd_gfx_store_op_dont_care  // discard after rendering
} fnd_gfx_store_op;

typedef struct fnd_gfx_pipeline_attachment_state {
    uint32_t                    color_attachments_count;
    fnd_gfx_texture_format*     color_attachments_formats;
    fnd_gfx_texture_format*     depth_stencil_format;   // nullable
} fnd_gfx_pipeline_attachment_state;

// Input Assembly

typedef enum fnd_gfx_primitive_topology {
    fnd_gfx_primitive_topology_point_list,
    fnd_gfx_primitive_topology_line_list,
    fnd_gfx_primitive_topology_line_strip,
    fnd_gfx_primitive_topology_triangle_list,
    fnd_gfx_primitive_topology_triangle_strip
} fnd_gfx_primitive_topology;

typedef struct fnd_gfx_pipeline_input_assembler_state {
    fnd_gfx_primitive_topology topology;   
} fnd_gfx_pipeline_input_assembler_state;

// Rasterizer

typedef enum fnd_gfx_cull_mode {
    fnd_gfx_cull_mode_none,
    fnd_gfx_cull_mode_front,
    fnd_gfx_cull_mode_back,
    fnd_gfx_cull_mode_front_and_back
} fnd_gfx_cull_mode;

typedef enum fnd_gfx_fill_mode {
    fnd_gfx_fill_mode_solid,
    fnd_gfx_fill_mode_wireframe
} fnd_gfx_fill_mode;

typedef struct fnd_gfx_pipeline_rasterizer_state {
    fnd_gfx_cull_mode       cull_mode;
    fnd_gfx_fill_mode       fill_mode;
    int                 depth_clamp_enable;
    int                 scissor_enable;
} fnd_gfx_pipeline_rasterizer_state;

// Blending

typedef enum fnd_gfx_blend_op {
    fnd_gfx_blend_op_add = 0,
    fnd_gfx_blend_op_subtract,
    fnd_gfx_blend_op_reverse_subtract,
    fnd_gfx_blend_op_min,
    fnd_gfx_blend_op_max
} fnd_gfx_blend_op;

typedef enum fnd_gfx_blend_factor {
    fnd_gfx_blend_factor_zero = 0,
    fnd_gfx_blend_factor_one,

    fnd_gfx_blend_factor_src_color,
    fnd_gfx_blend_factor_one_minus_src_color,

    fnd_gfx_blend_factor_dst_color,
    fnd_gfx_blend_factor_one_minus_dst_color,

    fnd_gfx_blend_factor_src_alpha,
    fnd_gfx_blend_factor_one_minus_src_alpha,

    fnd_gfx_blend_factor_dst_alpha,
    fnd_gfx_blend_factor_one_minus_dst_alpha,

    fnd_gfx_blend_factor_constant_color,
    fnd_gfx_blend_factor_one_minus_constant_color,

    fnd_gfx_blend_factor_constant_alpha,
    fnd_gfx_blend_factor_one_minus_constant_alpha,

    fnd_gfx_blend_factor_src_alpha_saturate
} fnd_gfx_blend_factor;

typedef struct fnd_gfx_pipeline_blend_state {
    int                     blend_enable;
    fnd_gfx_blend_op            blend_op;
    fnd_gfx_blend_factor        src_factor;
    fnd_gfx_blend_factor        dst_factor;
} fnd_gfx_pipeline_blend_state; 

typedef struct fnd_gfx_pipeline_depth_stencil_state {
    int depth_test_enable;
    int depth_write_enable;
    int stencil_test_enable;
} fnd_gfx_pipeline_depth_stencil_state;

typedef struct fnd_gfx_pipeline_create_info {
    fnd_gfx_pipeline_shader_stages          shader_stages;
    fnd_gfx_pipeline_attachment_state       attachment_state;
    fnd_gfx_pipeline_input_assembler_state  input_assembler_state;
    fnd_gfx_pipeline_rasterizer_state       rasterizer_state;
    fnd_gfx_pipeline_blend_state            blend_state;
    fnd_gfx_pipeline_depth_stencil_state    depth_stencil_state;
} fnd_gfx_pipeline_create_info;

typedef struct fnd_gfx_pipeline fnd_gfx_pipeline;
fnd_gfx_pipeline* fnd_gfx_create_pipeline(fnd_gfx_hardware*, const fnd_gfx_pipeline_create_info* info);
void fnd_gfx_free_pipeline(fnd_gfx_pipeline* pipeline);

// ===========================
// Window

typedef struct fnd_gfx_window_create_info {
    const char* title;          // Desired window title
    uint32_t    width;          // Desired window width
    uint32_t    height;         // Desired window height
    uint32_t    attachments;    // Desired color attachments
} fnd_gfx_window_create_info;

typedef struct fnd_gfx_window fnd_gfx_window;
fnd_gfx_window* fnd_gfx_create_window(fnd_gfx_hardware*, const fnd_gfx_window_create_info*);
void fnd_gfx_free_window(fnd_gfx_window*);

int  fnd_gfx_window_query_shall_close(fnd_gfx_window*);
void fnd_gfx_window_query_is_focused(fnd_gfx_window*, int* is);
void fnd_gfx_window_query_cursor_pos(fnd_gfx_window*, int* xpos, int* ypos);
void fnd_gfx_window_query_input(fnd_gfx_window*, int* left_pressed, int* right_pressed, float* scroll);

void fnd_gfx_window_query_size    (fnd_gfx_window*, uint32_t* width, uint32_t* height);

// non-zero at success
int fnd_gfx_window_acquire (
    fnd_gfx_window*     window, 
    fnd_gfx_timeline*   can_render_timeline, 
    uint64_t            can_render_signal, 
    uint32_t*           out_index
);

void fnd_gfx_window_present(
    fnd_gfx_window*     window, 
    uint32_t            target_index, 
    uint32_t            wait_timeline_count,    
    fnd_gfx_timeline**  wait_timelines, 
    uint64_t*           wait_signals, 
    fnd_gfx_timeline*   wait_finished_timeline, 
    uint64_t            wait_finished_signal
);

fnd_gfx_texture*        fnd_gfx_window_get_attachment_color(fnd_gfx_window*, uint32_t target_index);
fnd_gfx_texture_format  fnd_gfx_window_get_attachment_format(fnd_gfx_window*);

// ===========================
// General Commands (cmd)
// Can only be called inside 
// command list recorder callback

/*void fnd_gfx_cmd_declare_buffer_use(
    fnd_gfx_usage_type          producer,
    fnd_gfx_usage_type          consumer,
    uint32_t                buffers_count,
    fnd_gfx_buffer**            buffers
);*/

// ===========================
// Transfer Commands (tcmd)
// Can only be called inside 
// command list recorder callback

void fnd_gfx_tcmd_copy_staging_to_buffer(
    fnd_gfx_staging*            staging,
    fnd_gfx_buffer*             target_buffer,
    uint64_t                    staging_region_offset,
    uint64_t                    buffer_write_region_offset,
    uint64_t                    buffer_write_region_size
);

void fnd_gfx_tcmd_copy_staging_to_texture(
    fnd_gfx_staging*            staging,
    fnd_gfx_texture*            target_texture,
    uint64_t                    staging_region_offset,
    fnd_gfx_texture_dimensions  texture_write_region_offset,
    fnd_gfx_texture_dimensions  texture_write_region_size
);

void fnd_gfx_tcmd_copy_buffer_to_buffer(
    fnd_gfx_buffer* source_buffer,
    fnd_gfx_buffer* target_buffer,
    uint64_t        source_region_offset,
    uint64_t        target_region_offset,
    uint64_t        target_region_size
);

// ===========================
// Graphics Commands (gcmd)
// Can only be called inside 
// command list recorder callback

typedef struct fnd_gfx_gcmd_rendering_attachment_info {
    fnd_gfx_color       clear_color;
    fnd_gfx_load_op     load_op;
    fnd_gfx_store_op    store_op;
    fnd_gfx_texture*    texture;
} fnd_gfx_gcmd_rendering_attachment_info;

typedef struct fnd_gfx_gcmd_rendering_info {
    int32_t                                 area_offset_x, area_offset_y;
    uint32_t                                area_width, area_height;
    uint32_t                                color_attachments_count;
    fnd_gfx_gcmd_rendering_attachment_info*     color_attachments;
    fnd_gfx_gcmd_rendering_attachment_info*     depth_stencil_attachment; // nullable
} fnd_gfx_gcmd_rendering_info;

void fnd_gfx_gcmd_begin_rendering (fnd_gfx_gcmd_rendering_info* info);
void fnd_gfx_gcmd_finish_rendering();

void fnd_gfx_gcmd_bind_graphics_pipeline(fnd_gfx_pipeline* pipeline);

void fnd_gfx_gcmd_write_constants(
    fnd_gfx_pipeline*    pipeline, 
    fnd_gfx_shader_stage stage, 
    uint32_t             offset,
    uint32_t             bytes, 
    void*                data
);

void fnd_gfx_gcmd_draw(
    uint32_t vertices_base,
    uint32_t vertices_count,
    uint32_t instances_base,
    uint32_t instances_count
);

void fnd_gfx_gcmd_set_scissors(
    int32_t root_x,  int32_t  root_y,
    uint32_t width,  uint32_t height
);

void fnd_gfx_gcmd_set_viewport(
    int32_t root_x,  int32_t  root_y,
    uint32_t width,  uint32_t height
);


// ===========================
// Helpers

static inline uint64_t fnd_gfx_texture_format_get_pixel_bytes(fnd_gfx_texture_format format) {
    switch (format) {
        case fnd_gfx_texture_format_undefined:              return 0;
        case fnd_gfx_texture_format_r8_unorm:               return 1;
        case fnd_gfx_texture_format_rg8_unorm:              return 2;
        case fnd_gfx_texture_format_rgba8_unorm:            return 4;
        case fnd_gfx_texture_format_rgba8_srgb:             return 4;
        case fnd_gfx_texture_format_bgra8_unorm:            return 4;
        case fnd_gfx_texture_format_bgra8_srgb:             return 4;
        case fnd_gfx_texture_format_r16_float:              return 2;
        case fnd_gfx_texture_format_rg16_float:             return 4;
        case fnd_gfx_texture_format_rgba16_float:           return 8;
        case fnd_gfx_texture_format_r32_float:              return 4;
        case fnd_gfx_texture_format_rg32_float:             return 8;
        case fnd_gfx_texture_format_rgba32_float:           return 16;
        case fnd_gfx_texture_format_depth16_unorm:          return 2;
        case fnd_gfx_texture_format_depth24_unorm_stencil8: return 4;
        case fnd_gfx_texture_format_depth32_float:          return 4;
    } return 0;
}

#endif

#ifdef FUNDATIO_GRAPHICS_IMPL
#ifdef FUNDATIO_GRAPHICS_VULKAN

/*
    Vulkan branch uses extensions:
    - VK_KHR_SWAPCHAIN_EXTENSION
    - VK_KHR_timeline_semaphore
    - VK_KHR_dynamic_rendering
    - VK_KHR_synchronisation_2
*/

// ===========================
// Depedency

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../../../depedency/volk/volk.h"
#include "../../../depedency/volk/volk.c"

#include "fundatio/algorithm/partitioner.h"
#include "fundatio/platform/threads.h"

// ===========================
// Config

// Command Allocators

// Guaranteed to cover entire aindex range, no checking needed
#define HARDWARE_COMMANDS_ALLOCATORS_COUNT 256

// Guaranteed to cover entire work group index range, no checking needed
#define HARDWARE_MAX_WORK_GROUPS 256

// Instance Extensions

const char** config_instance_extensions_array = (const char*[]){
    "VK_KHR_get_physical_device_properties2"
};
const uint32_t config_instance_extensions_count = 1;

// Validation Extensions

#ifdef FUNDATIO_GRAPHICS_VALIDATE
    static const char* validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
    const char**    config_validation_layers_array = &validation_layers[0];
    const uint32_t  config_validation_layers_count = 1;
#else 
    const char**    config_validation_layers_array = NULL;
    const uint32_t  config_validation_layers_count = 0;
#endif

// Device Extensions

const char** config_required_device_extensions_array = (const char*[]){  
    VK_KHR_MULTIVIEW_EXTENSION_NAME,                // create renderpass2 depedency
    VK_KHR_MAINTENANCE2_EXTENSION_NAME,             // create renderpass2 depedency
    VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,      // depth stencil resolve depedency
    VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,    // dynamic rendering depedency
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
    VK_KHR_MAINTENANCE3_EXTENSION_NAME,             // descriptor indexing depedency
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
const uint32_t config_required_device_extensions_count = 9;

// Pipelines Dynamic State

const VkDynamicState* config_all_pipelines_dynamic_state_array = (VkDynamicState[]){
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
};
const uint32_t config_all_pipelines_dynamic_state_count = 2;

// ===========================
// Windowing Platform Library Forward

int     windowing_platform_init(fnd_gfx_library*);
void    windowing_platform_term(fnd_gfx_library*);
int     windowing_platform_get_required_extensions(fnd_gfx_library*, uint32_t* count, const char*** names);
int     windowing_platform_query_presentation_support(fnd_gfx_library*, VkPhysicalDevice device);
int     windowing_platform_create_test_surface(fnd_gfx_library* library, VkSurfaceKHR* surface, void** other_data_storage);
void    windowing_platform_free_test_surface  (fnd_gfx_library*, VkSurfaceKHR surface, void* other_data_return);

// ===========================
// Physical Device Queries

int check_physical_device_extension_support(VkPhysicalDevice device) {
    uint32_t extensions_count; vkEnumerateDeviceExtensionProperties(device, 0, &extensions_count, 0);

    VkExtensionProperties* available_extensions = malloc(sizeof(VkExtensionProperties) * extensions_count);
    vkEnumerateDeviceExtensionProperties(device, 0, &extensions_count, available_extensions);

    uint32_t unfound_required_extensions = config_required_device_extensions_count;
    for (uint32_t i = 0; i < extensions_count; i++) {
        VkExtensionProperties* extension = &available_extensions[i];
        
        for (uint32_t j = 0; j < config_required_device_extensions_count; j++) {
            const char* required = config_required_device_extensions_array[j];
            if (strcmp(extension->extensionName, required) == 0) unfound_required_extensions--;
        }
    }

    free(available_extensions);
    if (unfound_required_extensions == 0) return 1;
    return 0;
}

int check_physical_device_feature_support(VkPhysicalDevice device) {
    VkPhysicalDeviceTimelineSemaphoreFeatures timeline_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
        .timelineSemaphore = VK_TRUE
    };

    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &timeline_features
    };

    vkGetPhysicalDeviceFeatures2(device, &features2);
    if (!timeline_features.timelineSemaphore) return 0;

    return 1;
}

typedef struct queues_family_info {
    uint32_t family[fnd_gfx_command_domain_count];
    uint32_t count [fnd_gfx_command_domain_count];
    int      presentation_separate;
    uint32_t presentation_family;
    uint32_t presentation_count;
} queues_family_info;
 
queues_family_info get_physical_device_queues_family_info(fnd_gfx_library* library, VkPhysicalDevice device, int windowing_enabled) {
    // Enumerate queue families
    uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, NULL);
    
    // Pull families info
    VkQueueFamilyProperties* families = malloc(sizeof(VkQueueFamilyProperties) * family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, families);
    
    // Initialize info structure
    queues_family_info info = {0};
    
    // Create test surface to test for presentation
    VkSurfaceKHR surface; void* other_surface_info;
    if (windowing_enabled) {
        if (!windowing_platform_create_test_surface(library, &surface, &other_surface_info)) {
            free(families); return info;
        }
    }

    // For rating queue for presentation
    // The bigger value the better, 0 means not found
    // 4 points - separate queue
    // 3 points - prefer graphics + present queue
    // 2 points - prefer compute  + present queue
    // 1 points - prefer transfer + present queue
    int current_presentation_queue_score = 0;
    
    // Walk queue families and categorize them
    for (uint32_t i = 0; i < family_count; i++) {
        VkQueueFlags flags = families[i].queueFlags;
        int has_graphics = (flags & VK_QUEUE_GRAPHICS_BIT);
        int has_compute  = (flags & VK_QUEUE_COMPUTE_BIT);
        int has_transfer = (flags & VK_QUEUE_TRANSFER_BIT);
        
        // Check for presentation support
        if (windowing_enabled) {
            VkBool32 presentation_support = 0; vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentation_support);

            if (presentation_support) {
                int is_separate = !has_graphics && !has_compute && !has_transfer;
                int this_score  = 0;

                if      (is_separate)  this_score = 4;
                else if (has_graphics) this_score = 3;
                else if (has_compute)  this_score = 2;
                else if (has_transfer) this_score = 1;

                if (this_score > current_presentation_queue_score) {
                    current_presentation_queue_score = this_score;
                    info.presentation_separate  = is_separate;
                    info.presentation_family    = i;
                    info.presentation_count     = families[i].queueCount;
                }
            }
        }
        
        // Graphics domain: has graphics (and implicitly compute and transfer)
        if (has_graphics && info.count[fnd_gfx_command_domain_graphics] == 0) {
            info.family[fnd_gfx_command_domain_graphics] = i;
            info.count [fnd_gfx_command_domain_graphics] = families[i].queueCount;
        }
        // Compute domain: has compute (and implicitly transfer) but not graphics
        else if (!has_graphics && has_compute && info.count[fnd_gfx_command_domain_compute] == 0) {
            info.family[fnd_gfx_command_domain_compute] = i;
            info.count [fnd_gfx_command_domain_compute] = families[i].queueCount;
        }
        // Transfer domain: only transfer, no graphics or compute
        else if (!has_graphics && !has_compute && has_transfer && info.count[fnd_gfx_command_domain_transfer] == 0) {
            info.family[fnd_gfx_command_domain_transfer] = i;
            info.count [fnd_gfx_command_domain_transfer] = families[i].queueCount;
        }
    }
    
    // Free test surface
    if (windowing_enabled) {
        windowing_platform_free_test_surface(library, surface, other_surface_info);
    }
    
    // Free families info and return
    free(families);
    return info;
}

// ===========================
// Library

struct fnd_gfx_library {
    VkInstance  instance;
    void*       windowing;
};

static int was_vulkan_loaded = 0;
fnd_gfx_library* fnd_gfx_create_library(const fnd_gfx_library_create_info* info) {
    if (!was_vulkan_loaded) {
        if (volkInitialize() == VK_SUCCESS) {
            was_vulkan_loaded = 1;
        }  else return NULL;
    }
     
    fnd_gfx_library* library = calloc(1, sizeof(fnd_gfx_library));
    if (!library) return NULL;

    VkApplicationInfo app_info = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "fundatio Graphics App",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "No Engine",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo create_info = {
        .sType                      = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo           = &app_info,
        .enabledLayerCount          = config_validation_layers_count,
        .ppEnabledLayerNames        = config_validation_layers_array,
    };

    if (!windowing_platform_init(library)) goto _fail;

    uint32_t     platform_extension_count;
    const char** platform_extension_names;
    
    if (!windowing_platform_get_required_extensions(library, &platform_extension_count, &platform_extension_names)) goto _fail;

    uint32_t     enabled_extensions_combined_count = platform_extension_count + config_instance_extensions_count;
    const char** enabled_extensions_combined = malloc(enabled_extensions_combined_count * sizeof(const char*));

    memcpy(
        enabled_extensions_combined, 
        config_instance_extensions_array, 
        config_instance_extensions_count * sizeof(const char*)
    );

    memcpy(
        enabled_extensions_combined + config_instance_extensions_count, 
        platform_extension_names, 
        platform_extension_count * sizeof(const char*)
    );

    create_info.enabledExtensionCount   = enabled_extensions_combined_count;
    create_info.ppEnabledExtensionNames = enabled_extensions_combined;

    VkResult result = vkCreateInstance(&create_info, 0, &library->instance);
    free(enabled_extensions_combined); if (result != VK_SUCCESS) goto _fail;

    volkLoadInstance(library->instance);
    return library;

_fail: windowing_platform_term(library); free(library); return NULL;
}

void fnd_gfx_free_library(fnd_gfx_library* library) {
    if (!library) return;
    vkDestroyInstance(library->instance, 0);
    windowing_platform_term(library);
    free(library);
}

int get_physical_device_at_index(fnd_gfx_library* library, uint32_t index, VkPhysicalDevice* target) {
    if (!library || !library->instance) return 0;
    
    uint32_t device_count = 0; if (vkEnumeratePhysicalDevices(library->instance, &device_count, NULL) != VK_SUCCESS) return 0;
    if (index >= device_count) return 0;
    
    VkPhysicalDevice* devices = malloc(device_count * sizeof(VkPhysicalDevice)); if (!devices) return 0;
    if (vkEnumeratePhysicalDevices(library->instance, &device_count, devices) != VK_SUCCESS) { free(devices); return 0; }

    *target = devices[index]; free(devices); return 1;
}

uint32_t fnd_gfx_library_query_hardware_count(fnd_gfx_library* library) {
    if (!library || !library->instance) return 0;
    uint32_t device_count = 0; if (vkEnumeratePhysicalDevices(library->instance, &device_count, NULL) != VK_SUCCESS) return 0;
    return device_count;
}

char* fnd_gfx_library_query_hardware_name(fnd_gfx_library* library, uint32_t index) {
    VkPhysicalDevice device; if (!get_physical_device_at_index(library, index, &device)) return NULL;

    VkPhysicalDeviceProperties properties; vkGetPhysicalDeviceProperties(device, &properties);
    char* device_name = malloc(sizeof(char) * VK_MAX_PHYSICAL_DEVICE_NAME_SIZE); if (!device_name) return NULL;
    strncpy(device_name, properties.deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1);
    device_name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1] = '\0';

    return device_name;
}

// check if hardware support required extensions
int fnd_gfx_library_query_hardware_supported(fnd_gfx_library* library, uint32_t index) {
    VkPhysicalDevice device; if (!get_physical_device_at_index(library, index, &device)) return 0;
    int success = 1;
    success &= check_physical_device_extension_support(device);
    success &= check_physical_device_feature_support(device);
    return success;
}

int fnd_gfx_library_query_hardware_windowing_support(fnd_gfx_library* library, uint32_t index) {
    VkPhysicalDevice device; if (!get_physical_device_at_index(library, index, &device)) return 0;
    
    // Query presentation support with platform-specific surface
    int supports_windowing = windowing_platform_query_presentation_support(library, device); 
    return supports_windowing;
}

uint8_t fnd_gfx_library_query_hardware_concurrent_work_groups(fnd_gfx_library* library, uint32_t index, fnd_gfx_command_domain domain) {
    VkPhysicalDevice   device; if (!get_physical_device_at_index(library, index, &device)) return 0;
    queues_family_info qf_info = get_physical_device_queues_family_info(library, device, 0);
    uint8_t count = qf_info.count[domain] > 255 ? 255 : qf_info.count[domain];
    return count;
}

// ===========================
// Hardware Allocators Forwards

int  hardware_init_command_allocators(fnd_gfx_hardware* hardware, const fnd_gfx_hardware_create_info*);
void hardware_free_command_allocators(fnd_gfx_hardware* hardware);

int  hardware_init_memory_allocators(fnd_gfx_hardware* hardware, const fnd_gfx_hardware_create_info*);
void hardware_free_memory_allocators(fnd_gfx_hardware* hardware);

int  hardware_init_shader_access(fnd_gfx_hardware* hardware, const fnd_gfx_hardware_create_info*);
void hardware_free_shader_access(fnd_gfx_hardware* hardware);

// ===========================
// Hardware

typedef struct command_allocator  command_allocator;
typedef struct memory_allocator   memory_allocator;
typedef struct bindless_allocator bindless_allocator;

struct fnd_gfx_hardware {
    // Hardware device info
    // Set at creation
    fnd_gfx_library*                owning_library;
    VkPhysicalDevice            physical_device;
    VkPhysicalDeviceProperties  physical_device_properties;
    VkDevice                    logical_device;

    // Loaded device functions
    // Set at creation
    PFN_vkCmdBeginRenderingKHR          vkCmdBeginRenderingKHR;
    PFN_vkCmdEndRenderingKHR            vkCmdEndRenderingKHR;
    PFN_vkSignalSemaphoreKHR            vkSignalSemaphoreKHR;
    PFN_vkWaitSemaphoresKHR             vkWaitSemaphoresKHR;
    PFN_vkGetSemaphoreCounterValueKHR   vkGetSemaphoreCounterValueKHR;

    // Work groups info
    // Set at creation
    uint8_t  work_group_count       [fnd_gfx_command_domain_count];                             // Requested work groups count
    uint32_t work_group_queue_family[fnd_gfx_command_domain_count];                             // Mapping : domain to queue family
    VkQueue  work_group_queue       [fnd_gfx_command_domain_count][HARDWARE_MAX_WORK_GROUPS];   // Mapping work groups to queues
    VkQueue  presentation_queue;
    uint32_t presentation_queue_family;

    // Command pools
    // Mutexed, since will have to respond to queries from diffrent threads
    // As command list (while creation) and not hardware is the API sync unit
    // Managed by separate code section see "Command Allocation"
    fnd_thr_mutex*          command_allocators_mutex;   // structure mutex
    command_allocator*  command_allocators;         // indexed with allocator index

    // Memory allocator data
    // Managed by separate code section see "Memory Allocation"
    VkPhysicalDeviceMemoryProperties memory_properties;  // queried from physical device
    memory_allocator*                memory_allocators;  // per memory type

    // Shader access
    // Managed by separate code section see "Shader Access"
    VkDescriptorSetLayout   bindless_descriptor_layout;
    VkDescriptorPool        bindless_descriptor_pool;
    VkDescriptorSet         bindless_descriptor;
    bindless_allocator*     bindless_allocators;    // per resource type
};

uint8_t queues_count_min(uint8_t desired_work_groups, uint32_t hardware_queues) {
    return desired_work_groups < hardware_queues ? desired_work_groups : hardware_queues;
}

fnd_gfx_hardware* fnd_gfx_create_hardware(fnd_gfx_library* library, const fnd_gfx_hardware_create_info* info) {
    fnd_gfx_hardware* hardware = calloc(1, sizeof(fnd_gfx_hardware));
    hardware->owning_library = library;

    // Get physical device
    if (!get_physical_device_at_index(library, info->hardware_index, &hardware->physical_device)) goto _fail;

    // Get queues family info
    queues_family_info qf_info = get_physical_device_queues_family_info(
        library, hardware->physical_device, info->enable_windowing
    );

    // Assert for presentation queue
    if (qf_info.presentation_count == 0 && info->enable_windowing) goto _fail;

    // Find queue families and counts
    uint32_t domain_created_queues  [fnd_gfx_command_domain_count] = {0};
    uint32_t domain_fallback_domain [fnd_gfx_command_domain_count] = {0};
    for (uint32_t assigned_domain = 0; assigned_domain < fnd_gfx_command_domain_count; assigned_domain++) {
        // by default read from the target domain
        uint32_t* read_domains = (uint32_t[]){assigned_domain};
        uint32_t  read_count = 1;

        // for selected domains allow fallback queue families
        // in case hardware does not have dedicated queues
        switch (assigned_domain) {
        case fnd_gfx_command_domain_transfer: {
            read_domains = (uint32_t[]){fnd_gfx_command_domain_transfer, fnd_gfx_command_domain_compute, fnd_gfx_command_domain_graphics};
            read_count = 3;
        } break;
        case fnd_gfx_command_domain_compute: {
            read_domains = (uint32_t[]){fnd_gfx_command_domain_compute, fnd_gfx_command_domain_graphics};
            read_count = 2;
        } break;
        }

        // walk possible domains
        uint32_t read_domains_itr = 0;
        while (1) {
            uint32_t read_domain = read_domains[read_domains_itr];
            domain_fallback_domain[assigned_domain] = read_domain;

            // assign this queue family to domain
            hardware->work_group_queue_family[assigned_domain] = qf_info.family[read_domain];

            // do not change user work groups count
            // we will implicitly map queues to possibly bigger number
            // just to keep user code simple
            hardware->work_group_count[assigned_domain] = info->work_groups_per_domain[assigned_domain];
            
            // created queue count = min(desired work groups, available queues)
            domain_created_queues[read_domain] = queues_count_min(
                info->work_groups_per_domain[assigned_domain], qf_info.count[read_domain]
            );

            // if no queues, try to fallback
            if (qf_info.count[read_domain] == 0 && info->work_groups_per_domain[assigned_domain] != 0) {
                read_domains_itr++; if (read_domains_itr >= read_count) goto _fail; continue;
            }

            break; // found suitable queue family
        }
    }

    // Set all queues priority to equal of 1.0f
    float* queues_priorities = malloc(1024 * sizeof(float));
    for (uint32_t i = 0; i < 1024; i++) queues_priorities[i] = 1.0f;

    // Queues Creation Info
    uint32_t                queues_create_info_itr = 0;
    VkDeviceQueueCreateInfo queues_create_infos[fnd_gfx_command_domain_count + 1];  // +1 for possible presentation queue

    for (uint32_t domain = 0; domain < fnd_gfx_command_domain_count; domain++) {
        if (!domain_created_queues[domain]) continue;
        queues_create_infos[queues_create_info_itr++] = (VkDeviceQueueCreateInfo){
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = hardware->work_group_queue_family[domain],
            .queueCount       = domain_created_queues[domain],
            .pQueuePriorities = queues_priorities,
        };
    }

    // additional separate presentation queue
    if (info->enable_windowing && qf_info.presentation_separate) queues_create_infos[queues_create_info_itr++] = (VkDeviceQueueCreateInfo){
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = qf_info.presentation_family,
        .queueCount       = 1,
        .pQueuePriorities = queues_priorities,
    };

    // Device Creation
    // Enable required extensions and features

    VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .runtimeDescriptorArray                         = VK_TRUE,
        .shaderStorageBufferArrayNonUniformIndexing     = VK_TRUE,
        .descriptorBindingUniformBufferUpdateAfterBind  = VK_TRUE,
        .descriptorBindingStorageBufferUpdateAfterBind  = VK_TRUE,
        .descriptorBindingStorageImageUpdateAfterBind   = VK_TRUE,
        .descriptorBindingSampledImageUpdateAfterBind   = VK_TRUE
    };
    
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .pNext = &descriptor_indexing_features,
        .dynamicRendering = VK_TRUE
    };

    VkPhysicalDeviceTimelineSemaphoreFeatures timeline_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
        .pNext = &dynamic_rendering_features,
        .timelineSemaphore = VK_TRUE
    };

    VkDeviceCreateInfo device_create_info = {
        .sType                      = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                      = &timeline_features,
        .queueCreateInfoCount       = queues_create_info_itr,
        .pQueueCreateInfos          = queues_create_infos,
        .enabledExtensionCount      = config_required_device_extensions_count,
        .ppEnabledExtensionNames    = config_required_device_extensions_array,
    };

    VkResult result = vkCreateDevice(hardware->physical_device, &device_create_info, 0, &hardware->logical_device);
    free(queues_priorities); if (result != VK_SUCCESS) goto _fail;

    // Pull domains queue objects
    for (uint32_t domain = 0; domain < fnd_gfx_command_domain_count; domain++) {
        uint32_t queues_domain = domain_fallback_domain[domain];
        uint32_t queues_count  = domain_created_queues[queues_domain];
        uint32_t family_index  = hardware->work_group_queue_family[domain];
        for (uint32_t work_group = 0; work_group < hardware->work_group_count[domain]; work_group++) {
            uint32_t queue_index = work_group % queues_count;
            vkGetDeviceQueue(hardware->logical_device, family_index, queue_index, &hardware->work_group_queue[domain][work_group]);
        }
    }

    // Pull device functions
    #define DEVICE_FUNCTION_LOAD(name) \
        hardware->name = (PFN_##name)vkGetDeviceProcAddr(hardware->logical_device, #name)

    DEVICE_FUNCTION_LOAD(vkCmdBeginRenderingKHR);
    DEVICE_FUNCTION_LOAD(vkCmdEndRenderingKHR);
    DEVICE_FUNCTION_LOAD(vkSignalSemaphoreKHR);
    DEVICE_FUNCTION_LOAD(vkWaitSemaphoresKHR);
    DEVICE_FUNCTION_LOAD(vkGetSemaphoreCounterValueKHR);

    #undef DEVICE_FUNCTION_LOAD

    // Pull presentation queue
    if (info->enable_windowing) {
        vkGetDeviceQueue(hardware->logical_device, qf_info.presentation_family, 0, &hardware->presentation_queue);
        hardware->presentation_queue_family = qf_info.presentation_family;
    }

    // Init allocators
    if (!hardware_init_command_allocators(hardware, info)) goto _fail;
    if (!hardware_init_memory_allocators(hardware, info))  goto _fail;
    if (!hardware_init_shader_access(hardware, info))      goto _fail;

    // Return
    return hardware;

_fail: fnd_gfx_free_hardware(hardware); return NULL;
}

void fnd_gfx_free_hardware(fnd_gfx_hardware* hardware) {
    if (!hardware) return;
    hardware_free_command_allocators(hardware);
    hardware_free_memory_allocators(hardware);
    hardware_free_shader_access(hardware);
    vkDestroyDevice(hardware->logical_device, 0);
    free(hardware);
}

void fnd_gfx_hardware_wait_idle(fnd_gfx_hardware* hardware) {
    vkDeviceWaitIdle(hardware->logical_device);
}

// ===========================
// Command Allocation
// Hardware owned and managed

struct command_allocator {
    VkCommandPool   transfer_pool;
    VkCommandPool   compute_pool;
    VkCommandPool   graphics_pool;
};

// No thread safe, as fnd_gfx_create_hardware is not thread safe, therefore no need
int hardware_init_command_allocators(fnd_gfx_hardware* hardware, const fnd_gfx_hardware_create_info* info) {
    hardware->command_allocators = calloc(HARDWARE_COMMANDS_ALLOCATORS_COUNT, sizeof(command_allocator));
    if (!hardware->command_allocators) return 0;

    for (size_t i = 0; i < HARDWARE_COMMANDS_ALLOCATORS_COUNT; i++) {
        command_allocator* allocator = &hardware->command_allocators[i];
        *allocator = (command_allocator){
            .transfer_pool  = VK_NULL_HANDLE,
            .compute_pool   = VK_NULL_HANDLE,
            .graphics_pool  = VK_NULL_HANDLE
        };
    }

    hardware->command_allocators_mutex = fnd_thr_create_mutex(&(fnd_thr_mutex_create_info){});
    return 1;
}

// No thread safe, as fnd_gfx_free_hardware is not thread safe, therefore no need
void hardware_free_command_allocators(fnd_gfx_hardware* hardware) {
    for (size_t i = 0; i < HARDWARE_COMMANDS_ALLOCATORS_COUNT; i++) {
        command_allocator* allocator = &hardware->command_allocators[i];
        if (allocator->transfer_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(hardware->logical_device, allocator->transfer_pool, NULL);
            allocator->transfer_pool = VK_NULL_HANDLE;
        }
        if (allocator->compute_pool  != VK_NULL_HANDLE) {
            vkDestroyCommandPool(hardware->logical_device, allocator->compute_pool,  NULL);
            allocator->compute_pool = VK_NULL_HANDLE;
        }
        if (allocator->graphics_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(hardware->logical_device, allocator->graphics_pool, NULL);
            allocator->graphics_pool = VK_NULL_HANDLE;
        }
    }
    free(hardware->command_allocators);
    fnd_thr_free_mutex(hardware->command_allocators_mutex);
}

// Thread safe operation, returns VK_NULL_HANDLE on failure
VkCommandPool hardware_get_command_pool(fnd_gfx_hardware* hardware, fnd_gfx_command_domain requested_domain, uint8_t allocator_index) {
    // Lock structure access
    fnd_thr_mutex_lock(hardware->command_allocators_mutex);

    // Get allocator
    command_allocator* allocator = &hardware->command_allocators[allocator_index];

    // Ensure command poll within allocator exist
    VkCommandPool* result_source = NULL;
    switch (requested_domain) {
    case fnd_gfx_command_domain_transfer:  result_source = &allocator->transfer_pool;   break;
    case fnd_gfx_command_domain_compute:   result_source = &allocator->compute_pool;    break;
    case fnd_gfx_command_domain_graphics:  result_source = &allocator->graphics_pool;   break;
    default: goto _finish; // Invalid domain
    }

    // If pool is not yet created do create
    if (*result_source == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo command_pool_create_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = hardware->work_group_queue_family[requested_domain],
            .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        };

        if (vkCreateCommandPool(hardware->logical_device, &command_pool_create_info, 0, result_source) != VK_SUCCESS) {
            goto _finish; // failed to create pool, return
        }
    }

_finish:
    // Unlock access and return handle
    fnd_thr_mutex_unlock(hardware->command_allocators_mutex);
    if (result_source == NULL) return VK_NULL_HANDLE;
    return *result_source;
}

// ===========================
// Memory Allocation
// Hardware owned and managed

typedef struct memory_pool {
    VkDeviceSize            bytes;
    fnd_par_partitioner*    partitioner;
    VkDeviceMemory          memory;
    struct memory_pool*     next;
} memory_pool;

struct memory_allocator {
    fnd_thr_mutex*          mutex;          // allocator mutex
    VkDeviceSize            minimal_size;   // typical alloc size
    memory_pool*            first_pool;     // first memory pool
};

typedef struct memory_allocation {
    memory_allocator*       owning_allocator;
    memory_pool*            owning_pool;
    fnd_par_partition*      partition;
} memory_allocation;

// No thread safe, as fnd_gfx_create_hardware is not thread safe, therefore no need
int hardware_init_memory_allocators(fnd_gfx_hardware* hardware, const fnd_gfx_hardware_create_info* info) {
    vkGetPhysicalDeviceMemoryProperties(hardware->physical_device, &hardware->memory_properties);
    hardware->memory_allocators = calloc(hardware->memory_properties.memoryTypeCount, sizeof(memory_allocator));
    for (size_t i = 0; i < hardware->memory_properties.memoryTypeCount; i++) {
        hardware->memory_allocators[i].mutex = fnd_thr_create_mutex(&(fnd_thr_mutex_create_info){});
    }
    return 1;
}

// No thread safe, as fnd_gfx_free_hardware is not thread safe, therefore no need
void hardware_free_memory_allocators(fnd_gfx_hardware* hardware) {
    for (size_t i = 0; i < hardware->memory_properties.memoryTypeCount; i++) {
        memory_allocator* ma = &hardware->memory_allocators[i];
        while (ma->first_pool) {
            memory_pool* pool = ma->first_pool; ma->first_pool = pool->next;
            vkFreeMemory(hardware->logical_device, pool->memory, NULL);
            fnd_par_free_partitioner(pool->partitioner);
            free(pool);
        }
        fnd_thr_free_mutex(ma->mutex);
    }
    free(hardware->memory_allocators);
}

// driver sorts memory types from fastest to slowest
// pick first meeting requirements
static inline uint32_t find_memory_type(
    const VkPhysicalDeviceMemoryProperties* properties, 
    uint32_t                                type_filter, 
    VkMemoryPropertyFlags                   requirements, 
    uint32_t                                search_start
) {
    for (uint32_t i = search_start; i < properties->memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (properties->memoryTypes[i].propertyFlags & requirements) == requirements) return i;
    }
    return UINT32_MAX;  // failure
}

// Thread safe operation, returns non-zero at success
int hardware_get_memory(
    fnd_gfx_hardware* hardware, VkMemoryRequirements requirements, VkMemoryPropertyFlags properties, memory_allocation* out_alloc
) {
    uint32_t search_start = 0;

    do {
        // try all memory types, 
        // starting from optimal one
        uint32_t memory_type = find_memory_type(
            &hardware->memory_properties, 
            requirements.memoryTypeBits,
            properties,
            search_start
        );

        if (memory_type == UINT32_MAX) return 0; // failure, no suitable memory type
        memory_allocator* ma = &hardware->memory_allocators[memory_type];
        
        fnd_thr_mutex_lock(ma->mutex);   // Lock allocator for this type

        // Try suballocate exisiting pools
        fnd_par_partition* partition = NULL;
        memory_pool*   pool = ma->first_pool;
        while (pool) {
            partition = fnd_par_partitioner_alloc_partition(pool->partitioner, requirements.size, requirements.alignment);
            if (partition)          break;  // succeeded to suballocate
            if (pool->next == NULL) break;  // no next pool
            pool = pool->next;              // advance
        }

        // If failed, try to allocate new pool
        if (!partition) {
            // New pool size = max(new alloc, minimal alloc size)
            VkDeviceSize new_size = requirements.size > ma->minimal_size ? requirements.size : ma->minimal_size;

            // Create pool
            memory_pool* new_pool = calloc(1, sizeof(memory_pool));
            if (!new_pool) goto _unlock_mutex;
            new_pool->bytes = new_size;

            // Allocate gpu memory
            if (vkAllocateMemory(hardware->logical_device, &(VkMemoryAllocateInfo){
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .memoryTypeIndex = memory_type, .allocationSize = new_size
            }, NULL, &new_pool->memory) != VK_SUCCESS) {
                free(new_pool); goto _unlock_mutex;
            }

            // Create partitioner
            new_pool->partitioner = fnd_par_create_partitioner(&(fnd_par_partitioner_create_info){
                .memory_bytes = new_size
            }); if (!new_pool->partitioner) {
                vkFreeMemory(hardware->logical_device, new_pool->memory, NULL);
                free(new_pool); goto _unlock_mutex;
            }

            // Append new page back
            if (pool) pool->next     = new_pool;
            else      ma->first_pool = new_pool;

            // Suballocate new pool
            partition = fnd_par_partitioner_alloc_partition(new_pool->partitioner, requirements.size, requirements.alignment);
            pool = new_pool;
        }

    _unlock_mutex:
        fnd_thr_mutex_unlock(ma->mutex); // Unlock this allocator
        
        // If succeeded to suballocate
        // return partition
        if (partition) {
            *out_alloc = (memory_allocation){
                .owning_allocator = ma,
                .owning_pool      = pool,
                .partition        = partition
            };
            return 1;
        }

        // Fallback to suboptimal types
        // Start search after this type
        // not to enter infinite loop
        search_start = memory_type + 1;
    } while(1);

    return 0; // no type succeeded at allocation
}

// Thread safe operation
void hardware_free_memory(fnd_gfx_hardware* hardware, memory_allocation alloc) {
    if (!alloc.owning_allocator) return;        // so buffer fail paths work
    fnd_thr_mutex_lock(alloc.owning_allocator->mutex);   // lock allocator for this type
    fnd_par_partitioner_free_partition(alloc.owning_pool->partitioner, alloc.partition);
    fnd_thr_mutex_unlock(alloc.owning_allocator->mutex); // unlock this allocator
}

// ===========================
// Shader Access
// Hardware owned and managed

VkDescriptorType fnd_gfx_resource_type_to_vk_descriptor_type(fnd_gfx_resource_type type) {
    switch (type) {
    case fnd_gfx_resource_type_uniform_buffer:  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case fnd_gfx_resource_type_storage_buffer:  return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case fnd_gfx_resource_type_sampled_texture: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case fnd_gfx_resource_type_storage_texture: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case fnd_gfx_resource_type_sampler:         return VK_DESCRIPTOR_TYPE_SAMPLER;
    default: assert(0 && "Invalid fnd_gfx_resource_type!");
    } return 0;
}

struct bindless_allocator {
    fnd_thr_mutex*  mutex;            // allocator mutex
    uint32_t        free_capacity;    // count of slots
    uint32_t        free_count;       // stack elements count
    uint32_t*       free_stack;       // stack of freed indices
};

// No thread safe, as fnd_gfx_create_hardware is not thread safe, therefore no need
int hardware_init_shader_access(fnd_gfx_hardware* hardware, const fnd_gfx_hardware_create_info* info) {
    VkDescriptorBindingFlags binding_flags[fnd_gfx_resource_type_count];

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount   = fnd_gfx_resource_type_count,
        .pBindingFlags  = binding_flags
    };

    VkDescriptorSetLayoutBinding* bindings = malloc(fnd_gfx_resource_type_count * sizeof(VkDescriptorSetLayoutBinding));
    for (uint32_t type = 0; type < fnd_gfx_resource_type_count; type++) {
        binding_flags[type] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
        bindings[type]      = (VkDescriptorSetLayoutBinding){
            .binding            = type,
            .descriptorCount    = info->shader_resources_limit[type],
            .descriptorType     = fnd_gfx_resource_type_to_vk_descriptor_type(type),
            .stageFlags         = VK_SHADER_STAGE_ALL
        };
    }
    
    if (vkCreateDescriptorSetLayout(hardware->logical_device, &(VkDescriptorSetLayoutCreateInfo){
        .sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext          = &binding_flags_info,
        .flags          = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount   = fnd_gfx_resource_type_count,
        .pBindings      = bindings
    }, NULL, &hardware->bindless_descriptor_layout) != VK_SUCCESS) {
        free(bindings); return 0;
    } free(bindings);

    uint32_t sizes_count = 0;
    VkDescriptorPoolSize* sizes = malloc(fnd_gfx_resource_type_count * sizeof(VkDescriptorPoolSize));
    for (uint32_t type = 0; type < fnd_gfx_resource_type_count; type++) {
        if (info->shader_resources_limit[type] == 0) continue; // due to spec
        sizes[sizes_count++] = (VkDescriptorPoolSize){
            .descriptorCount    = info->shader_resources_limit[type],
            .type               = fnd_gfx_resource_type_to_vk_descriptor_type(type)
        };
    }

    if (vkCreateDescriptorPool(hardware->logical_device, &(VkDescriptorPoolCreateInfo){
        .sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags          = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets        = 1,
        .poolSizeCount  = sizes_count,
        .pPoolSizes     = sizes
    }, NULL, &hardware->bindless_descriptor_pool) != VK_SUCCESS) {
        free(sizes); return 0;
    } free(sizes);

    if (vkAllocateDescriptorSets(hardware->logical_device, &(VkDescriptorSetAllocateInfo){
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .descriptorPool     = hardware->bindless_descriptor_pool,
        .pSetLayouts        = &hardware->bindless_descriptor_layout,
    }, &hardware->bindless_descriptor) != VK_SUCCESS) return 0;
    
    hardware->bindless_allocators = calloc(fnd_gfx_resource_type_count, sizeof(bindless_allocator));
    if (!hardware->bindless_allocators) return 0;
    for (uint32_t type = 0; type < fnd_gfx_resource_type_count; type++) {
        bindless_allocator* ba = &hardware->bindless_allocators[type];
        uint32_t cap = info->shader_resources_limit[type];
        *ba = (bindless_allocator){
            .mutex          = fnd_thr_create_mutex(&(fnd_thr_mutex_create_info){}),
            .free_capacity  = cap,
            .free_count     = cap,
            .free_stack     = malloc(cap * sizeof(uint32_t))
        };
        for (uint32_t i = 0; i < cap; i++) ba->free_stack[i] = i;
    }

    return 1;
}

void hardware_free_shader_access(fnd_gfx_hardware* hardware) {
    vkDestroyDescriptorSetLayout(hardware->logical_device, hardware->bindless_descriptor_layout, NULL);
    vkDestroyDescriptorPool(hardware->logical_device, hardware->bindless_descriptor_pool, NULL);
    for (uint32_t type = 0; type < fnd_gfx_resource_type_count; type++) {
        fnd_thr_free_mutex(hardware->bindless_allocators[type].mutex);
        free(hardware->bindless_allocators[type].free_stack);
    }
    free(hardware->bindless_allocators);
}

typedef struct resource_bind_cache {
    uint32_t bind_mask;                             // bit[resource type index] -> whether was bound
    uint32_t indices[fnd_gfx_resource_type_count];  // bind index
} resource_bind_cache;

resource_bind_cache* get_buffer_resource_bind_cache (fnd_gfx_buffer*  buffer);
resource_bind_cache* get_texture_resource_bind_cache(fnd_gfx_texture* texture);
resource_bind_cache* get_sampler_resource_bind_cache(fnd_gfx_sampler* sampler);

VkBuffer    get_native_buffer_handle (fnd_gfx_buffer*  buffer);
VkImageView get_native_texture_handle(fnd_gfx_texture* texture);
VkSampler   get_native_sampler_handle(fnd_gfx_sampler* sampler);

// Thread safe operation
uint32_t fnd_gfx_shader_resource_bind(fnd_gfx_hardware* hardware, fnd_gfx_resource_type type, void* resource, int* success) {
    bindless_allocator*  ba = &hardware->bindless_allocators[type];
    resource_bind_cache* bc;
    switch (type) {
        case fnd_gfx_resource_type_uniform_buffer: case fnd_gfx_resource_type_storage_buffer: {
            bc = get_buffer_resource_bind_cache(resource);
        } break;
        case fnd_gfx_resource_type_sampled_texture: case fnd_gfx_resource_type_storage_texture: {
            bc = get_texture_resource_bind_cache(resource);
        } break;
        case fnd_gfx_resource_type_sampler: {
            bc = get_sampler_resource_bind_cache(resource);
        } break;
        default: assert(0 && "Invalid fnd_gfx_resource_type!");
    }

    // Was bound
    if ((bc->bind_mask >> type) & 1U) {
        return bc->indices[type];
    }

    // Find and take free index
    fnd_thr_mutex_lock(ba->mutex);

    // Check again if bound
    // May happen if two threds try to bind the same resource for the first time
    if ((bc->bind_mask >> type) & 1U) {
        fnd_thr_mutex_unlock(ba->mutex); return bc->indices[type];
    }

    // Failed to allocate index
    if (ba->free_count == 0) {
        fnd_thr_mutex_unlock(ba->mutex); *success &= 0; return 0; // Arbitrary
    }

    // Obtain index
    uint32_t idx = ba->free_stack[ba->free_count - 1];
    ba->free_count--;

    // Assign bind index
    bc->indices[type] = idx;
    bc->bind_mask |= (1U << type);

    fnd_thr_mutex_unlock(ba->mutex);
    
    // Resouce Info
    VkDescriptorBufferInfo  buffer_info;
    VkDescriptorImageInfo   image_info;

    VkDescriptorBufferInfo* buffer_info_ptr = NULL;
    VkDescriptorImageInfo*  image_info_ptr  = NULL;

    switch (type) {
    case fnd_gfx_resource_type_uniform_buffer: case fnd_gfx_resource_type_storage_buffer:{
        fnd_gfx_buffer* buffer_resource = resource;
        buffer_info = (VkDescriptorBufferInfo){
            .buffer = get_native_buffer_handle(buffer_resource),
            .offset = 0,
            .range  = VK_WHOLE_SIZE
        };
        buffer_info_ptr = &buffer_info;   
    } break;
    case fnd_gfx_resource_type_sampled_texture: case fnd_gfx_resource_type_storage_texture: {
        fnd_gfx_texture* texture_resource = resource;
        image_info = (VkDescriptorImageInfo){
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView   = get_native_texture_handle(texture_resource),
            .sampler     = VK_NULL_HANDLE
        };
        image_info_ptr = &image_info;
    } break;
    case fnd_gfx_resource_type_sampler: {
        fnd_gfx_sampler* sampler_resource = resource;
        image_info = (VkDescriptorImageInfo){
            .imageLayout = 0,
            .imageView   = VK_NULL_HANDLE,
            .sampler     = get_native_sampler_handle(sampler_resource)
        };
        image_info_ptr = &image_info;
    } break;
    }

    // Write GPU
    vkUpdateDescriptorSets(hardware->logical_device, 1, &(VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet             = hardware->bindless_descriptor,
        .dstBinding         = type,
        .dstArrayElement    = idx,
        .descriptorType     = fnd_gfx_resource_type_to_vk_descriptor_type(type),
        .descriptorCount    = 1,
        .pBufferInfo        = buffer_info_ptr,
        .pImageInfo         = image_info_ptr,
        .pTexelBufferView   = NULL
    }, 0, NULL);
    
    *success &= 1;
    return idx;
}

// Thread safe operation
void shader_resource_unbind(fnd_gfx_hardware* hardware, resource_bind_cache* bc) {
    for (uint32_t type = 0; type < fnd_gfx_resource_type_count; type++) {
        bindless_allocator* ba = &hardware->bindless_allocators[type];
        if (!((bc->bind_mask >> type) & 1U)) continue; // Was not bound

        // Release index
        fnd_thr_mutex_lock(ba->mutex);
        ba->free_stack[ba->free_count++] = bc->indices[type];
        fnd_thr_mutex_unlock(ba->mutex);
    }
}

// ===========================
// Timeline

struct fnd_gfx_timeline {
    fnd_gfx_hardware*   owning_hardware;
    VkSemaphore     timeline_semaphore;
};

fnd_gfx_timeline* fnd_gfx_create_timeline(fnd_gfx_hardware* hardware, const fnd_gfx_timeline_create_info* info) {
    fnd_gfx_timeline* timeline = malloc(sizeof(fnd_gfx_timeline));
    if (!timeline) return NULL; *timeline = (fnd_gfx_timeline){.owning_hardware = hardware};

    VkSemaphoreTypeCreateInfoKHR type_info = {
        .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR,
        .pNext         = NULL,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue  = info->initial_value,
    };

    if (vkCreateSemaphore(hardware->logical_device, &(VkSemaphoreCreateInfo){
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &type_info,
        .flags = 0,
    }, NULL, &timeline->timeline_semaphore) != VK_SUCCESS) goto _fail;

    return timeline;
_fail: fnd_gfx_free_timeline(timeline); return NULL;
}

void fnd_gfx_free_timeline(fnd_gfx_timeline* timeline) {
    if (!timeline) return;
    vkDestroySemaphore(timeline->owning_hardware->logical_device, timeline->timeline_semaphore, NULL);
    free(timeline);
}

void fnd_gfx_timeline_signal(fnd_gfx_timeline* timeline, uint64_t value) {
    timeline->owning_hardware->vkSignalSemaphoreKHR(timeline->owning_hardware->logical_device, &(VkSemaphoreSignalInfoKHR){
        .sType      = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO_KHR,
        .semaphore  = timeline->timeline_semaphore,
        .value      = value
    });
}

void fnd_gfx_timeline_wait(fnd_gfx_timeline* timeline, uint64_t value) {
    timeline->owning_hardware->vkWaitSemaphoresKHR(timeline->owning_hardware->logical_device, &(VkSemaphoreWaitInfoKHR){
        .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR,
        .flags          = 0,
        .semaphoreCount = 1,
        .pSemaphores    = &timeline->timeline_semaphore,
        .pValues        = &value,
    }, UINT64_MAX);
}

int fnd_gfx_timeline_is_after(fnd_gfx_timeline* timeline, uint64_t value) {
    return fnd_gfx_timeline_get_value(timeline) >= value;
}

uint64_t fnd_gfx_timeline_get_value(fnd_gfx_timeline* timeline) {
    uint64_t value = 0;
    if (timeline->owning_hardware->vkGetSemaphoreCounterValueKHR(
        timeline->owning_hardware->logical_device, timeline->timeline_semaphore, &value
    ) != VK_SUCCESS) return 0;
    return value;
}

// ===========================
// Command List

// Command list recording target
static fnd_thr_thread_local fnd_gfx_commands* recording_state_commands = NULL;

struct fnd_gfx_commands {
    fnd_gfx_hardware*       owning_hardware;
    VkCommandPool       owning_pool;
    uint8_t             allocator_index;
    fnd_gfx_command_domain  command_domain;
    VkCommandBuffer     command_buffer;
};

fnd_gfx_commands* fnd_gfx_create_commands(fnd_gfx_hardware* hardware, const fnd_gfx_commands_create_info* info) {
    fnd_gfx_commands* list = NULL;

    // If no parent to reuse, create new list
    if (!info->parent) {
        list = calloc(1, sizeof(fnd_gfx_commands));
        if (!list) return NULL;

        VkCommandPool pool = hardware_get_command_pool(hardware, info->domain, info->aindex);
        if (pool == VK_NULL_HANDLE) {
            free(list); return NULL;
        }

        VkCommandBufferAllocateInfo allocation_info = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = pool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        if (vkAllocateCommandBuffers(hardware->logical_device, &allocation_info, &list->command_buffer) != VK_SUCCESS) {
            free(list); return NULL;
        }

        *list = (fnd_gfx_commands){
            .owning_hardware = hardware,
            .owning_pool     = pool,
            .allocator_index = info->aindex,
            .command_domain  = info->domain,
            .command_buffer  = list->command_buffer,
        };
    }
    // Reuse parent object
    else {
        list = info->parent;
    }

    // Record Command List
    if (vkResetCommandBuffer(
        list->command_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT) != VK_SUCCESS
    ) goto _fail;   // failed to reset, no recording can be made

    if (vkBeginCommandBuffer(list->command_buffer, &(VkCommandBufferBeginInfo){
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags              = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        .pInheritanceInfo   = 0
    }) != VK_SUCCESS) goto _fail; // failed to begin recording command buffer

    recording_state_commands = list;
    info->record(info->params);
    recording_state_commands = NULL;

    if (vkEndCommandBuffer(list->command_buffer) != VK_SUCCESS) goto _fail; // failed to record command buffer
    return list;    // Return List

_fail: fnd_gfx_free_commands(list); return NULL;
}

void fnd_gfx_free_commands(fnd_gfx_commands* list) {
    if (!list) return;
    vkFreeCommandBuffers(
        list->owning_hardware->logical_device,
        list->owning_pool, 1, &list->command_buffer
    );
    free(list);
}

void fnd_gfx_commands_submit(uint32_t count, fnd_gfx_commands** lists, const fnd_gfx_submit_info* info) {
    if (count == 0) return;

    VkQueue queue = lists[0]->owning_hardware->work_group_queue[lists[0]->command_domain][info->domain_work_group];
    if (queue == VK_NULL_HANDLE) return;

    VkCommandBuffer* command_buffers = malloc(count * sizeof(VkCommandBuffer));
    for (uint32_t i = 0; i < count; i++) {
        command_buffers[i] = lists[i]->command_buffer;
    }

    VkSemaphore*            wait_semaphores  = NULL;
    VkPipelineStageFlags*   wait_stages      = NULL;
    if (info->wait_count > 0) {
        wait_semaphores = malloc(info->wait_count * sizeof(VkSemaphore));
        wait_stages     = malloc(info->wait_count * sizeof(VkPipelineStageFlags));
        for (uint32_t i = 0; i < info->wait_count; i++) {
            wait_semaphores[i] = info->wait_timelines[i]->timeline_semaphore;
            wait_stages[i]     = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }
    }

    VkSemaphore* signal_semaphores = NULL;
    if (info->signal_count > 0) {
        signal_semaphores = malloc(info->signal_count * sizeof(VkSemaphore));
        for (uint32_t i = 0; i < info->signal_count; i++) {
            signal_semaphores[i] = info->signal_timelines[i]->timeline_semaphore;
        }
    }

    VkTimelineSemaphoreSubmitInfoKHR timeline_info = {
        .sType                      = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .waitSemaphoreValueCount    = info->wait_count,
        .pWaitSemaphoreValues       = info->wait_values,
        .signalSemaphoreValueCount  = info->signal_count,
        .pSignalSemaphoreValues     = info->signal_values,
    };

    VkSubmitInfo submit_info = {
        .sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                  = &timeline_info,
        .commandBufferCount     = count,
        .pCommandBuffers        = command_buffers,
        .waitSemaphoreCount     = info->wait_count,
        .pWaitSemaphores        = wait_semaphores,
        .pWaitDstStageMask      = wait_stages,
        .signalSemaphoreCount   = info->signal_count,
        .pSignalSemaphores      = signal_semaphores,
    };

    vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);

    free(command_buffers);  free(wait_semaphores);
    free(wait_stages);      free(signal_semaphores);
}

// ===========================
// Staging

struct fnd_gfx_staging {
    fnd_gfx_hardware*       owning_hardware;
    memory_allocation   allocation;
    VkBuffer            buffer;
};

fnd_gfx_staging* fnd_gfx_create_staging(fnd_gfx_hardware* hardware, const fnd_gfx_staging_create_info* info) {
    fnd_gfx_staging* staging = calloc(1, sizeof(fnd_gfx_staging));
    if (!staging) goto _fail;

    *staging = (fnd_gfx_staging){
        .owning_hardware = hardware
    };

    if (vkCreateBuffer(hardware->logical_device, &(VkBufferCreateInfo){
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = info->bytes,
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    }, 0, &staging->buffer) != VK_SUCCESS) goto _fail;

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(hardware->logical_device, staging->buffer, &memory_requirements);

    if (!hardware_get_memory(
        hardware, memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging->allocation
    )) goto _fail;

    if (vkBindBufferMemory(
        hardware->logical_device, staging->buffer,
        staging->allocation.owning_pool->memory,
        fnd_par_partition_query_offset(staging->allocation.partition)
    ) != VK_SUCCESS) goto _fail;

    return staging;

_fail: fnd_gfx_free_staging(staging); return NULL;
}

void fnd_gfx_free_staging(fnd_gfx_staging* staging) {
    if (!staging) return;
    vkDestroyBuffer(staging->owning_hardware->logical_device, staging->buffer, 0);
    hardware_free_memory(staging->owning_hardware, staging->allocation);
    free(staging);
}

void* fnd_gfx_staging_map(fnd_gfx_staging* memory, uint64_t region_offset, uint64_t region_size) {
    void* data; if (vkMapMemory(
        memory->owning_hardware->logical_device, 
        memory->allocation.owning_pool->memory, region_offset, region_size, 0, &data
    ) != VK_SUCCESS) return 0x0;
    return data;
}

void fnd_gfx_staging_unmap(fnd_gfx_staging* memory) {
    vkUnmapMemory(memory->owning_hardware->logical_device, memory->allocation.owning_pool->memory);
}

// ===========================
// Buffer

static inline VkBufferUsageFlags fnd_gfx_memory_access_to_vk_buffer_usage(fnd_gfx_memory_access access) {
    switch (access) {
    case fnd_gfx_memory_access_rendering_internal:      return 0;
    case fnd_gfx_memory_access_staging_read:            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    case fnd_gfx_memory_access_staging_write:           return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case fnd_gfx_memory_access_staging_read_and_write:  return VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    default: assert(0 && "Invalid fnd_gfx_memory_access!");
    } return 0;
}

static inline VkBufferUsageFlags fnd_gfx_buffer_usage_to_vk_buffer_usage(fnd_gfx_buffer_usage usage) {
    switch (usage) {
        case fnd_gfx_buffer_usage_uniform:  return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        case fnd_gfx_buffer_usage_storage:  return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        default: assert(0 && "invalid fnd_gfx_buffer_usage");
    } return 0;
}

struct fnd_gfx_buffer {
    fnd_gfx_hardware*       owning_hardware;
    memory_allocation   allocation;
    VkBuffer            buffer;
    resource_bind_cache bind_cache;
};

VkBuffer get_native_buffer_handle(fnd_gfx_buffer* buffer) {
    return buffer->buffer;
}

resource_bind_cache* get_buffer_resource_bind_cache (fnd_gfx_buffer*  buffer) {
    return &buffer->bind_cache;
}

fnd_gfx_buffer* fnd_gfx_create_buffer(fnd_gfx_hardware* hardware, const fnd_gfx_buffer_create_info* info) {
    fnd_gfx_buffer* buffer = malloc(sizeof(fnd_gfx_buffer));
    if (!buffer) goto _fail; *buffer = (fnd_gfx_buffer){.owning_hardware = hardware};

    if (vkCreateBuffer(hardware->logical_device, &(VkBufferCreateInfo){
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = info->bytes,
        .usage       = fnd_gfx_buffer_usage_to_vk_buffer_usage(info->usage) | fnd_gfx_memory_access_to_vk_buffer_usage(info->access),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    }, 0, &buffer->buffer) != VK_SUCCESS) goto _fail;

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(hardware->logical_device, buffer->buffer, &memory_requirements);

    if (!hardware_get_memory(
        hardware, memory_requirements, 0, &buffer->allocation
    )) goto _fail;

    if (vkBindBufferMemory(
        hardware->logical_device, buffer->buffer,
        buffer->allocation.owning_pool->memory,
        fnd_par_partition_query_offset(buffer->allocation.partition)
    ) != VK_SUCCESS) goto _fail;

    return buffer;
_fail: fnd_gfx_free_buffer(buffer); return NULL;
}

void fnd_gfx_free_buffer(fnd_gfx_buffer* buffer) {
    if (!buffer) return;
    shader_resource_unbind(buffer->owning_hardware, &buffer->bind_cache);
    vkDestroyBuffer(buffer->owning_hardware->logical_device, buffer->buffer, 0);
    hardware_free_memory(buffer->owning_hardware, buffer->allocation);
    free(buffer);
}

uint64_t fnd_gfx_buffer_query_bytes(fnd_gfx_buffer* buffer) {
    return fnd_par_partition_query_size(buffer->allocation.partition);
}

// ===========================
// Texture

struct fnd_gfx_texture {
    fnd_gfx_hardware*           owning_hardware;
    memory_allocation           allocation;
    fnd_gfx_texture_dimensions  dimensions;
    fnd_gfx_texture_format      format;
    uint32_t                    mip_levels;
    uint32_t                    layers;
    VkImage                     image;
    VkImageView                 view;
    resource_bind_cache         bind_cache;
};

VkImageView get_native_texture_handle(fnd_gfx_texture* texture) {
    return texture->view;
}

resource_bind_cache* get_texture_resource_bind_cache(fnd_gfx_texture* texture) {
    return &texture->bind_cache;
}

static inline VkFormat fnd_gfx_to_vk_texture_format(fnd_gfx_texture_format format) {
    switch (format) {
        case fnd_gfx_texture_format_undefined:              return VK_FORMAT_UNDEFINED;

        case fnd_gfx_texture_format_r8_unorm:               return VK_FORMAT_R8_UNORM;
        case fnd_gfx_texture_format_rg8_unorm:              return VK_FORMAT_R8G8_UNORM;
        case fnd_gfx_texture_format_rgba8_unorm:            return VK_FORMAT_R8G8B8A8_UNORM;
        case fnd_gfx_texture_format_rgba8_srgb:             return VK_FORMAT_R8G8B8A8_SRGB;
        case fnd_gfx_texture_format_bgra8_unorm:            return VK_FORMAT_B8G8R8A8_UNORM;
        case fnd_gfx_texture_format_bgra8_srgb:             return VK_FORMAT_B8G8R8A8_SRGB;

        case fnd_gfx_texture_format_r16_float:              return VK_FORMAT_R16_SFLOAT;
        case fnd_gfx_texture_format_rg16_float:             return VK_FORMAT_R16G16_SFLOAT;
        case fnd_gfx_texture_format_rgba16_float:           return VK_FORMAT_R16G16B16A16_SFLOAT;

        case fnd_gfx_texture_format_r32_float:              return VK_FORMAT_R32_SFLOAT;
        case fnd_gfx_texture_format_rg32_float:             return VK_FORMAT_R32G32_SFLOAT;
        case fnd_gfx_texture_format_rgba32_float:           return VK_FORMAT_R32G32B32A32_SFLOAT;

        case fnd_gfx_texture_format_depth16_unorm:          return VK_FORMAT_D16_UNORM;
        case fnd_gfx_texture_format_depth24_unorm_stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
        case fnd_gfx_texture_format_depth32_float:          return VK_FORMAT_D32_SFLOAT;

        default: assert(0 && "Invalid fnd_gfx_texture_format!");
    } return VK_FORMAT_UNDEFINED;
}

static inline VkImageType fnd_gfx_texture_type_to_vk_image_type(fnd_gfx_texture_type type) {
    switch (type) {
        case fnd_gfx_texture_type_1d:       return VK_IMAGE_TYPE_1D;
        case fnd_gfx_texture_type_2d:       return VK_IMAGE_TYPE_2D;
        case fnd_gfx_texture_type_cubemap:  return VK_IMAGE_TYPE_2D;
        case fnd_gfx_texture_type_3d:       return VK_IMAGE_TYPE_3D;
        default: assert(0 && "Invalid fnd_gfx_texture_type");
    } return VK_IMAGE_TYPE_2D;
}

static inline VkImageUsageFlags fnd_gfx_memory_access_to_vk_image_usage(fnd_gfx_memory_access access) {
    switch (access) {
    case fnd_gfx_memory_access_rendering_internal:      return 0;
    case fnd_gfx_memory_access_staging_read:            return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    case fnd_gfx_memory_access_staging_write:           return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    case fnd_gfx_memory_access_staging_read_and_write:  return VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    default: assert(0 && "Invalid fnd_gfx_memory_access!");
    } return 0;
}

static inline VkImageUsageFlags fnd_gfx_texture_usage_to_vk_image_usage(fnd_gfx_texture_usage usage) {
    switch (usage) {
    case fnd_gfx_texture_usage_sampled:                     return VK_IMAGE_USAGE_SAMPLED_BIT;
    case fnd_gfx_texture_usage_color_attachment:            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    case fnd_gfx_texture_usage_depth_stencil_attachment:    return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    case fnd_gfx_texture_usage_storage:                     return VK_IMAGE_USAGE_STORAGE_BIT;
    default: assert(0 && "Invalid fnd_gfx_texture_usage!");
    } return 0;
}

fnd_gfx_texture* fnd_gfx_create_texture(fnd_gfx_hardware* hardware, const fnd_gfx_texture_create_info* info) {
    fnd_gfx_texture* texture = malloc(sizeof(fnd_gfx_texture)); if (!texture) goto _fail;
    *texture = (fnd_gfx_texture){
        .owning_hardware = hardware, 
        .dimensions = info->dimensions,
        .mip_levels = info->mipmap_layers,
        .layers     = info->array_length,
        .format     = info->format
    };

    if (vkCreateImage(hardware->logical_device, &(VkImageCreateInfo){
        .sType          = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType      = fnd_gfx_texture_type_to_vk_image_type(info->type),
        .extent.width   = info->dimensions.width,
        .extent.height  = info->dimensions.height,
        .extent.depth   = info->dimensions.depth,
        .mipLevels      = info->mipmap_layers,
        .arrayLayers    = info->array_length,
        .format         = fnd_gfx_to_vk_texture_format(info->format),
        .tiling         = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage          = fnd_gfx_texture_usage_to_vk_image_usage(info->usage) | fnd_gfx_memory_access_to_vk_image_usage(info->memory_access),
        .sharingMode    = VK_SHARING_MODE_EXCLUSIVE,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .flags          = 0,
    }, 0, &texture->image) != VK_SUCCESS) goto _fail;

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(hardware->logical_device, texture->image, &memory_requirements);
    if (!hardware_get_memory(hardware, memory_requirements, 0, &texture->allocation)) goto _fail;
    vkBindImageMemory(
        hardware->logical_device, texture->image, 
        texture->allocation.owning_pool->memory, 
        fnd_par_partition_query_offset(texture->allocation.partition)
    );

    if (vkCreateImageView(hardware->logical_device, &(VkImageViewCreateInfo){
        .sType                              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                              = texture->image,
        .viewType                           = VK_IMAGE_VIEW_TYPE_2D,
        .format                             = fnd_gfx_to_vk_texture_format(info->format),
        .subresourceRange.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel      = 0,
        .subresourceRange.levelCount        = 1,
        .subresourceRange.baseArrayLayer    = 0,
        .subresourceRange.layerCount        = 1,
    }, 0, &texture->view) != VK_SUCCESS) goto _fail;

    return texture;
_fail: fnd_gfx_free_texture(texture); return NULL;
};

void fnd_gfx_free_texture(fnd_gfx_texture* texture) {
    if (!texture) return;
    shader_resource_unbind(texture->owning_hardware, &texture->bind_cache);
    vkDestroyImageView(texture->owning_hardware->logical_device, texture->view, NULL);
    vkDestroyImage(texture->owning_hardware->logical_device, texture->image, NULL);
    hardware_free_memory(texture->owning_hardware, texture->allocation);
    free(texture);
}

fnd_gfx_texture_dimensions fnd_gfx_texture_query_dimensions(fnd_gfx_texture* texture) {
    return texture->dimensions;
}

fnd_gfx_texture_format fnd_gfx_texture_query_format(fnd_gfx_texture* texture) {
    return texture->format;
}

// ===========================
// Sampler

static inline VkFilter fnd_gfx_to_vk_filter(fnd_gfx_sampler_filter filter) {
    switch (filter) {
    case fnd_gfx_sampler_filter_nearest:    return VK_FILTER_NEAREST;
    case fnd_gfx_sampler_filter_linear:     return VK_FILTER_LINEAR;
    default: assert(0 && "Invalid fnd_gfx_sampler_filter!");
    } return 0;
}

static inline VkSamplerMipmapMode fnd_gfx_to_vk_mipmap_mode(fnd_gfx_sampler_filter filter) {
    switch (filter) {
    case fnd_gfx_sampler_filter_nearest:    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case fnd_gfx_sampler_filter_linear:     return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    default: assert(0 && "Invalid fnd_gfx_sampler_filter!");
    } return 0;
}

static inline VkSamplerAddressMode fnd_gfx_to_vk_sampler_wrapping(fnd_gfx_sampler_wrapping wrapping) {
    switch (wrapping) {
        case fnd_gfx_sampler_wrapping_repeat:                   return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case fnd_gfx_sampler_wrapping_repeat_mirrored:          return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case fnd_gfx_sampler_wrapping_repeat_clamp_coordinates: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case fnd_gfx_sampler_wrapping_repeat_clamp_texture:     return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default: assert(0 && "Invalid fnd_gfx_sampler_wrapping!");
    } return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

struct fnd_gfx_sampler {
    fnd_gfx_hardware*       owning_hardware;
    VkSampler           sampler;
    resource_bind_cache bind_cache;
};

VkSampler get_native_sampler_handle(fnd_gfx_sampler* sampler) {
    return sampler->sampler;
}

resource_bind_cache* get_sampler_resource_bind_cache(fnd_gfx_sampler* sampler) {
    return &sampler->bind_cache;
}

fnd_gfx_sampler* fnd_gfx_create_sampler(fnd_gfx_hardware* hardware, const fnd_gfx_sampler_create_info* info) {
    fnd_gfx_sampler* sampler = malloc(sizeof(fnd_gfx_sampler)); if (!sampler) goto _fail;
    *sampler = (fnd_gfx_sampler){.owning_hardware = hardware};

    if (vkCreateSampler(hardware->logical_device, &(VkSamplerCreateInfo){
        .sType  = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = fnd_gfx_to_vk_filter(info->mag_filter),
        .minFilter               = fnd_gfx_to_vk_filter(info->min_filter),
        .mipmapMode              = fnd_gfx_to_vk_mipmap_mode(info->mipmap_filter),
        .addressModeU            = fnd_gfx_to_vk_sampler_wrapping(info->x_coord_wrapping),
        .addressModeV            = fnd_gfx_to_vk_sampler_wrapping(info->y_coord_wrapping),
        .addressModeW            = fnd_gfx_to_vk_sampler_wrapping(info->z_coord_wrapping),
        .mipLodBias              = info->mip_lod_bias,
        .anisotropyEnable        = VK_FALSE,    // todo!
        .maxAnisotropy           = hardware->physical_device_properties.limits.maxSamplerAnisotropy,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .minLod                  = info->min_lod,
        .maxLod                  = info->max_lod,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = info->unnormalized_coordinates ? VK_TRUE : VK_FALSE
    }, 0, &sampler->sampler) != VK_SUCCESS) goto _fail;

    return sampler;
_fail: fnd_gfx_free_sampler(sampler); return NULL;
}

void fnd_gfx_free_sampler(fnd_gfx_sampler* sampler) {
    if (!sampler) return;
    shader_resource_unbind(sampler->owning_hardware, &sampler->bind_cache);
    vkDestroySampler(sampler->owning_hardware->logical_device, sampler->sampler, 0);
    free(sampler);
}

// ===========================
// Shader

struct fnd_gfx_shader {
    fnd_gfx_hardware*   owning_hardware;
    VkShaderModule  module;
};

fnd_gfx_shader* fnd_gfx_create_shader(fnd_gfx_hardware* hardware, const fnd_gfx_shader_create_info* info) {
    fnd_gfx_shader* shader = calloc(1, sizeof(fnd_gfx_shader)); if (!shader) goto _fail;
    *shader = (fnd_gfx_shader){.owning_hardware = hardware};

    if (vkCreateShaderModule(hardware->logical_device, &(VkShaderModuleCreateInfo){
        .sType      = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize   = info->source_size,
        .pCode      = (uint32_t*)info->source_code
    }, 0, &shader->module) != VK_SUCCESS) {
        return 0x0; // failed to create shader module
    }

    return shader;
_fail: fnd_gfx_free_shader(shader); return NULL;
}

void fnd_gfx_free_shader(fnd_gfx_shader* shader) {
    if (shader == 0x0) return;
    vkDestroyShaderModule(shader->owning_hardware->logical_device, shader->module, 0);
    free(shader);
}

// ===========================
// Graphics Pipeline

static inline VkShaderStageFlags fnd_gfx_to_vk_shader_stage(fnd_gfx_shader_stage stage) {
    switch (stage) {
    case fnd_gfx_shader_stage_vertex:   return VK_SHADER_STAGE_VERTEX_BIT;
    case fnd_gfx_shader_stage_geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
    case fnd_gfx_shader_stage_pixel:    return VK_SHADER_STAGE_FRAGMENT_BIT;
    default: assert(0 && "Invalid fnd_gfx_shader_stage!");
    } return VK_SHADER_STAGE_VERTEX_BIT;
}

static inline VkPrimitiveTopology fnd_gfx_to_vk_primitive_topology(fnd_gfx_primitive_topology topology) {
    switch (topology) {
        case fnd_gfx_primitive_topology_point_list:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case fnd_gfx_primitive_topology_line_list:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case fnd_gfx_primitive_topology_line_strip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case fnd_gfx_primitive_topology_triangle_list:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case fnd_gfx_primitive_topology_triangle_strip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        default: assert(0 && "Invalid fnd_gfx_primitive_topology!");
    } return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

static inline VkCullModeFlags fnd_gfx_to_vk_cull_mode(fnd_gfx_cull_mode mode) {
    switch (mode) {
        case fnd_gfx_cull_mode_none:            return VK_CULL_MODE_NONE;
        case fnd_gfx_cull_mode_front:           return VK_CULL_MODE_FRONT_BIT;
        case fnd_gfx_cull_mode_back:            return VK_CULL_MODE_BACK_BIT;
        case fnd_gfx_cull_mode_front_and_back:  return VK_CULL_MODE_FRONT_AND_BACK;
        default: assert(0 && "Invalid fnd_gfx_cull_mode!");
    } return VK_CULL_MODE_NONE;
}

static inline VkPolygonMode fnd_gfx_to_vk_fill_mode(fnd_gfx_fill_mode mode) {
    switch (mode) {
        case fnd_gfx_fill_mode_solid:       return VK_POLYGON_MODE_FILL;
        case fnd_gfx_fill_mode_wireframe:   return VK_POLYGON_MODE_LINE;
        default: assert(0 && "Invalid fnd_gfx_fill_mode!");
    } return VK_POLYGON_MODE_FILL;
}

static inline VkBlendFactor fnd_gfx_to_vk_blend_factor(fnd_gfx_blend_factor factor) {
    switch (factor) {
        case fnd_gfx_blend_factor_zero:                     return VK_BLEND_FACTOR_ZERO;
        case fnd_gfx_blend_factor_one:                      return VK_BLEND_FACTOR_ONE;

        case fnd_gfx_blend_factor_src_color:                return VK_BLEND_FACTOR_SRC_COLOR;
        case fnd_gfx_blend_factor_one_minus_src_color:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;

        case fnd_gfx_blend_factor_dst_color:                return VK_BLEND_FACTOR_DST_COLOR;
        case fnd_gfx_blend_factor_one_minus_dst_color:      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;

        case fnd_gfx_blend_factor_src_alpha:                return VK_BLEND_FACTOR_SRC_ALPHA;
        case fnd_gfx_blend_factor_one_minus_src_alpha:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

        case fnd_gfx_blend_factor_dst_alpha:                return VK_BLEND_FACTOR_DST_ALPHA;
        case fnd_gfx_blend_factor_one_minus_dst_alpha:      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;

        case fnd_gfx_blend_factor_constant_color:           return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case fnd_gfx_blend_factor_one_minus_constant_color: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;

        case fnd_gfx_blend_factor_constant_alpha:           return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case fnd_gfx_blend_factor_one_minus_constant_alpha: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;

        case fnd_gfx_blend_factor_src_alpha_saturate:       return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        default: assert(0 && "Invalid fnd_gfx_blend_factor!");
    } return VK_BLEND_FACTOR_ZERO;
}

static inline VkBlendOp fnd_gfx_to_vk_blend_op(fnd_gfx_blend_op op) {
    switch (op) {
        case fnd_gfx_blend_op_add:              return VK_BLEND_OP_ADD;
        case fnd_gfx_blend_op_subtract:         return VK_BLEND_OP_SUBTRACT;
        case fnd_gfx_blend_op_reverse_subtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case fnd_gfx_blend_op_min:              return VK_BLEND_OP_MIN;
        case fnd_gfx_blend_op_max:              return VK_BLEND_OP_MAX;
        default: assert(0 && "Invalid fnd_gfx_blend_op!");
    } return VK_BLEND_OP_ADD;
}

struct fnd_gfx_pipeline {
    fnd_gfx_hardware*       owning_hardware;
    VkPipeline          pipeline;
    VkPipelineLayout    layout;
    uint32_t            constants_offset[fnd_gfx_shader_stage_count];
};

fnd_gfx_pipeline* fnd_gfx_create_pipeline(fnd_gfx_hardware* hardware, const fnd_gfx_pipeline_create_info* info) {
    // For later, just initializing in case of _fail
    VkFormat*                            formats_aux_array = NULL;
    VkPipelineColorBlendAttachmentState* color_blend_attachments = NULL;

    fnd_gfx_pipeline* pipeline = malloc(sizeof(fnd_gfx_pipeline));
    if (!pipeline) goto _fail; *pipeline = (fnd_gfx_pipeline){.owning_hardware = hardware};

    // Constants ranges

    uint32_t            constants_ranges_count = 0;
    VkPushConstantRange constants_ranges[fnd_gfx_shader_stage_count];
    uint32_t            constants_offset = 0;
    for (uint32_t stage = 0; stage < fnd_gfx_shader_stage_count; stage++) {
        if (!info->shader_stages.constants[stage]) continue;
        constants_ranges[constants_ranges_count++] = (VkPushConstantRange){
            .stageFlags = fnd_gfx_to_vk_shader_stage(stage),
            .offset     = constants_offset,
            .size       = info->shader_stages.constants[stage],
        };
        pipeline->constants_offset[stage] = constants_offset;
        constants_offset += info->shader_stages.constants[stage];
    }

    // Pipeline Layout

    if (vkCreatePipelineLayout(hardware->logical_device, &(VkPipelineLayoutCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = constants_ranges_count,
        .pPushConstantRanges    = constants_ranges,
        .setLayoutCount         = 1,
        .pSetLayouts            = &hardware->bindless_descriptor_layout
    }, NULL, &pipeline->layout) != VK_SUCCESS) goto _fail;

    // Dynamic State

    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = config_all_pipelines_dynamic_state_count,
        .pDynamicStates    = config_all_pipelines_dynamic_state_array
    };

    // Shader Stages

    uint32_t stages_count = 0;
    VkPipelineShaderStageCreateInfo stages[fnd_gfx_shader_stage_count];

    for (uint32_t stage = 0; stage < fnd_gfx_shader_stage_count; stage++) {
        if (!info->shader_stages.shaders[stage]) continue;
        stages[stages_count++] = (VkPipelineShaderStageCreateInfo){
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = fnd_gfx_to_vk_shader_stage(stage),
            .module = info->shader_stages.shaders[stage]->module,
            .pName  = "main"
        };
    }

    // Vertex Input
    // Enforce vertex pulling

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 0,
        .pVertexBindingDescriptions      = NULL,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions    = NULL
    };

    // Input Assembly

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = fnd_gfx_to_vk_primitive_topology(info->input_assembler_state.topology),
        .primitiveRestartEnable = VK_FALSE
    };

    // Rasterizer

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = (info->rasterizer_state.fill_mode == fnd_gfx_fill_mode_wireframe)
            ? VK_POLYGON_MODE_LINE
            : VK_POLYGON_MODE_FILL,
        .cullMode  = fnd_gfx_to_vk_cull_mode(info->rasterizer_state.cull_mode),
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
        .depthClampEnable = info->rasterizer_state.depth_clamp_enable,
        .rasterizerDiscardEnable = VK_FALSE
    };

    // Viewport

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount  = 1,
        .scissorCount   = 1
    };

    // Multisampling

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    // Depth / Stencil

    VkPipelineDepthStencilStateCreateInfo depth = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable  = info->depth_stencil_state.depth_test_enable,
        .depthWriteEnable = info->depth_stencil_state.depth_write_enable,
    };

    // Color Blend

    color_blend_attachments = malloc(info->attachment_state.color_attachments_count * sizeof(VkPipelineColorBlendAttachmentState));
    if (!color_blend_attachments) goto _fail;
    for (uint32_t i = 0; i < info->attachment_state.color_attachments_count; i++) {
        color_blend_attachments[i] = (VkPipelineColorBlendAttachmentState){
            .blendEnable         = info->blend_state.blend_enable ? VK_TRUE : VK_FALSE,
            .srcColorBlendFactor = fnd_gfx_to_vk_blend_factor(info->blend_state.src_factor),
            .dstColorBlendFactor = fnd_gfx_to_vk_blend_factor(info->blend_state.dst_factor),
            .colorBlendOp        = fnd_gfx_to_vk_blend_op(info->blend_state.blend_op),
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT
        };
    }

    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = info->attachment_state.color_attachments_count,
        .pAttachments    = color_blend_attachments
    };

    // Render target info
    
    formats_aux_array = malloc(info->attachment_state.color_attachments_count * sizeof(VkFormat));
    if (!formats_aux_array) goto _fail;
    for (uint32_t i = 0; i < info->attachment_state.color_attachments_count; i++) {
        formats_aux_array[i] = fnd_gfx_to_vk_texture_format(info->attachment_state.color_attachments_formats[i]);
    }

    VkFormat depth_format = info->attachment_state.depth_stencil_format ? 
        fnd_gfx_to_vk_texture_format(*info->attachment_state.depth_stencil_format) : VK_FORMAT_UNDEFINED;

    VkPipelineRenderingCreateInfoKHR render_target_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount    = info->attachment_state.color_attachments_count,
        .pColorAttachmentFormats = formats_aux_array,
        .depthAttachmentFormat   = depth_format,
        .stencilAttachmentFormat = depth_format
    };

    // Graphics Pipeline

    if (vkCreateGraphicsPipelines(hardware->logical_device, VK_NULL_HANDLE, 1, &(VkGraphicsPipelineCreateInfo){
        .sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext                  = &render_target_info,
        .pDynamicState          = &dynamic_state,
        .stageCount             = stages_count,
        .pStages                = stages,
        .pVertexInputState      = &vertex_input,
        .pInputAssemblyState    = &input_assembly,
        .pViewportState         = &viewport_state,
        .pRasterizationState    = &rasterizer,
        .pMultisampleState      = &multisampling,
        .pDepthStencilState     = &depth,
        .pColorBlendState       = &blend,
        .layout                 = pipeline->layout,
        .basePipelineHandle     = VK_NULL_HANDLE,
        .basePipelineIndex      = -1, 
        .subpass                = 0,
    }, NULL, &pipeline->pipeline) != VK_SUCCESS) goto _fail;

    free(formats_aux_array); free(color_blend_attachments);
    return pipeline;

_fail: free(formats_aux_array); free(color_blend_attachments); fnd_gfx_free_pipeline(pipeline); return NULL;
}

void fnd_gfx_free_pipeline(fnd_gfx_pipeline* pipeline) {
    if (!pipeline) return;
    vkDestroyPipeline(pipeline->owning_hardware->logical_device, pipeline->pipeline, NULL);
    vkDestroyPipelineLayout(pipeline->owning_hardware->logical_device, pipeline->layout, NULL);
    free(pipeline);
}

// ===========================
// Transfer Commands

void fnd_gfx_tcmd_copy_staging_to_buffer(
    fnd_gfx_staging*     staging,
    fnd_gfx_buffer*             target_buffer,
    uint64_t                staging_region_offset,
    uint64_t                buffer_write_region_offset,
    uint64_t                buffer_write_region_size
) {
    VkBufferCopy copy_region = {
        .srcOffset  = staging_region_offset,
        .size       = buffer_write_region_size,
        .dstOffset  = buffer_write_region_offset
    };

    vkCmdCopyBuffer(
        recording_state_commands->command_buffer, 
        staging->buffer, 
        target_buffer->buffer, 
        1, &copy_region
    );
}

void fnd_gfx_tcmd_copy_staging_to_texture(
    fnd_gfx_staging*     staging,
    fnd_gfx_texture*            target_texture,
    uint64_t                staging_region_offset,
    fnd_gfx_texture_dimensions  texture_write_region_offset,
    fnd_gfx_texture_dimensions  texture_write_region_size
) {
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = target_texture->image,
        .subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = target_texture->mip_levels,
            .baseArrayLayer = 0,
            .layerCount     = target_texture->layers,
        },
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    };

    vkCmdPipelineBarrier(
        recording_state_commands->command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL,
        1, &barrier
    );

    VkBufferImageCopy region = {
        .bufferOffset       = staging_region_offset,
        .bufferRowLength    = 0,
        .bufferImageHeight  = 0,

        .imageSubresource.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT,
        .imageSubresource.mipLevel          = 0,
        .imageSubresource.baseArrayLayer    = 0,
        .imageSubresource.layerCount        = 1,

        .imageOffset = {
            texture_write_region_offset.width,
            texture_write_region_offset.height,
            texture_write_region_offset.depth
        },

        .imageExtent = {
            texture_write_region_size.width,
            texture_write_region_size.height,
            texture_write_region_size.depth
        }
    };

    vkCmdCopyBufferToImage(
        recording_state_commands->command_buffer,
        staging->buffer,
        target_texture->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region
    );
}

void fnd_gfx_tcmd_copy_buffer_to_buffer(
    fnd_gfx_buffer*             source_buffer,
    fnd_gfx_buffer*             target_buffer,
    uint64_t                source_region_offset,
    uint64_t                target_region_offset,
    uint64_t                target_region_size
) {
    VkBufferCopy copy_region = {
        .srcOffset  = source_region_offset,
        .size       = target_region_size,
        .dstOffset  = target_region_offset
    };

    vkCmdCopyBuffer(
        recording_state_commands->command_buffer, 
        source_buffer->buffer, 
        target_buffer->buffer, 
        1, &copy_region
    );
}

// ===========================
// Graphics Commands

static inline VkAttachmentLoadOp fnd_gfx_to_vk_load_op(fnd_gfx_load_op op) {
    switch (op) {
        case fnd_gfx_load_op_load:       return VK_ATTACHMENT_LOAD_OP_LOAD;
        case fnd_gfx_load_op_clear:      return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case fnd_gfx_load_op_dont_care:  return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        default: assert(0 && "Invalid fnd_gfx_load_op!");
    } return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

static inline VkAttachmentStoreOp fnd_gfx_to_vk_store_op(fnd_gfx_store_op op) {
    switch (op) {
        case fnd_gfx_store_op_store:     return VK_ATTACHMENT_STORE_OP_STORE;
        case fnd_gfx_store_op_dont_care: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        default: assert(0 && "Invalid fnd_gfx_store_op!");
    } return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkRenderingAttachmentInfo fnd_gfx_to_vk_rendering_attachment(fnd_gfx_gcmd_rendering_attachment_info* info, int depth) {
    return (VkRenderingAttachmentInfo){
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .clearValue.color.float32[0] = info->clear_color.r,
        .clearValue.color.float32[1] = info->clear_color.g,
        .clearValue.color.float32[2] = info->clear_color.b,
        .clearValue.color.float32[3] = info->clear_color.a,
        .imageLayout        = depth ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .imageView          = info->texture->view,
        .loadOp             = fnd_gfx_to_vk_load_op(info->load_op),
        .storeOp            = fnd_gfx_to_vk_store_op(info->store_op),
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .resolveImageView   = VK_NULL_HANDLE,
        .resolveMode        = 0
    };
}

void fnd_gfx_gcmd_begin_rendering(fnd_gfx_gcmd_rendering_info* info) {
    VkRenderingAttachmentInfo* color_infos = malloc(info->color_attachments_count * sizeof(VkRenderingAttachmentInfo));
    if (!color_infos) return; for (uint32_t i = 0; i < info->color_attachments_count; i++) {
        color_infos[i] = fnd_gfx_to_vk_rendering_attachment(&info->color_attachments[i], 0);
    }
    
    VkRenderingAttachmentInfo depth_info;
    if (info->depth_stencil_attachment) depth_info = fnd_gfx_to_vk_rendering_attachment(info->depth_stencil_attachment, 1);

    recording_state_commands->owning_hardware->vkCmdBeginRenderingKHR(
        recording_state_commands->command_buffer, &(VkRenderingInfoKHR){
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .layerCount             = 1,
            .colorAttachmentCount   = info->color_attachments_count,
            .pColorAttachments      = color_infos,
            .pDepthAttachment       = info->depth_stencil_attachment ? &depth_info : NULL,
            .pStencilAttachment     = info->depth_stencil_attachment ? &depth_info : NULL,
            .renderArea = (VkRect2D){
                .offset = {info->area_offset_x, info->area_offset_y},
                .extent = {info->area_width, info->area_height}
            }
        }
    );

    free(color_infos);
}

void fnd_gfx_gcmd_finish_rendering() {
    recording_state_commands->owning_hardware->vkCmdEndRenderingKHR(
        recording_state_commands->command_buffer
    );
}

void fnd_gfx_gcmd_bind_graphics_pipeline(fnd_gfx_pipeline* pipeline) {
    vkCmdBindPipeline(
        recording_state_commands->command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->pipeline
    );
    
    vkCmdBindDescriptorSets(
        recording_state_commands->command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->layout,
        0, 1, &pipeline->owning_hardware->bindless_descriptor,
        0, NULL
    );
}

void fnd_gfx_gcmd_write_constants(
    fnd_gfx_pipeline*    pipeline, 
    fnd_gfx_shader_stage stage, 
    uint32_t         offset,
    uint32_t         bytes, 
    void*            data
) {
    vkCmdPushConstants(
        recording_state_commands->command_buffer,
        pipeline->layout,
        fnd_gfx_to_vk_shader_stage(stage),
        pipeline->constants_offset[stage] + offset, bytes, data
    );
}

void fnd_gfx_gcmd_draw(
    uint32_t vertices_base,
    uint32_t vertices_count,
    uint32_t instances_base,
    uint32_t instances_count
) {
    vkCmdDraw(
        recording_state_commands->command_buffer,
        (uint32_t)vertices_count,
        (uint32_t)instances_count,
        (uint32_t)vertices_base,
        (uint32_t)instances_base
    );
}

void fnd_gfx_gcmd_set_scissors(
    int32_t root_x,  int32_t  root_y,
    uint32_t width,  uint32_t height
) {
    vkCmdSetScissor(recording_state_commands->command_buffer, 0, 1, &(VkRect2D){
        .offset = {root_x, root_y},
        .extent = {width, height}
    });
}

void fnd_gfx_gcmd_set_viewport(
    int32_t root_x,  int32_t  root_y,
    uint32_t width,  uint32_t height
) {
    vkCmdSetViewport(recording_state_commands->command_buffer, 0, 1, &(VkViewport){
        .x        = (float)root_x,
        .y        = (float)root_y,
        .width    = (float)width,
        .height   = (float)height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    });
}

// ===========================
// Windowing Platform

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

typedef struct swapchain_support_details {
    VkSurfaceCapabilitiesKHR capabilities;

    uint32_t                 formats_count;
    VkSurfaceFormatKHR*      formats;

    uint32_t                 present_modes_count;
    VkPresentModeKHR*        present_modes;
} swapchain_support_details;

swapchain_support_details get_swapchain_support_details(VkPhysicalDevice device, VkSurfaceKHR surface) {
    swapchain_support_details details = {0};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formats_count, 0);
    if (details.formats_count != 0) {
        details.formats = malloc(details.formats_count * sizeof(VkSurfaceFormatKHR));
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formats_count, details.formats);
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.present_modes_count, 0);
    if (details.present_modes_count != 0) {
        details.present_modes = malloc(details.present_modes_count * sizeof(VkPresentModeKHR));
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.present_modes_count, details.present_modes);
    }

    return details;
}

void free_swapchain_support_details(swapchain_support_details details) {
    free(details.present_modes);
    free(details.formats);
}

int windowing_platform_init(fnd_gfx_library* library) {
    return glfwInit() == GLFW_TRUE;
}

void windowing_platform_term(fnd_gfx_library* library) {
    glfwTerminate();
}

int windowing_platform_get_required_extensions(fnd_gfx_library* library, uint32_t* count, const char*** names) {
    *names = glfwGetRequiredInstanceExtensions(count); return 1;
}

int windowing_platform_query_presentation_support(fnd_gfx_library* library, VkPhysicalDevice device) {
    VkSurfaceKHR surface; void* other_data_storage; 
    if (!windowing_platform_create_test_surface(library, &surface, &other_data_storage)) return 0;
    swapchain_support_details details = get_swapchain_support_details(device, surface);

    int result = 1;
    if (details.formats_count == 0)       result = 0;
    if (details.present_modes_count == 0) result = 0;

    free_swapchain_support_details(details);
    windowing_platform_free_test_surface(library, surface, other_data_storage);

    return result;
}

int windowing_platform_create_test_surface(fnd_gfx_library* library, VkSurfaceKHR* surface, void** other_data_storage) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* test_window = glfwCreateWindow(100, 100, "Vulkan test window", NULL, NULL);
    if (!test_window) return 0;

    VkSurfaceKHR srf; if (glfwCreateWindowSurface(library->instance, test_window, 0, &srf) != VK_SUCCESS) {
        glfwDestroyWindow(test_window); return 0;
    }

    *surface            = srf;
    *other_data_storage = test_window;
    return 1;
}

void windowing_platform_free_test_surface(fnd_gfx_library* library, VkSurfaceKHR surface, void* other_data_return) {
    vkDestroySurfaceKHR(library->instance, surface, 0); 
    glfwDestroyWindow((GLFWwindow*)other_data_return);
}

// ===========================
// Window Physical Device Queries

uint32_t clamp_u32(uint32_t v, uint32_t min, uint32_t max) {
    if (v < min) v = min;
    if (v > max) v = max;
    return v;
}

uint32_t min_u32(uint32_t l, uint32_t r) {
    return l < r ? l : r;
}

uint32_t max_u32(uint32_t l, uint32_t r) {
    return l > r ? l : r;
}

typedef struct window_swapchain_settings {
    VkSurfaceFormatKHR              format;
    VkPresentModeKHR                presentation;
    VkExtent2D                      extend;
    uint32_t                        image_count;
    VkSurfaceTransformFlagBitsKHR   pretransform;
} window_swapchain_settings;

static window_swapchain_settings pick_window_swapchain_settings(
    VkPhysicalDevice    device,
    VkSurfaceKHR        surface,
    int                 window_framebuffer_width, 
    int                 window_framebuffer_height,
    uint32_t            desired_images_count
) {
    swapchain_support_details details = get_swapchain_support_details(device, surface);

    // Pick best surface format
    VkSurfaceFormatKHR format = details.formats[0];
    for (uint32_t i = 0; i < details.formats_count; i++) {
        if (details.formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && details.formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            format = details.formats[i];
        }
    }

    // Pick best presentation mode
    VkPresentModeKHR presentation = VK_PRESENT_MODE_FIFO_KHR; // guaranteed
    for (uint32_t i = 0; i < details.present_modes_count; i++) {
        if (details.present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentation = VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    // Pick swapchain extend
    VkExtent2D extend;
    if (details.capabilities.currentExtent.width != UINT32_MAX) extend = details.capabilities.currentExtent;
    else {
        extend = (VkExtent2D){
            (uint32_t)(window_framebuffer_width),
            (uint32_t)(window_framebuffer_height)
        };

        extend.width  = clamp_u32(extend.width,  details.capabilities.minImageExtent.width,  details.capabilities.maxImageExtent.width);
        extend.height = clamp_u32(extend.height, details.capabilities.minImageExtent.height, details.capabilities.maxImageExtent.height);
    }

    // Swapchain images count
    uint32_t image_count = desired_images_count; 
    image_count = max_u32(image_count, details.capabilities.minImageCount);                                         // minimal limit
    if (details.capabilities.maxImageCount) image_count = min_u32(image_count, details.capabilities.maxImageCount); // maximal limit (if exists)

    free_swapchain_support_details(details);

    return (window_swapchain_settings){
        .format         = format,
        .presentation   = presentation,
        .extend         = extend,
        .image_count    = image_count,
        .pretransform   = details.capabilities.currentTransform
    };
}

// ===========================
// Window

typedef struct swapchain_pack {
    uint32_t                width;
    uint32_t                height;
    VkSwapchainKHR          swapchain;
    uint32_t                attachments_count;
    fnd_gfx_texture*        attachments_array;
    VkCommandBuffer*        transition;         // Transition i'th attachments format for presentation
    fnd_gfx_texture_format  color_format;
    uint32_t                semaphores_count;
    uint32_t                semaphores_iter;
    VkSemaphore*            acquire_semaphores;
    VkSemaphore*            present_semaphores;
} swapchain_pack;

typedef struct retired_swapchain {
    uint64_t            target_timeline;    // Padded target value that must be hit to ensure presentation is truly finished
    swapchain_pack      retired_pack;       // Retired swapchain, enqueued for deletion
} retired_swapchain;

struct fnd_gfx_window {
    fnd_gfx_hardware*   owning_hardware;    // The hardware
    GLFWwindow*         window;             // GLFW window handle
    VkSurfaceKHR        surface;            // Window surface object
    uint64_t            present_timepoint;  // Last present call present timeline signal
    VkSemaphore         present_timeline;   // Allows safe erase of retired swapchains
    VkCommandPool       transitions_pool;   // Allocs swapchain transition buffers (graphics domain)
    uint32_t            attachments;        // Desired color attachments
    swapchain_pack      swapchain;          // Current non-retired swapchain
    uint32_t            retired_capacity;   // Circular buffer capacity
    uint32_t            retired_first;      // Circular buffer first retired swapchain (queue behavior)
    uint32_t            retired_count;      // Circular buffer objects count
    retired_swapchain*  retired;            // Circular buffer memory
    float               scroll_input;       // Window scroll input from callback
};

// Returns non-zero at success, can recreate, retires current swapchain
int  create_swapchain(fnd_gfx_window* window);
void retire_swapchain(fnd_gfx_window* window);
void safe_free_retired_swapchains(fnd_gfx_window* window, int force);

// Window-GLFW input callbacks

void window_scroll_callback(GLFWwindow* platform_window, double xoffset, double yoffset) {
    fnd_gfx_window* window = glfwGetWindowUserPointer(platform_window);
    window->scroll_input = yoffset;
}

// Window Creation

fnd_gfx_window* fnd_gfx_create_window(fnd_gfx_hardware* hardware, const fnd_gfx_window_create_info* info) {
    fnd_gfx_window* window = malloc(sizeof(fnd_gfx_window)); if (!window) goto _fail;
    *window = (fnd_gfx_window){.owning_hardware = hardware, .attachments = info->attachments};

    // Window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);   // Ensure no-opengl context
    glfwWindowHint(GLFW_VISIBLE,    GLFW_TRUE);     // Ensure visible
    window->window = glfwCreateWindow(info->width, info->height, info->title, 0, 0);
    if (!window->window) goto _fail;

    // Set Window Callbacks
    glfwSetScrollCallback    (window->window, window_scroll_callback);
    glfwSetWindowUserPointer (window->window, window);  // set glfw payload to owning dgx window

    // Surface
    if (glfwCreateWindowSurface(
        window->owning_hardware->owning_library->instance, window->window, 0, &window->surface
    ) != VK_SUCCESS) goto _fail;

    // Ensure presentation support on surface
    VkBool32 presentation_supported;
    vkGetPhysicalDeviceSurfaceSupportKHR(
        window->owning_hardware->physical_device, 
        window->owning_hardware->presentation_queue_family,
        window->surface, 
        &presentation_supported
    ); if (!presentation_supported) goto _fail;

    // Presentation semaphore
    window->present_timepoint = 0;
    if (vkCreateSemaphore(hardware->logical_device, &(VkSemaphoreCreateInfo){
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &(VkSemaphoreTypeCreateInfoKHR){
            .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue  = window->present_timepoint,
        }
    }, NULL, &window->present_timeline) != VK_SUCCESS) goto _fail;

    // Command pool
    if (vkCreateCommandPool(hardware->logical_device, &(VkCommandPoolCreateInfo){
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = hardware->work_group_queue_family[fnd_gfx_command_domain_graphics],
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    }, NULL, &window->transitions_pool) != VK_SUCCESS) goto _fail;

    // Swapchain
    if (!create_swapchain(window)) goto _fail;

    return window;
_fail: fnd_gfx_free_window(window); return NULL;
}

void fnd_gfx_free_window(fnd_gfx_window* window) {
    if (!window) return;
    
    // Enqueue current swapchain for erase
    retire_swapchain(window);

    // Wait for the entire device to be idle, guaranteeing presentation engines are done
    fnd_gfx_hardware_wait_idle(window->owning_hardware);
    safe_free_retired_swapchains(window, 1);
    free(window->retired);

    vkDestroySurfaceKHR(window->owning_hardware->owning_library->instance, window->surface, NULL);
    vkDestroySemaphore(window->owning_hardware->logical_device, window->present_timeline, NULL);
    vkDestroyCommandPool(window->owning_hardware->logical_device, window->transitions_pool, NULL);
    glfwDestroyWindow(window->window);
    free(window);
}

// Window Attachments

fnd_gfx_texture* fnd_gfx_window_get_attachment_color(fnd_gfx_window* window, uint32_t target_index) {
    return &window->swapchain.attachments_array[target_index];
}

fnd_gfx_texture_format fnd_gfx_window_get_attachment_format(fnd_gfx_window* window) {
    return window->swapchain.color_format;
}

// Window Present

int fnd_gfx_window_acquire(fnd_gfx_window* window, fnd_gfx_timeline* can_render_timeline, uint64_t can_render_signal, uint32_t* out_index) {
    // Implicitly update input
    window->scroll_input = 0.0f;
    glfwPollEvents();
    
    VkResult result = vkAcquireNextImageKHR(
        window->owning_hardware->logical_device, 
        window->swapchain.swapchain, 
        2 * (uint64_t)1e9, // seconds
        window->swapchain.acquire_semaphores[window->swapchain.semaphores_iter],
        VK_NULL_HANDLE,
        out_index
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        create_swapchain(window);
        return fnd_gfx_window_acquire(window, can_render_timeline, can_render_signal, out_index);
    } 
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return 0; // Failed to acquire swap chain image
    }

    // Once acquire semaphore is signaled, release timeline signal
    if (vkQueueSubmit(window->owning_hardware->presentation_queue, 1, &(VkSubmitInfo){
        .sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount     = 1,
        .pWaitSemaphores        = &window->swapchain.acquire_semaphores[window->swapchain.semaphores_iter],
        .signalSemaphoreCount   = can_render_timeline ? 1 : 0,
        .pSignalSemaphores      = &can_render_timeline->timeline_semaphore,
        .pWaitDstStageMask      = (VkPipelineStageFlags[]){VK_PIPELINE_STAGE_ALL_COMMANDS_BIT},
        .pNext                  = &(VkTimelineSemaphoreSubmitInfoKHR){
            .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
            .signalSemaphoreValueCount  = can_render_timeline ? 1 : 0,
            .pSignalSemaphoreValues     = &can_render_signal
        },
    }, VK_NULL_HANDLE) != VK_SUCCESS) return 0;
    return 1;
}

void fnd_gfx_window_present(
    fnd_gfx_window*     window, 
    uint32_t            target_index, 
    uint32_t            wait_timeline_count,    
    fnd_gfx_timeline**  wait_timelines, 
    uint64_t*           wait_signals, 
    fnd_gfx_timeline*   wait_finished_timeline, 
    uint64_t            wait_finished_signal
) {
    window->present_timepoint++;

    // Build wait semaphore / value arrays from the timeline list
    VkSemaphore*            wait_semaphores = malloc(sizeof(VkSemaphore) * wait_timeline_count);
    VkPipelineStageFlags*   wait_stages     = malloc(sizeof(VkPipelineStageFlags) * wait_timeline_count);
    uint64_t*               wait_values     = malloc(sizeof(uint64_t) * wait_timeline_count);

    for (uint32_t i = 0; i < wait_timeline_count; ++i) {
        wait_semaphores[i] = wait_timelines[i]->timeline_semaphore;
        wait_stages[i]     = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        wait_values[i]     = wait_signals[i];
    }

    // Check wait finished
    int included_wait_finished = wait_finished_timeline ? 1 : 0;

    // Once timeline(s) signaled, signal present semaphore
    VkSubmitInfo submit_info = {
        .sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount     = 1,
        .pCommandBuffers        = &window->swapchain.transition[target_index],
        .waitSemaphoreCount     = wait_timeline_count,
        .pWaitSemaphores        = wait_semaphores,
        .signalSemaphoreCount   = 2 + included_wait_finished,
        .pSignalSemaphores      = (VkSemaphore[]){
            window->swapchain.present_semaphores[target_index],
            window->present_timeline,
            included_wait_finished ? wait_finished_timeline->timeline_semaphore : VK_NULL_HANDLE
        },
        .pWaitDstStageMask      = wait_stages,
        .pNext                  = &(VkTimelineSemaphoreSubmitInfoKHR){
            .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
            .waitSemaphoreValueCount    = wait_timeline_count,
            .pWaitSemaphoreValues       = wait_values,
            .signalSemaphoreValueCount  = 2 + included_wait_finished,
            .pSignalSemaphoreValues     = (uint64_t[]){
                0, window->present_timepoint, wait_finished_signal
            }
        },
    };
    vkQueueSubmit(window->owning_hardware->presentation_queue, 1, &submit_info, VK_NULL_HANDLE);
    free(wait_semaphores); free(wait_stages); free(wait_values);

    // Once present semaphore signaled present
    vkQueuePresentKHR(
        window->owning_hardware->presentation_queue,
        &(VkPresentInfoKHR){
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .swapchainCount     = 1,
            .pSwapchains        = &window->swapchain.swapchain,
            .pImageIndices      = &target_index,
            .pResults           = 0,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &window->swapchain.present_semaphores[target_index]
        }
    );

    window->swapchain.semaphores_iter = (window->swapchain.semaphores_iter + 1) % window->swapchain.semaphores_count;

    // Try to remove old swapchains safely
    safe_free_retired_swapchains(window, 0);
}

// Window Input

int fnd_gfx_window_query_shall_close(fnd_gfx_window* window) {
    return glfwWindowShouldClose(window->window);
}

void fnd_gfx_window_query_is_focused(fnd_gfx_window* window, int* is) {
    if (is) *is = glfwGetWindowAttrib(window->window, GLFW_FOCUSED);
}

void fnd_gfx_window_query_cursor_pos(fnd_gfx_window* window, int* xpos, int* ypos) {
    double x, y; glfwGetCursorPos(window->window, &x, &y);
    if (xpos) *xpos = (int)x;
    if (ypos) *ypos = (int)y;
}

void fnd_gfx_window_query_input(fnd_gfx_window* window, int* left_pressed, int* right_pressed, float* scroll) {
    GLFWwindow* w = (GLFWwindow*)window->window;
    if (left_pressed)   *left_pressed  = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (right_pressed)  *right_pressed = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (scroll)         *scroll        = window->scroll_input;
}

// Window Queries

void fnd_gfx_window_query_size(fnd_gfx_window* window, uint32_t* width, uint32_t* height) {
    if (width)  *width  = window->swapchain.width;
    if (height) *height = window->swapchain.height;
}

// Swapchain

static fnd_gfx_texture_format vk_to_fnd_gfx_texture_format(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R8_UNORM:            return fnd_gfx_texture_format_r8_unorm;
    case VK_FORMAT_R8G8_UNORM:          return fnd_gfx_texture_format_rg8_unorm;
    case VK_FORMAT_R8G8B8A8_UNORM:      return fnd_gfx_texture_format_rgba8_unorm;
    case VK_FORMAT_R8G8B8A8_SRGB:       return fnd_gfx_texture_format_rgba8_srgb;
    case VK_FORMAT_B8G8R8A8_UNORM:      return fnd_gfx_texture_format_bgra8_unorm;
    case VK_FORMAT_B8G8R8A8_SRGB:       return fnd_gfx_texture_format_bgra8_srgb;
    case VK_FORMAT_R16_SFLOAT:          return fnd_gfx_texture_format_r16_float;
    case VK_FORMAT_R16G16_SFLOAT:       return fnd_gfx_texture_format_rg16_float;
    case VK_FORMAT_R16G16B16A16_SFLOAT: return fnd_gfx_texture_format_rgba16_float;
    case VK_FORMAT_R32_SFLOAT:          return fnd_gfx_texture_format_r32_float;
    case VK_FORMAT_R32G32_SFLOAT:       return fnd_gfx_texture_format_rg32_float;
    case VK_FORMAT_R32G32B32A32_SFLOAT: return fnd_gfx_texture_format_rgba32_float;
    case VK_FORMAT_D16_UNORM:           return fnd_gfx_texture_format_depth16_unorm;
    case VK_FORMAT_D24_UNORM_S8_UINT:   return fnd_gfx_texture_format_depth24_unorm_stencil8;
    case VK_FORMAT_D32_SFLOAT:          return fnd_gfx_texture_format_depth32_float;
    } return fnd_gfx_texture_format_undefined;
}

int create_swapchain(fnd_gfx_window* window) {
    VkImage* images = NULL;
    VkImageView* views = NULL;

    int framebuffer_width, framebuffer_height;
    glfwGetFramebufferSize(window->window, &framebuffer_width, &framebuffer_height);

    window_swapchain_settings swapchain_settings = pick_window_swapchain_settings(
        window->owning_hardware->physical_device,
        window->surface,
        framebuffer_width,
        framebuffer_height,
        window->attachments
    );

    swapchain_pack new_pack = {
        .width  = (uint32_t)framebuffer_width,
        .height = (uint32_t)framebuffer_height,
    };

    // Swapchain target queues
    uint32_t queue_families_indices[] = {
        window->owning_hardware->work_group_queue_family[fnd_gfx_command_domain_graphics], 
        window->owning_hardware->presentation_queue_family
    };

    uint32_t      queue_families_indices_count;
    VkSharingMode image_sharing_mode;
    if (queue_families_indices[0] != queue_families_indices[1]) {
        image_sharing_mode           = VK_SHARING_MODE_CONCURRENT;
        queue_families_indices_count = 2;
    }
    else {
        image_sharing_mode           = VK_SHARING_MODE_EXCLUSIVE;
        queue_families_indices_count = 0;
    }
    
    VkResult swapchain_create_result = vkCreateSwapchainKHR(window->owning_hardware->logical_device, &(VkSwapchainCreateInfoKHR){
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface                = window->surface,
        .minImageCount          = swapchain_settings.image_count,
        .imageFormat            = swapchain_settings.format.format,
        .imageColorSpace        = swapchain_settings.format.colorSpace,
        .imageExtent            = swapchain_settings.extend,
        .imageArrayLayers       = 1,
        .imageUsage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .presentMode            = swapchain_settings.presentation,
        .clipped                = VK_TRUE,
        .compositeAlpha         = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .oldSwapchain           = window->swapchain.swapchain,
        .preTransform           = swapchain_settings.pretransform,
        .imageSharingMode       = image_sharing_mode,
        .queueFamilyIndexCount  = queue_families_indices_count,
        .pQueueFamilyIndices    = queue_families_indices
    }, NULL, &new_pack.swapchain);

    // Retire current swapchain
    retire_swapchain(window);
    window->swapchain.swapchain = VK_NULL_HANDLE;

    if (swapchain_create_result != VK_SUCCESS) goto _fail;

    // Swapchain images count
    vkGetSwapchainImagesKHR(window->owning_hardware->logical_device, new_pack.swapchain, &new_pack.attachments_count, NULL);

    // Swapchain images
    images = malloc(new_pack.attachments_count * sizeof(VkImage)); if (!images) goto _fail;
    vkGetSwapchainImagesKHR(window->owning_hardware->logical_device, new_pack.swapchain, &new_pack.attachments_count, images);
   
    // Swapchain views
    VkImageViewCreateInfo image_view_create_info = {
        .sType                              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType                           = VK_IMAGE_VIEW_TYPE_2D,
        .format                             = swapchain_settings.format.format,
        .components.r                       = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.g                       = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.b                       = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.a                       = VK_COMPONENT_SWIZZLE_IDENTITY,
        .subresourceRange.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel      = 0,
        .subresourceRange.levelCount        = 1,
        .subresourceRange.baseArrayLayer    = 0,
        .subresourceRange.layerCount        = 1
    };

    views = malloc(new_pack.attachments_count * sizeof(VkImageView)); if (!views) goto _fail;
    for (uint32_t view = 0; view < new_pack.attachments_count; view++) {
        image_view_create_info.image = images[view];
        if (vkCreateImageView(
            window->owning_hardware->logical_device, &image_view_create_info, 0, &views[view]) != VK_SUCCESS
        ) goto _fail;
    }

    // Swapchain attachments
    new_pack.attachments_array = calloc(new_pack.attachments_count, sizeof(fnd_gfx_texture)); if (!new_pack.attachments_array) goto _fail;
    for (uint32_t attachment = 0; attachment < new_pack.attachments_count; attachment++) {
        new_pack.attachments_array[attachment] = (fnd_gfx_texture){
            .owning_hardware = window->owning_hardware,
            .dimensions      = (fnd_gfx_texture_dimensions){
                .width  = new_pack.width,
                .height = new_pack.height,
                .depth  = 1
            },
            .image      = images[attachment],
            .view       = views[attachment],
            .allocation = {} // must not be freed
        };
    }

    // Attachment format
    new_pack.color_format = vk_to_fnd_gfx_texture_format(swapchain_settings.format.format);

    // Record format transitions
    new_pack.transition = calloc(new_pack.attachments_count, sizeof(VkCommandBuffer)); if (!new_pack.transition) goto _fail;
    if (vkAllocateCommandBuffers(window->owning_hardware->logical_device, &(VkCommandBufferAllocateInfo){
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = window->transitions_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = new_pack.attachments_count
    }, new_pack.transition) != VK_SUCCESS) goto _fail;
    for (uint32_t attachment = 0; attachment < new_pack.attachments_count; attachment++) {
        VkCommandBuffer command_buffer = new_pack.transition[attachment];
        
        if (vkBeginCommandBuffer(command_buffer, &(VkCommandBufferBeginInfo){
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags              = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
            .pInheritanceInfo   = 0
        }) != VK_SUCCESS) goto _fail;

        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = new_pack.attachments_array[attachment].image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        vkCmdPipelineBarrier(
            command_buffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, NULL, 0, NULL,
            1, &barrier
        );

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) goto _fail;
    }

    // Generate generational semaphores directly for this swapchain
    new_pack.semaphores_count = new_pack.attachments_count;
    new_pack.semaphores_iter  = 0;
    new_pack.acquire_semaphores = malloc(new_pack.semaphores_count * sizeof(VkSemaphore));
    new_pack.present_semaphores = malloc(new_pack.semaphores_count * sizeof(VkSemaphore));
    
    for (uint32_t i = 0; i < new_pack.semaphores_count; i++) {
        vkCreateSemaphore(window->owning_hardware->logical_device, &(VkSemaphoreCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        }, NULL, &new_pack.acquire_semaphores[i]);
        vkCreateSemaphore(window->owning_hardware->logical_device, &(VkSemaphoreCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        }, NULL, &new_pack.present_semaphores[i]);
    }

    // Save swapchain
    window->swapchain = new_pack;

    free(images); free(views);
    return 1;

_fail: 
    if (new_pack.transition) {
        vkFreeCommandBuffers(window->owning_hardware->logical_device, window->transitions_pool, new_pack.attachments_count, new_pack.transition);
    } free(new_pack.transition);
    
    if (new_pack.attachments_array) {
        for (uint32_t view = 0; view < new_pack.attachments_count; view++) {
            if (new_pack.attachments_array[view].view) {
                vkDestroyImageView(window->owning_hardware->logical_device, new_pack.attachments_array[view].view, NULL);
            }
        }
    } free(new_pack.attachments_array);
    
    if (new_pack.acquire_semaphores) {
        for (uint32_t i = 0; i < new_pack.semaphores_count; i++) {
            if (new_pack.acquire_semaphores[i]) vkDestroySemaphore(window->owning_hardware->logical_device, new_pack.acquire_semaphores[i], NULL);
            if (new_pack.present_semaphores[i]) vkDestroySemaphore(window->owning_hardware->logical_device, new_pack.present_semaphores[i], NULL);
        }
    }
    free(new_pack.acquire_semaphores);
    free(new_pack.present_semaphores);

    if (images) free(images); 
    if (views) free(views); 
    if (new_pack.swapchain) vkDestroySwapchainKHR(window->owning_hardware->logical_device, new_pack.swapchain, NULL);
    
    return 0;
}

void retire_swapchain(fnd_gfx_window* window) {
_begin: 
    if (window->swapchain.swapchain == VK_NULL_HANDLE) return; // nothing to retire
    
    if (window->retired_count == window->retired_capacity) {
        uint32_t           new_capacity = window->retired_capacity ? window->retired_capacity * 2 : 4;
        retired_swapchain* new_retired  = malloc(new_capacity * sizeof(retired_swapchain));

        // Busy-wait free spot
        if (!new_retired) {
            fnd_gfx_hardware_wait_idle(window->owning_hardware);
            safe_free_retired_swapchains(window, 1);
            goto _begin;
        }

        // Move to new block
        for (uint32_t itr = 0; itr < window->retired_count; itr++) {
            uint32_t org = (window->retired_first + itr) % window->retired_capacity;
            new_retired[itr] = window->retired[org];
        }
        
        free(window->retired);
        window->retired          = new_retired;
        window->retired_capacity = new_capacity;
        window->retired_first    = 0;
    }

    uint32_t free_spot = (window->retired_first + window->retired_count) % window->retired_capacity;
    window->retired[free_spot] = (retired_swapchain){
        .target_timeline = window->present_timepoint + window->swapchain.attachments_count + 1,
        .retired_pack    = window->swapchain
    };
    window->retired_count++;
}

void free_retired_swapchain(fnd_gfx_window* window, swapchain_pack pack) {
    for (uint32_t attachment = 0; attachment < pack.attachments_count; attachment++) {
        vkDestroyImageView(window->owning_hardware->logical_device, pack.attachments_array[attachment].view, NULL);
        vkFreeCommandBuffers(window->owning_hardware->logical_device, window->transitions_pool, 1, &pack.transition[attachment]);
    } free(pack.attachments_array); free(pack.transition);
    
    for (uint32_t i = 0; i < pack.semaphores_count; i++) {
        vkDestroySemaphore(window->owning_hardware->logical_device, pack.acquire_semaphores[i], NULL);
        vkDestroySemaphore(window->owning_hardware->logical_device, pack.present_semaphores[i], NULL);
    } free(pack.acquire_semaphores); free(pack.present_semaphores);

    vkDestroySwapchainKHR(window->owning_hardware->logical_device, pack.swapchain, NULL);
}

void safe_free_retired_swapchains(fnd_gfx_window* window, int force) {
    uint32_t itr = window->retired_first;
    uint64_t val; if (window->owning_hardware->vkGetSemaphoreCounterValueKHR(
        window->owning_hardware->logical_device, window->present_timeline, &val
    ) != VK_SUCCESS) val = 0;

    while (window->retired_count) {
        if (!force && window->retired[itr].target_timeline > val) break; // This and following swapchain still are to be presented
        free_retired_swapchain(window, window->retired[itr].retired_pack);
        itr = (itr + 1) % window->retired_capacity;
        window->retired_first = itr;
        window->retired_count--;
    }
}

#else
    #error "No fundatio graphics.h backend set!"
#endif
#endif
