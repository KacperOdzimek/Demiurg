/*
----------------------------------------------------------------
Contents:
This file provides a RHI - render hardware interface.
This interface allows user to write api independent low-level graphics systems.

----------------------------------------------------------------
Code info:
- dgx prefix
- DEMIGURG_GRAPHICS_IMPL macro to build
- user must select graphics api at build using one of the macros below:
    - DEMIGURG_GRAPHICS_VULKAN
- add DEMIGURG_GRAPHICS_VALIDATE macro-flag pre implementation to enable validation and error logging

----------------------------------------------------------------
Depedencies:
- Each of native APIs have they own compilation requirements:
- DEMIGURG_GRAPHICS_VULKAN: - (todo)

----------------------------------------------------------------
Usage: See dedicated documentation

----------------------------------------------------------------
Possible Optimizations and TODOs:
- (Vulkan) paged memory allocation
- (Vulkan) non blocking swapchain recreation, render layout preservation
*/

#ifndef DEMIGURG_GRAPHICS_H
#define DEMIGURG_GRAPHICS_H

// ==========================
// Compile Flags

// #define DEMIGURG_GRAPHICS_VALIDATE - enables validation layers

// ==========================
// Depedencies

#include <stdint.h>

// ==========================
// Enumerations

typedef enum dgx_hardware_type {
    dgx_hardware_type_dont_mind = 0,
    dgx_hardware_type_discrete,
    dgx_hardware_type_integrated,
} dgx_hardware_type;

typedef enum dgx_hardware_queue_type {
    dgx_hardware_queue_type_graphics,
    dgx_hardware_queue_type_transfer,
    dgx_hardware_queue_type_compute,
    dgx_hardware_queue_type_transfer_compute,
} dgx_hardware_queue_type;

typedef enum dgx_hardware_limit {
    // Textures

    dgx_hardware_limit_max_texture_dimension_1d,
    dgx_hardware_limit_max_texture_dimension_2d,
    dgx_hardware_limit_max_texture_dimension_3d,
    dgx_hardware_limit_max_texture_dimension_cube,
    dgx_hardware_limit_max_texture_array_layers,

    // Descriptors

    dgx_hardware_limit_max_descriptor_per_pipeline,

    // Per-stage descriptor limits
    dgx_hardware_limit_max_descriptor_uniform_buffers_per_stage,
    dgx_hardware_limit_max_descriptor_storage_buffers_per_stage,
    dgx_hardware_limit_max_descriptor_sampled_images_per_stage,
    dgx_hardware_limit_max_descriptor_samplers_per_stage,

    // Descriptor set layout limits
    dgx_hardware_limit_max_descriptor_uniform_buffers,
    dgx_hardware_limit_max_descriptor_storage_buffers,
    dgx_hardware_limit_max_descriptor_sampled_images,
    dgx_hardware_limit_max_descriptor_samplers,

    // Bound buffer size limits
    dgx_hardware_limit_max_descriptor_bound_uniform_buffer_length,
    dgx_hardware_limit_max_descriptor_bound_storage_buffer_length,

    // Vertex Input

    dgx_hardware_limit_max_vertex_input_attributes,
    dgx_hardware_limit_max_vertex_input_bindings,
    dgx_hardware_limit_max_vertex_input_attribute_offset,
} dgx_hardware_limit;

typedef enum dgx_data_type {
    dgx_data_type_int32,
    dgx_data_type_vec2i32,
    dgx_data_type_vec3i32,
    dgx_data_type_vec4i32,

    dgx_data_type_float32,
    dgx_data_type_vec2f32,
    dgx_data_type_vec3f32,
    dgx_data_type_vec4f32,
} dgx_data_type;

typedef enum dgx_vertex_attribute_input_rate {
    dgx_vertex_attribute_input_rate_per_vertex,
    dgx_vertex_attribute_input_rate_per_instance
} dgx_vertex_attribute_input_rate;

typedef enum dgx_shader_stages_bitmask {
    dgx_shader_stage_vertex     = 1,
    dgx_shader_stage_pixel      = 2,
    dgx_shader_stage_geometry   = 4,
    dgx_shader_stage_compute    = 8
} dgx_shader_stages_bitmask;

typedef enum dgx_memory_allocation_strategy {
    dgx_memory_allocation_strategy_paged,
    dgx_memory_allocation_strategy_dedicated
} dgx_memory_allocation_strategy;

typedef enum dgx_memory_access {
    dgx_memory_access_gpu_only,
    dgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_read,
    dgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_write,
    dgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_read_and_write
} dgx_memory_access;

typedef enum dgx_buffer_usage {
    dgx_buffer_usage_vertex,
    dgx_buffer_usage_index,
    dgx_buffer_usage_uniform,
    dgx_buffer_usage_storage
} dgx_buffer_usage;

typedef enum dgx_texture_usage {
    dgx_texture_usage_sampled,
    dgx_texture_usage_color_attachment,
    dgx_texture_usage_depth_stencil_attachment,
    dgx_texture_usage_storage,
} dgx_texture_usage;

typedef enum dgx_sampler_filter {
    dgx_sampler_filter_nearest,
    dgx_sampler_filter_linear,
} dgx_sampler_filter;

typedef enum dgx_sampler_wrapping {
    dgx_sampler_wrapping_repeat,
    dgx_sampler_wrapping_repeat_mirrored,
    dgx_sampler_wrapping_repeat_clamp_coordinates,
    dgx_sampler_wrapping_repeat_clamp_texture
} dgx_sampler_wrapping;

typedef enum dgx_texture_type {
    dgx_texture_type_1d,
    dgx_texture_type_2d,
    dgx_texture_type_3d,
    dgx_texture_type_cubemap,
} dgx_texture_type;

typedef enum dgx_texture_format {
    dgx_texture_format_undefined = 0,

    // 8-bit
    dgx_texture_format_r8_unorm,
    dgx_texture_format_rg8_unorm,
    dgx_texture_format_rgba8_unorm,
    dgx_texture_format_rgba8_srgb,
    dgx_texture_format_bgra8_unorm,
    dgx_texture_format_bgra8_srgb,

    // 16-bit float
    dgx_texture_format_r16_float,
    dgx_texture_format_rg16_float,
    dgx_texture_format_rgba16_float,

    // 32-bit float
    dgx_texture_format_r32_float,
    dgx_texture_format_rg32_float,
    dgx_texture_format_rgba32_float,

    // Depth / stencil
    dgx_texture_format_depth16_unorm,
    dgx_texture_format_depth24_unorm_stencil8,
    dgx_texture_format_depth32_float,
} dgx_texture_format;

typedef enum dgx_descriptor_binding_type {
    dgx_descriptor_binding_type_uniform_buffer,
    dgx_descriptor_binding_type_storage_buffer,
    dgx_descriptor_binding_type_sampled_texture,
    dgx_descriptor_binding_type_sampler,
} dgx_descriptor_binding_type;

typedef enum dgx_load_op {
    dgx_load_op_load,       // keep previous contents
    dgx_load_op_clear,      // clear at start
    dgx_load_op_dont_care   // undefined (fast)
} dgx_load_op;

typedef enum dgx_store_op {
    dgx_store_op_store,     // keep result
    dgx_store_op_dont_care  // discard after rendering
} dgx_store_op;

typedef enum dgx_primitive_topology {
    dgx_primitive_topology_point_list,
    dgx_primitive_topology_line_list,
    dgx_primitive_topology_line_strip,
    dgx_primitive_topology_triangle_list,
    dgx_primitive_topology_triangle_strip
} dgx_primitive_topology;

typedef enum dgx_cull_mode {
    dgx_cull_mode_none,
    dgx_cull_mode_front,
    dgx_cull_mode_back,
    dgx_cull_mode_front_and_back
} dgx_cull_mode;

typedef enum dgx_fill_mode {
    dgx_fill_mode_solid,
    dgx_fill_mode_wireframe
} dgx_fill_mode;

typedef enum dgx_blend_factor {
    dgx_blend_factor_zero = 0,
    dgx_blend_factor_one,

    dgx_blend_factor_src_color,
    dgx_blend_factor_one_minus_src_color,

    dgx_blend_factor_dst_color,
    dgx_blend_factor_one_minus_dst_color,

    dgx_blend_factor_src_alpha,
    dgx_blend_factor_one_minus_src_alpha,

    dgx_blend_factor_dst_alpha,
    dgx_blend_factor_one_minus_dst_alpha,

    dgx_blend_factor_constant_color,
    dgx_blend_factor_one_minus_constant_color,

    dgx_blend_factor_constant_alpha,
    dgx_blend_factor_one_minus_constant_alpha,

    dgx_blend_factor_src_alpha_saturate
} dgx_blend_factor;

typedef enum dgx_blend_op {
    dgx_blend_op_add = 0,
    dgx_blend_op_subtract,
    dgx_blend_op_reverse_subtract,
    dgx_blend_op_min,
    dgx_blend_op_max
} dgx_blend_op;

// ===========================
// Utility Structs

typedef struct dgx_color {
    float r, g, b, a;
} dgx_color;

typedef struct dgx_uv_2d {
    float min_x, min_y;
    float max_x, max_y;
} dgx_uv_2d;

// ===========================
// Library

typedef struct dgx_library_create_info {
    int platform_code_enabled;
} dgx_library_create_info;

typedef struct dgx_library dgx_library;

dgx_library* dgx_create_library(const dgx_library_create_info*);
void dgx_free_library(dgx_library*);

// ===========================
// Hardware

typedef struct dgx_hardware_create_info {
    // Requirements

    int require_presentation_queue;
    int require_graphics_queues;

    // Desires

    dgx_hardware_type   desired_hardware_type;
    uint32_t            desired_graphics_queues;
    uint32_t            desired_transfer_compute_queues;
    uint32_t            desired_transfer_queues;
    uint32_t            desired_compute_queues;
} dgx_hardware_create_info;

typedef struct dgx_hardware dgx_hardware;
typedef struct dgx_hardware_queue dgx_hardware_queue;

dgx_hardware* dgx_create_hardware(
    dgx_library*                    library, 
    const dgx_hardware_create_info* info
);
void dgx_free_hardware(dgx_hardware*);

uint32_t dgx_hardware_query_queues_count(dgx_hardware*, dgx_hardware_queue_type type);
void     dgx_hardware_query_queues(dgx_hardware*, dgx_hardware_queue_type type, uint32_t queues_offset, uint32_t queues_count, dgx_hardware_queue** queues);
uint64_t dgx_hardware_query_limit(dgx_hardware*, dgx_hardware_limit limit);

// Wait till all hardware queues have no work at all
// For synchronization
void dgx_hardware_wait_idle(dgx_hardware*);

// ===========================
// Cpu Signal

typedef struct dgx_cpu_signal_create_info {
    int initialy_signaled;
} dgx_cpu_signal_create_info;

// cpu object for syncing with gpu queues
// (fence)
typedef struct dgx_cpu_signal dgx_cpu_signal;

dgx_cpu_signal* dgx_create_cpu_signal(dgx_hardware*, const dgx_cpu_signal_create_info* info);
void dgx_free_cpu_signal(dgx_cpu_signal*);

int  dgx_cpu_signal_signaled(dgx_cpu_signal*);
void dgx_cpu_signal_wait    (dgx_cpu_signal*);
void dgx_cpu_signal_reset   (dgx_cpu_signal*);

// ===========================
// Gpu Signal

// gpu object for syncing between gpu queues
// (semaphore)
typedef struct dgx_gpu_signal dgx_gpu_signal;

dgx_gpu_signal* dgx_create_gpu_signal(dgx_hardware*);
void dgx_free_gpu_signal(dgx_gpu_signal*);

// ===========================
// Window

struct dgx_window;
typedef void(*dgx_window_render_targets_recreated_callback)(struct dgx_window* window, int render_target_layout_changed);

typedef struct dgx_window_create_info {
    const char* title;
    uint32_t    width;
    uint32_t    height;

    // desired amount of frames you are willing to work 
    // at in the same time, "frames in flight"
    // actual frames count may be lower than desired, due to
    // driver limitiations - query with dgx_window_get_render_targets_count
    uint32_t desired_render_targets;

    // called when, due to recreation of swapchain
    // render target layout and render targets gets recreated
    // render targets count may change also
    // may be NULL
    dgx_window_render_targets_recreated_callback render_target_recreated_callback;
} dgx_window_create_info;

typedef struct dgx_window dgx_window;

dgx_window* dgx_create_window(dgx_hardware*, const dgx_window_create_info*);
void dgx_free_window(dgx_window*);

void    dgx_window_update_input(dgx_window*);
int     dgx_window_query_shall_close(dgx_window*);
void    dgx_window_query_is_focused(dgx_window*, int* is);
void    dgx_window_query_cursor_pos(dgx_window*, uint32_t* xpos, uint32_t* ypos);
void    dgx_window_query_input(dgx_window*, int* left_pressed, int* right_pressed, float* scroll);

void    dgx_window_get_size(dgx_window*, uint32_t* width, uint32_t* height);

uint32_t dgx_window_get_render_targets_count(dgx_window*);
uint32_t dgx_window_acquire_next_render_target_index(dgx_window* window, dgx_gpu_signal* can_render_signal);
void     dgx_window_enqueue_render_target_present(dgx_window* window, uint32_t index, uint32_t wait_signals_count, dgx_gpu_signal** wait_signals);

struct dgx_render_target*        dgx_window_get_render_target(dgx_window*, uint32_t target_index);
struct dgx_render_target_layout* dgx_window_get_render_target_layout(dgx_window*);

// ===========================
// Command Lists

typedef struct dgx_command_lists_allocator_create_info {
    dgx_hardware_queue_type target_queue_type;
    int                     often_recorded;
} dgx_command_lists_allocator_create_info;

typedef struct dgx_command_lists_allocator dgx_command_lists_allocator;
typedef struct dgx_command_list dgx_command_list;

dgx_command_lists_allocator* dgx_create_command_lists_allocator(dgx_hardware*, const dgx_command_lists_allocator_create_info*);
void dgx_free_command_lists_allocator(dgx_command_lists_allocator*);

dgx_command_list* dgx_command_lists_allocator_alloc_command_list(dgx_command_lists_allocator*);
void dgx_command_lists_allocator_free_command_list(dgx_command_list*);

void dgx_begin_command_list_recording(dgx_command_list*);
void dgx_finish_command_list_recording(dgx_command_list*);

typedef struct dgx_submit_info {
    uint32_t            command_lists_count;
    dgx_command_list**  command_lists;

    uint32_t            wait_gpu_signals_count;
    dgx_gpu_signal**    wait_gpu_signals;

    uint32_t            signal_gpu_signals_count;
    dgx_gpu_signal**    signal_gpu_signals;

    dgx_cpu_signal*     cpu_signal;
} dgx_submit_info;

void dgx_submit_command_list(
    dgx_hardware_queue*     queue,
    const dgx_submit_info*  info
);

// ==========================
// Staging Memory

typedef struct dgx_staging_memory_create_info {
    uint64_t    size_bytes;
} dgx_staging_memory_create_info;

typedef struct dgx_staging_memory dgx_staging_memory;

dgx_staging_memory* dgx_create_staging_memory(dgx_hardware*, const dgx_staging_memory_create_info*);
void dgx_free_staging_memory(dgx_staging_memory*);

void* dgx_staging_memory_map(dgx_staging_memory*, uint64_t region_offset, uint64_t region_size);
void dgx_staging_memory_unmap(dgx_staging_memory*);

// ==========================
// Buffer

typedef struct dgx_buffer_create_info {
    uint64_t                        size_bytes;
    dgx_buffer_usage                usage;
    dgx_memory_allocation_strategy  memory_strategy;
    dgx_memory_access               memory_access;
} dgx_buffer_create_info;

typedef struct dgx_buffer dgx_buffer;

dgx_buffer* dgx_create_buffer(dgx_hardware*, const dgx_buffer_create_info* info);
void dgx_free_buffer(dgx_buffer*);

uint64_t dgx_buffer_get_size_bytes(dgx_buffer*);

// Very inefficient way of writing to buffer
// User should target async uploads with staging memory
// This function creates all objects required to perform write:
// staging memory, command list allocator, ... 
// It is really heavy. Non zero at success
int dgx_buffer_sync_upload(dgx_buffer*, uint64_t buffer_offset, const void* data, uint64_t data_size_bytes);

typedef struct dgx_buffer_multi_upload_region {
    void*       source_data;
    uint64_t    source_bytes;
    uint64_t    buffer_offset;
    dgx_buffer* buffer;
} dgx_buffer_multi_upload_region;

// Usefull utility function, which  uploads N regions from CPU memory to N GPU buffers 
// using a shared staging window. The window may be smaller than the
// combined data; the function will loop, flushing intermediate batches synchronously via a CPU signal, 
// and only emitting the GPU signal on the final submit.
// CPU signal may be NULL, in case of multiple batches function will create own
// If multiple batches were sent, returns a combined data size
// Else returns 0
uint64_t dgx_buffer_multi_upload(
    dgx_buffer_multi_upload_region* regions,
    uint32_t                        region_count,
    dgx_command_list*               command_list,
    dgx_hardware_queue*             queue_for_uploads,
    dgx_staging_memory*             staging_memory,
    uint64_t                        staging_memory_region_offset,
    uint64_t                        staging_memory_region_size,
    dgx_cpu_signal*                 upload_finished_cpu,
    dgx_gpu_signal*                 upload_finished_gpu
);

// ==========================
// Sampler

typedef struct dgx_sampler_create_info {
    dgx_sampler_filter      mag_filter;
    dgx_sampler_filter      min_filter;
    dgx_sampler_filter      mipmap_filter;

    dgx_sampler_wrapping    x_coord_wrapping;
    dgx_sampler_wrapping    y_coord_wrapping;
    dgx_sampler_wrapping    z_coord_wrapping;
    int                     unnormalized_coordinates;

    float                   min_lod, max_lod;
    float                   mip_lod_bias;
} dgx_sampler_create_info;

typedef struct dgx_sampler dgx_sampler;

dgx_sampler* dgx_create_sampler(dgx_hardware*, const dgx_sampler_create_info* info);
void dgx_free_sampler(dgx_sampler*);

// ==========================
// Texture

typedef struct dgx_texture_dimensions {
    uint32_t x;
    uint32_t y;
    uint32_t z;
} dgx_texture_dimensions;

typedef struct dgx_texture_create_info {
    dgx_texture_type                type;
    dgx_texture_usage               usage;
    dgx_texture_format              format;
    dgx_texture_dimensions          dimensions;
    uint32_t                        array_length;
    //uint32_t                      sample_count;
    uint32_t                        mipmap_layers;
    dgx_memory_allocation_strategy  memory_strategy;
    dgx_memory_access               memory_access;
} dgx_texture_create_info;

typedef struct dgx_texture dgx_texture;

dgx_texture* dgx_create_texture(dgx_hardware*, const dgx_texture_create_info* info);
void dgx_free_texture(dgx_texture* texture);

dgx_texture_dimensions dgx_texture_get_dimensions(dgx_texture*);

// Very inefficient way of writing to texture
// User should target async uploads with staging memory
// This function creates all objects required to perform write:
// staging memory, command list allocator, ... 
// It is really heavy. Non zero at success
int dgx_texture_sync_upload(dgx_texture*, dgx_texture_dimensions texture_offset, const void* data, dgx_texture_dimensions write_dimensions);

// ===========================
// Render Target Layout

typedef struct dgx_render_target_layout_attachment {
    dgx_texture_format  format;
    uint32_t            sample_count;
    dgx_load_op         load_op;
    dgx_store_op        store_op;
} dgx_render_target_layout_attachment;

typedef struct dgx_render_target_layout_create_info {
    uint32_t                                color_attachments_count;
    dgx_render_target_layout_attachment*    color_attachments;
    dgx_render_target_layout_attachment*    depth_stencil_attachment; // nullable
} dgx_render_target_layout_create_info;

typedef struct dgx_render_target_layout dgx_render_target_layout;

dgx_render_target_layout* dgx_create_render_target_layout(dgx_hardware*, const dgx_render_target_layout_create_info* info);
void dgx_free_render_target_layout(dgx_render_target_layout*);

// ===========================
// Render Target

typedef struct dgx_render_target_attachment {
    dgx_texture*                    texture;
} dgx_render_target_attachment;

typedef struct dgx_render_target_create_info {
    dgx_render_target_layout*       render_target_layout;
    uint32_t                        color_attachments_count;
    dgx_render_target_attachment*   color_attachments;
    dgx_render_target_attachment*   depth_stencil_attachment; // nullable
} dgx_render_target_create_info;

typedef struct dgx_render_target dgx_render_target;

dgx_render_target* dgx_create_render_target(dgx_hardware*, const dgx_render_target_create_info* info);
void dgx_free_render_target(dgx_render_target*);

// ===========================
// Descriptor Layout

typedef struct dgx_descriptor_binding {
    uint32_t                    binding;
    dgx_descriptor_binding_type type;
    uint32_t                    count;
    dgx_shader_stages_bitmask   stages;
} dgx_descriptor_binding;

typedef struct dgx_descriptor_layout_create_info {
    uint32_t                    bindings_count;
    dgx_descriptor_binding*     bindings;
} dgx_descriptor_layout_create_info;

typedef struct dgx_descriptor_layout dgx_descriptor_layout;

dgx_descriptor_layout* dgx_create_descriptor_layout(dgx_hardware*, const dgx_descriptor_layout_create_info*);
void dgx_free_descriptor_layout(dgx_descriptor_layout*);

// ===========================
// Descriptors Allocator

typedef struct dgx_descriptor_allocator_create_info {
    dgx_descriptor_layout*  descriptor_layout;
    uint32_t                max_descriptors_allocated;
} dgx_descriptor_allocator_create_info;

typedef struct dgx_descriptor_allocator dgx_descriptor_allocator;
typedef struct dgx_descriptor dgx_descriptor;

dgx_descriptor_allocator* dgx_create_descriptor_allocator(dgx_hardware*, const dgx_descriptor_allocator_create_info*);
void dgx_free_descriptor_allocator(dgx_descriptor_allocator*);

dgx_descriptor* dgx_descriptor_allocator_alloc_descriptor(dgx_descriptor_allocator*);
void dgx_descriptor_allocator_free_descriptor(dgx_descriptor*);

typedef struct dgx_descriptor_buffer_write_info {
    dgx_buffer*     buffer;
    uint32_t        offset;
    uint32_t        length;
} dgx_descriptor_buffer_write_info;

typedef struct dgx_descriptor_sampler_write_info {
    dgx_sampler*    sampler;
} dgx_descriptor_sampler_write_info;

typedef struct dgx_descriptor_sampled_texture_write_info {
    dgx_texture*    sampled_texture;
} dgx_descriptor_sampled_texture_write_info;

typedef struct dgx_descriptor_write_info {
    dgx_descriptor*             descriptor;
    dgx_descriptor_binding_type binding_type;
    uint32_t                    binding_index;
    uint32_t                    array_element_index;
    uint32_t                    array_elements_count;
    union {
        dgx_descriptor_buffer_write_info*           for_buffers;
        dgx_descriptor_sampler_write_info*          for_samplers;
        dgx_descriptor_sampled_texture_write_info*  for_sampled_textures;
    } infos;
} dgx_descriptor_write_info;

void dgx_descriptors_write(dgx_hardware*, uint32_t writes_count, dgx_descriptor_write_info* write_infos);

// ===========================
// Pipeline Descriptors Layout

typedef struct dgx_pipeline_descriptors_layout_create_info {
    uint32_t                layouts_count;
    dgx_descriptor_layout** layouts;
} dgx_pipeline_descriptors_layout_create_info;

typedef struct dgx_pipeline_descriptors_layout dgx_pipeline_descriptors_layout;

dgx_pipeline_descriptors_layout* dgx_create_pipeline_descriptors_layout(dgx_hardware*, const dgx_pipeline_descriptors_layout_create_info* info);
void dgx_free_pipeline_descriptors_layout(dgx_pipeline_descriptors_layout*);

// ===========================
// Shader

typedef struct dgx_shader_create_info {
    const char* source_code;
    uint32_t    source_size;
} dgx_shader_create_info;

typedef struct dgx_shader dgx_shader;

dgx_shader* dgx_create_shader(dgx_hardware*, const dgx_shader_create_info* info);
void dgx_free_shader(dgx_shader* shader);

// ===========================
// Graphics Pipeline

typedef struct dgx_vertex_input_binding_info {
    uint32_t                            binding;
    uint32_t                            stride;
    dgx_vertex_attribute_input_rate     input_rate;
} dgx_vertex_input_binding_info;

typedef struct dgx_vertex_input_attribute_info {
    uint32_t                            binding;
    uint32_t                            location;
    dgx_data_type                       type;
    uint32_t                            offset;
} dgx_vertex_input_attribute_info;

typedef struct dgx_pipeline_vertex_layout {
    uint32_t                            bindings_count;
    dgx_vertex_input_binding_info*      bindings;
    uint32_t                            attributes_count;
    dgx_vertex_input_attribute_info*    attributes;
} dgx_pipeline_vertex_layout;

typedef struct dgx_pipeline_input_assembly {
    dgx_primitive_topology              topology;
} dgx_pipeline_input_assembly;

typedef struct dgx_pipeline_rasterizer_state {
    dgx_cull_mode                       cull_mode;
    dgx_fill_mode                       fill_mode;
    int                                 depth_clamp_enable;
    int                                 scissor_enable;
} dgx_pipeline_rasterizer_state;

typedef struct dgx_pipeline_shader_stages {
    dgx_shader*                         vertex;
    dgx_shader*                         geometry;
    dgx_shader*                         pixel;
} dgx_pipeline_shader_stages;

typedef struct dgx_pipeline_blend_state {
    int                                 blend_enable;
    dgx_blend_op                        blend_op;
    dgx_blend_factor                    src_factor;
    dgx_blend_factor                    dst_factor;
} dgx_pipeline_blend_state;

typedef struct dgx_pipeline_depth_stencil_state {
    int                                 depth_test_enable;
    int                                 depth_write_enable;
    int                                 stencil_test_enable;
} dgx_pipeline_depth_stencil_state;

typedef struct dgx_pipeline_create_info {
    dgx_render_target_layout*           render_target_layout;
    dgx_pipeline_descriptors_layout*    descriptor_layout;
    dgx_pipeline_vertex_layout          vertex_layout;
    dgx_pipeline_shader_stages          shader_stages;
    dgx_pipeline_input_assembly         input_assembly;
    dgx_pipeline_rasterizer_state       rasterizer;
    dgx_pipeline_blend_state            blend;
    dgx_pipeline_depth_stencil_state    depth_stencil;
} dgx_pipeline_create_info;

typedef struct dgx_pipeline dgx_pipeline;

dgx_pipeline* dgx_create_pipeline(dgx_hardware*, const dgx_pipeline_create_info* info);
void dgx_free_pipeline(dgx_pipeline* pipeline);

// ===========================
// Generic Commands  (cmd)

typedef enum dgx_buffer_sync_point {
    dgx_buffer_sync_point_this_command,

    // transfer
    dgx_buffer_sync_point_transfer_source,
    dgx_buffer_sync_point_transfer_destination,

    // compute
    dgx_buffer_sync_point_compute_read,
    dgx_buffer_sync_point_compute_write,

    // graphics shader stages
    dgx_buffer_sync_point_vertex_shader_read,
    dgx_buffer_sync_point_fragment_shader_read,

    // fixed-function input
    dgx_buffer_sync_point_vertex_buffer,
    dgx_buffer_sync_point_index_buffer,
} dgx_buffer_sync_point;

void dgx_cmd_sync_buffers(
    dgx_command_list*       target,

    dgx_buffer_sync_point   previous_use,
    dgx_buffer_sync_point   next_use,

    uint32_t                buffers_count,
    dgx_buffer**            buffers
);

typedef enum dgx_texture_sync_point {
    dgx_texture_sync_point_this_command,

    // transfer
    dgx_texture_sync_point_transfer_source,
    dgx_texture_sync_point_transfer_destination,

    // compute
    dgx_texture_sync_point_compute_read,
    dgx_texture_sync_point_compute_write,

    // shader sampling
    dgx_texture_sync_point_vertex_shader_read,
    dgx_texture_sync_point_fragment_shader_read,

    // attachments
    dgx_texture_sync_point_color_attachment,
    dgx_texture_sync_point_depth_attachment,
} dgx_texture_sync_point;

void dgx_cmd_sync_textures(
    dgx_command_list*       target,

    dgx_texture_sync_point  previous_use,
    dgx_texture_sync_point  next_use,

    uint32_t                textures_count,
    dgx_texture**           textures
);

void dgx_cmd_copy_staging_memory_to_buffer(
    dgx_command_list*       target,
    dgx_staging_memory*     staging_memory,
    dgx_buffer*             target_buffer,
    uint32_t                staging_memory_region_offset,
    uint32_t                buffer_write_region_offset,
    uint32_t                buffer_write_region_size
);

void dgx_cmd_copy_staging_memory_to_texture(
    dgx_command_list*       target,
    dgx_staging_memory*     staging_memory,
    dgx_texture*            target_texture,
    uint32_t                staging_memory_region_offset,
    dgx_texture_dimensions  texture_write_region_offset,
    dgx_texture_dimensions  texture_write_region_size
);

// ===========================
// Graphics Commands (gcmd)

typedef struct dgx_gcmd_begin_render_target_write_info {
    dgx_render_target*  render_target;
    uint32_t            clear_colors_count;
    dgx_color*          clear_colors;
} dgx_gcmd_begin_render_target_write_info;

void dgx_gcmd_begin_render_target_write(
    dgx_command_list*   target,
    dgx_gcmd_begin_render_target_write_info* info
);

void dgx_gcmd_end_render_target_write(
    dgx_command_list*   target
);

void dgx_gcmd_bind_graphics_pipeline(
    dgx_command_list*   target,
    dgx_pipeline*       pipeline
);

void dgx_gcmd_bind_graphics_pipeline_vertex_buffer(
    dgx_command_list*   target,
    dgx_buffer*         buffer,
    uint32_t            offset,
    uint32_t            binding
);

void dgx_gcmd_bind_graphics_pipeline_index_buffer(
    dgx_command_list*   target,
    dgx_buffer*         buffer,
    uint32_t            offset,
    int                 uint32_not_uint16
);

void dgx_gcmd_bind_graphics_pipeline_descriptors(
    dgx_command_list*                   target,
    dgx_pipeline_descriptors_layout*    layout,
    uint32_t                            first_descriptor_index,
    uint32_t                            descriptors_count,
    dgx_descriptor**                    descriptors
);

void dgx_gcmd_draw_vertices(
    dgx_command_list*   target,

    uint32_t            vertices_count,
    uint32_t            vertices_buffer_offset_index,

    uint32_t            instances_count,
    uint32_t            instances_id_values_offset
);

void dgx_gcmd_draw_indexed(
    dgx_command_list*   target,

    uint32_t            indices_count,
    uint32_t            indicies_buffer_offset_index,
    int32_t             indicies_values_offset,

    uint32_t            instances_count,
    uint32_t            instances_id_values_offset
);

void dgx_gcmd_set_scissors(
    dgx_command_list*   target,
    float               root_x,  
    float               root_y,
    float               width,   
    float               height
);

void dgx_gcmd_set_viewport(
    dgx_command_list*   target,
    float               root_x,  
    float               root_y,
    float               width,   
    float               height
);

#endif

#ifdef DEMIGURG_GRAPHICS_IMPL

// Vulkan Implementation
#ifdef DEMIGURG_GRAPHICS_VULKAN

#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdalign.h>

#include <string.h>
#include <assert.h>

// ===========================
// Config

extern const char**             instance_extensions_array;
extern const uint32_t           instance_extensions_count;

extern const char**             config_validation_layers_array;
extern const uint32_t           config_validation_layers_count;

extern const char**             config_required_device_extensions_array;
extern const uint32_t           config_required_device_extensions_count;

extern const VkDynamicState*    config_all_pipelines_dynamic_state_array;
extern const uint32_t           config_all_pipelines_dynamic_state_count;

// ===========================
// Helpers

static inline uint32_t min_u32(uint32_t a, uint32_t b) {
    if (a < b) return a;
    return b;
}

static inline uint32_t max_u32(uint32_t a, uint32_t b) {
    if (a < b) return b;
    return a;
}

static inline uint32_t clamp_u32(uint32_t a, uint32_t min, uint32_t max) {
    if (a < min) return min;
    if (a > max) return max;
    return a;
}

// ===========================
// Thread Local Operational Memory

/*
    THIS IS UNSAFE AND NEEDS TO BE FIXED ASAP
*/

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define thread_local _Thread_local
#elif defined(_MSC_VER)
    #define thread_local __declspec(thread)
#elif defined(__GNUC__)
    #define thread_local __thread
#else
    #error "Thread-local storage not supported, and required"
#endif

#define tlom_size (16 * 1024 * 1024)
extern thread_local char tlom[tlom_size];

static inline uint32_t uint32_t_align_up(uint32_t x, uint32_t a) {
    return (x + (a - 1)) & ~(a - 1);
}

static inline void* tlom_alloc_only(uint32_t block) {
    if (block > tlom_size) {
        assert(0 && "Overloaded thread local operational memory");
    }

    return tlom;
}

static inline void* tlom_alloc(uint32_t* iterator, uint32_t block) {
    char* position = &tlom[*iterator];
    *iterator = uint32_t_align_up(*iterator + block, 16);

    if (*iterator > tlom_size) {
        assert(0 && "Overloaded thread local operational memory");
        return tlom;
    }

    return position;
}

// ===========================
// Structures Definitions

struct dgx_library {
    // Vulkan
    VkInstance  instance;

    // Platform
    int platform_enabled;
};

struct dgx_hardware_queue {
    VkQueue     handle;
    uint32_t    family_index;
    uint32_t    queue_index;
};

typedef struct hardware_dedicated_memory    hardware_dedicated_memory;
typedef struct hardware_paged_memory        hardware_paged_memory;

struct dgx_hardware {
    dgx_library*                        owning_library;

    // devices

    VkPhysicalDevice                    physical_device;
    VkPhysicalDeviceProperties          physical_device_properties;
    VkDevice                            logical_device;

    // memory allocator

    VkPhysicalDeviceMemoryProperties    memory_properties;
    hardware_paged_memory**             paged_per_type;     // blocks by memory type array

    // queues

    uint32_t                            graphics_queues_count;
    dgx_hardware_queue*                 graphics_queues;

    uint32_t                            transfer_queues_count;
    dgx_hardware_queue*                 transfer_queues;

    uint32_t                            compute_queues_count;
    dgx_hardware_queue*                 compute_queues;

    uint32_t                            trs_cmp_queues_count;
    dgx_hardware_queue*                 trs_cmp_queues;

    dgx_hardware_queue*                 presentation_queue;
};

typedef struct swapchain_bundle {
    uint32_t                    width;
    uint32_t                    height;

    VkSwapchainKHR              swapchain;
    dgx_render_target_layout*   render_target_layout;

    uint32_t                    images_count;
    VkImage*                    images;
    VkImageView*                images_views;
    dgx_render_target**         render_targets;
} swapchain_bundle;

struct dgx_window {
    // Constant once created
    dgx_hardware*       owning_hardware;
    uint32_t            desired_render_targets;
    GLFWwindow*         platform_window;
    VkSurfaceKHR        surface;

    // Input
    float               window_scroll_input;

    // Also constant, may be null

    dgx_window_render_targets_recreated_callback recreate_callback;

    // Changing
    swapchain_bundle    current_swapchain;
    uint32_t            retired_count;
    uint32_t            retired_capacity;
    swapchain_bundle*   retired_swapchains;
};

typedef struct command_lists_block command_lists_block;

struct dgx_command_list {
    dgx_command_lists_allocator*    owning_allocator;
    VkCommandBuffer                 command_buffer;
    uint8_t                         in_block_index;
};

struct dgx_command_lists_allocator {
    const dgx_hardware*     owning_hardware;
    VkCommandPool           command_pool;
    command_lists_block*    front_block;
};

struct dgx_staging_memory {
    dgx_hardware*   owning_hardware;
    VkBuffer        buffer;
    VkDeviceMemory  memory;
};

struct dgx_cpu_signal {
    dgx_hardware*   owning_hardware;
    VkFence         fence;
};

struct dgx_gpu_signal {
    dgx_hardware*   owning_hardware;
    VkSemaphore     semaphore;
};

struct dgx_shader {
    dgx_hardware*   owning_hardware;
    VkShaderModule  module;
};

struct dgx_buffer {
    dgx_hardware*   owning_hardware;
    VkBuffer        buffer;
    VkDeviceMemory  memory;
    uint32_t        size_bytes;
};

struct dgx_sampler {
    dgx_hardware*   owning_hardware;
    VkSampler       sampler;
};

struct dgx_texture {
    dgx_hardware*           owning_hardware;
    uint32_t                bytes_per_pixel;
    dgx_texture_dimensions  dimensions;
    VkImage                 image;
    VkImageView             view;
    VkDeviceMemory          memory;
};

struct dgx_render_target_layout {
    dgx_hardware*   owning_hardware;
    VkRenderPass    renderpass;
};

struct dgx_render_target {
    dgx_hardware*                   owning_hardware;
    const dgx_render_target_layout* layout;
    VkFramebuffer                   framebuffer;
    uint32_t                        width;
    uint32_t                        height;
};

struct dgx_descriptor_layout {
    dgx_hardware*           owning_hardware;
    VkDescriptorSetLayout   dsc_set_layout;

    uint32_t sampler_bindings;
    uint32_t sampled_textures_bindings;
    uint32_t storage_buffers_bindings;
    uint32_t uniform_buffers_bindings;
};

struct dgx_descriptor_allocator {
    dgx_hardware*               owning_hardware;
    dgx_descriptor_layout*      target_layout;
    uint32_t                    max_descriptors;
    VkDescriptorPool            vk_pool;
    dgx_descriptor*             dgx_pool;
};

struct dgx_descriptor {
    dgx_descriptor_allocator*   owning_allocator;
    VkDescriptorSet             dsc_set;
    uint8_t                     alive;
};

struct dgx_pipeline_descriptors_layout {
    dgx_hardware*       owning_hardware;
    VkPipelineLayout    layout;
};

struct dgx_pipeline {
    dgx_hardware*       owning_hardware;
    VkPipeline          pipeline;
};

// ===========================
// Hardware Memory Allocator

// 1 at success
int try_allocate_hardware_memory(
    dgx_hardware*                   hardware,
    dgx_memory_allocation_strategy  strategy,
    VkMemoryRequirements            memory_requirements,
    VkDeviceMemory*                 result_memory,
    uint32_t*                       result_offset
);

// ===========================
// Vulkan Methods

typedef struct queues_family_info {
    uint32_t    presentation_queues_family;
    uint32_t    presentation_queues_count;

    uint32_t    graphics_queues_family;
    uint32_t    graphics_queues_count;

    uint32_t    transfer_queues_family;
    uint32_t    transfer_queues_count;

    uint32_t    compute_queues_family;
    uint32_t    compute_queues_count;

    uint32_t    transfer_compute_queues_family;
    uint32_t    transfer_compute_queues_count;
} queues_family_info;

queues_family_info get_physical_device_queues_family_info(VkPhysicalDevice device, VkSurfaceKHR surface);

typedef struct swapchain_support_details {
    VkSurfaceCapabilitiesKHR capabilities;

    uint32_t                 formats_count;
    VkSurfaceFormatKHR*      formats;

    uint32_t                 present_modes_count;
    VkPresentModeKHR*        present_modes;
} swapchain_support_details;

swapchain_support_details get_swapchain_support_details(VkPhysicalDevice device, VkSurfaceKHR surface);
void free_swapchain_support_details(swapchain_support_details details);

// driver sorts memory types from fastest to slowest
// pick first meeting requirements
static inline uint32_t find_memory_type(
    const VkPhysicalDeviceMemoryProperties* properties, 
    uint32_t                                type_filter, 
    VkMemoryPropertyFlags                   requirements, 
    uint32_t                                search_start
) {
    for (uint32_t i = search_start; i < properties->memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (properties->memoryTypes[i].propertyFlags & requirements) == requirements) {
            return i;
        }
    }

    // failure
    return UINT32_MAX;
}

static inline uint32_t vk_format_pixel_size(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R8_UNORM:            return 1;
    case VK_FORMAT_R8G8_UNORM:          return 2;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT:          return 4;

    case VK_FORMAT_R16_SFLOAT:          return 2;
    case VK_FORMAT_R16G16_SFLOAT:       return 4;
    case VK_FORMAT_R16G16B16A16_SFLOAT: return 8;

    case VK_FORMAT_R32G32_SFLOAT:       return 8;
    case VK_FORMAT_R32G32B32A32_SFLOAT: return 16;

    case VK_FORMAT_D16_UNORM:           return 2;
    default: assert(0 && "Unsupported format");
    }

    return 0;
}

// ===========================
// Enumerations Conversion

static inline VkFormat dgx_to_vk_format(dgx_data_type type) {
    switch (type) {
        case dgx_data_type_int32:     return VK_FORMAT_R32_SINT;
        case dgx_data_type_vec2i32:   return VK_FORMAT_R32G32_SINT;
        case dgx_data_type_vec3i32:   return VK_FORMAT_R32G32B32_SINT;
        case dgx_data_type_vec4i32:   return VK_FORMAT_R32G32B32A32_SINT;

        case dgx_data_type_float32:   return VK_FORMAT_R32_SFLOAT;
        case dgx_data_type_vec2f32:   return VK_FORMAT_R32G32_SFLOAT;
        case dgx_data_type_vec3f32:   return VK_FORMAT_R32G32B32_SFLOAT;
        case dgx_data_type_vec4f32:   return VK_FORMAT_R32G32B32A32_SFLOAT;

        default: assert(0 && "Invalid dgx_data_type!");
    }

    return VK_FORMAT_UNDEFINED;
}

static inline VkVertexInputRate dgx_to_vk_vertex_input_rate(dgx_vertex_attribute_input_rate rate) {
    switch (rate) {
        case dgx_vertex_attribute_input_rate_per_vertex:    return VK_VERTEX_INPUT_RATE_VERTEX;
        case dgx_vertex_attribute_input_rate_per_instance:  return VK_VERTEX_INPUT_RATE_INSTANCE;
        default: assert(0 && "Invalid dgx_vertex_attribute_input_rate!");
    }

    return VK_VERTEX_INPUT_RATE_VERTEX;
}

static inline VkShaderStageFlags dgx_to_vk_shader_stage(dgx_shader_stages_bitmask stages) {
    VkShaderStageFlags flags = 0;

    if (stages & dgx_shader_stage_vertex)   flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (stages & dgx_shader_stage_geometry) flags |= VK_SHADER_STAGE_GEOMETRY_BIT; 
    if (stages & dgx_shader_stage_pixel)    flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stages & dgx_shader_stage_compute)  flags |= VK_SHADER_STAGE_COMPUTE_BIT;

    assert(flags && "Invalid dgx_shader_stage!");
    return flags;
}

static inline VkBufferUsageFlags dgx_memory_access_to_vk_buffer_usage(dgx_memory_access access) {
    switch (access) {
    case dgx_memory_access_gpu_only: 
        return 0;

    case dgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_read: 
        return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    case dgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_write:
        return VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    case dgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_read_and_write:
        return VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    default: assert(0 && "Invalid dgx_memory_access!");
    }

    return 0;
}

static inline VkBufferUsageFlags dgx_buffer_usage_to_vk_buffer_usage(dgx_buffer_usage usage) {
    switch (usage) {
        case dgx_buffer_usage_vertex:   return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        case dgx_buffer_usage_index:    return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        case dgx_buffer_usage_uniform:  return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        case dgx_buffer_usage_storage:  return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        default: assert(0 && "invalid dgx_buffer_usage");
    }

    return 0;
}

static inline VkImageUsageFlags dgx_memory_access_to_vk_image_usage(dgx_memory_access access) {
    switch (access) {
    case dgx_memory_access_gpu_only: 
        return 0;

    case dgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_read: 
        return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    case dgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_write:
        return VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    case dgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_read_and_write:
        return VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    default: assert(0 && "Invalid dgx_memory_access!");
    }

    return 0;
}

static inline VkImageUsageFlags dgx_texture_usage_to_vk_image_usage(dgx_texture_usage usage) {
    switch (usage) {
    case dgx_texture_usage_sampled:                     return VK_IMAGE_USAGE_SAMPLED_BIT;
    case dgx_texture_usage_color_attachment:            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    case dgx_texture_usage_depth_stencil_attachment:    return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    case dgx_texture_usage_storage:                     return VK_IMAGE_USAGE_STORAGE_BIT;
    default: assert(0 && "Invalid dgx_texture_usage!");
    }

    return 0;
}

static inline VkFilter dgx_to_vk_filter(dgx_sampler_filter filter) {
    switch (filter) {
    case dgx_sampler_filter_nearest:    return VK_FILTER_NEAREST;
    case dgx_sampler_filter_linear:     return VK_FILTER_LINEAR;
    default: assert(0 && "Invalid dgx_sampler_filter!");
    }

    return 0;
}

static inline VkSamplerMipmapMode dgx_to_vk_mipmap_mode(dgx_sampler_filter filter) {
    switch (filter) {
    case dgx_sampler_filter_nearest:    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case dgx_sampler_filter_linear:     return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    default: assert(0 && "Invalid dgx_sampler_filter!");
    }

    return 0;
}

static inline VkSamplerAddressMode dgx_to_vk_sampler_wrapping(dgx_sampler_wrapping wrapping) {
    switch (wrapping) {
        case dgx_sampler_wrapping_repeat:                   return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case dgx_sampler_wrapping_repeat_mirrored:          return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case dgx_sampler_wrapping_repeat_clamp_coordinates: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case dgx_sampler_wrapping_repeat_clamp_texture:     return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default: assert(0 && "Invalid dgx_sampler_wrapping!");
    }

    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

static inline VkImageType dgx_texture_type_to_vk_image_type(dgx_texture_type type) {
    switch (type) {
        case dgx_texture_type_1d:       return VK_IMAGE_TYPE_1D;
        case dgx_texture_type_2d:       return VK_IMAGE_TYPE_2D;
        case dgx_texture_type_cubemap:  return VK_IMAGE_TYPE_2D;
        case dgx_texture_type_3d:       return VK_IMAGE_TYPE_3D;
        default: assert(0 && "Invalid dgx_texture_type");
    }

    return VK_IMAGE_TYPE_2D;
}

static inline VkImageViewType dgx_texture_type_to_vk_image_view_type(dgx_texture_type type, uint32_t array_length) {
    if (type == dgx_texture_type_1d) {
        if (array_length > 1) return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        return VK_IMAGE_VIEW_TYPE_1D;
    }

    if (type == dgx_texture_type_2d) {
        if (array_length > 1) return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        return VK_IMAGE_VIEW_TYPE_2D;
    }

    if (type == dgx_texture_type_3d) {
        return VK_IMAGE_VIEW_TYPE_3D;
    }

    if (type == dgx_texture_type_cubemap) {
        if (array_length > 1) return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        return VK_IMAGE_VIEW_TYPE_CUBE;
    }

    assert(0 && "Invalid dgx_texture_type");
    return VK_IMAGE_VIEW_TYPE_2D;
}

static inline VkFormat dgx_to_vk_texture_format(dgx_texture_format format) {
    switch (format) {
        case dgx_texture_format_undefined:              return VK_FORMAT_UNDEFINED;

        case dgx_texture_format_r8_unorm:               return VK_FORMAT_R8_UNORM;
        case dgx_texture_format_rg8_unorm:              return VK_FORMAT_R8G8_UNORM;
        case dgx_texture_format_rgba8_unorm:            return VK_FORMAT_R8G8B8A8_UNORM;
        case dgx_texture_format_rgba8_srgb:             return VK_FORMAT_R8G8B8A8_SRGB;
        case dgx_texture_format_bgra8_unorm:            return VK_FORMAT_B8G8R8A8_UNORM;
        case dgx_texture_format_bgra8_srgb:             return VK_FORMAT_B8G8R8A8_SRGB;

        case dgx_texture_format_r16_float:              return VK_FORMAT_R16_SFLOAT;
        case dgx_texture_format_rg16_float:             return VK_FORMAT_R16G16_SFLOAT;
        case dgx_texture_format_rgba16_float:           return VK_FORMAT_R16G16B16A16_SFLOAT;

        case dgx_texture_format_r32_float:              return VK_FORMAT_R32_SFLOAT;
        case dgx_texture_format_rg32_float:             return VK_FORMAT_R32G32_SFLOAT;
        case dgx_texture_format_rgba32_float:           return VK_FORMAT_R32G32B32A32_SFLOAT;

        case dgx_texture_format_depth16_unorm:          return VK_FORMAT_D16_UNORM;
        case dgx_texture_format_depth24_unorm_stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
        case dgx_texture_format_depth32_float:          return VK_FORMAT_D32_SFLOAT;

        default: assert(0 && "Invalid dgx_texture_format!");
    }

    return VK_FORMAT_UNDEFINED;
}

static inline VkDescriptorType dgx_to_vk_descriptor_type(dgx_descriptor_binding_type type) {
    switch (type) {
        case dgx_descriptor_binding_type_sampler:           return VK_DESCRIPTOR_TYPE_SAMPLER;
        case dgx_descriptor_binding_type_sampled_texture:   return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case dgx_descriptor_binding_type_uniform_buffer:    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case dgx_descriptor_binding_type_storage_buffer:    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        default: assert(0 && "Invalid dgx_descriptor_type!");
    }

    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

static inline VkAttachmentLoadOp dgx_to_vk_load_op(dgx_load_op op) {
    switch (op) {
        case dgx_load_op_load:       return VK_ATTACHMENT_LOAD_OP_LOAD;
        case dgx_load_op_clear:      return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case dgx_load_op_dont_care:  return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        default: assert(0 && "Invalid dgx_load_op!");
    }

    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

static inline VkAttachmentStoreOp dgx_to_vk_store_op(dgx_store_op op) {
    switch (op) {
        case dgx_store_op_store:     return VK_ATTACHMENT_STORE_OP_STORE;
        case dgx_store_op_dont_care: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        default: assert(0 && "Invalid dgx_store_op!");
    }

    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

static inline VkPrimitiveTopology dgx_to_vk_primitive_topology(dgx_primitive_topology topology) {
    switch (topology) {
        case dgx_primitive_topology_point_list:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case dgx_primitive_topology_line_list:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case dgx_primitive_topology_line_strip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case dgx_primitive_topology_triangle_list:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case dgx_primitive_topology_triangle_strip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        default: assert(0 && "Invalid dgx_primitive_topology!");
    }

    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

static inline VkCullModeFlags dgx_to_vk_cull_mode(dgx_cull_mode mode) {
    switch (mode) {
        case dgx_cull_mode_none:            return VK_CULL_MODE_NONE;
        case dgx_cull_mode_front:           return VK_CULL_MODE_FRONT_BIT;
        case dgx_cull_mode_back:            return VK_CULL_MODE_BACK_BIT;
        case dgx_cull_mode_front_and_back:  return VK_CULL_MODE_FRONT_AND_BACK;
        default: assert(0 && "Invalid dgx_cull_mode!");
    }

    return VK_CULL_MODE_NONE;
}

static inline VkPolygonMode dgx_to_vk_fill_mode(dgx_fill_mode mode) {
    switch (mode) {
        case dgx_fill_mode_solid:       return VK_POLYGON_MODE_FILL;
        case dgx_fill_mode_wireframe:   return VK_POLYGON_MODE_LINE;
        default: assert(0 && "Invalid dgx_fill_mode!");
    }

    return VK_POLYGON_MODE_FILL;
}

static inline VkBlendFactor dgx_to_vk_blend_factor(dgx_blend_factor factor) {
    switch (factor) {
        case dgx_blend_factor_zero:                     return VK_BLEND_FACTOR_ZERO;
        case dgx_blend_factor_one:                      return VK_BLEND_FACTOR_ONE;

        case dgx_blend_factor_src_color:                return VK_BLEND_FACTOR_SRC_COLOR;
        case dgx_blend_factor_one_minus_src_color:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;

        case dgx_blend_factor_dst_color:                return VK_BLEND_FACTOR_DST_COLOR;
        case dgx_blend_factor_one_minus_dst_color:      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;

        case dgx_blend_factor_src_alpha:                return VK_BLEND_FACTOR_SRC_ALPHA;
        case dgx_blend_factor_one_minus_src_alpha:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

        case dgx_blend_factor_dst_alpha:                return VK_BLEND_FACTOR_DST_ALPHA;
        case dgx_blend_factor_one_minus_dst_alpha:      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;

        case dgx_blend_factor_constant_color:           return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case dgx_blend_factor_one_minus_constant_color: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;

        case dgx_blend_factor_constant_alpha:           return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case dgx_blend_factor_one_minus_constant_alpha: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;

        case dgx_blend_factor_src_alpha_saturate:       return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        default: assert(0 && "Invalid dgx_blend_factor!");
    }

    return VK_BLEND_FACTOR_ZERO;
}

static inline VkBlendOp dgx_to_vk_blend_op(dgx_blend_op op) {
    switch (op) {
        case dgx_blend_op_add:              return VK_BLEND_OP_ADD;
        case dgx_blend_op_subtract:         return VK_BLEND_OP_SUBTRACT;
        case dgx_blend_op_reverse_subtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case dgx_blend_op_min:              return VK_BLEND_OP_MIN;
        case dgx_blend_op_max:              return VK_BLEND_OP_MAX;
        default: assert(0 && "Invalid dgx_blend_op!");
    }

    return VK_BLEND_OP_ADD;
}

static inline void dgx_translate_buffer_usage(
    dgx_buffer_sync_point   prev,
    dgx_buffer_sync_point   next,
    VkPipelineStageFlags*   srcStage,
    VkPipelineStageFlags*   dstStage,
    VkAccessFlags*          srcAccess,
    VkAccessFlags*          dstAccess
) {
    // previous usage
    switch (prev) {
        case dgx_buffer_sync_point_this_command:
            *srcStage  = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            *srcAccess = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            break;

        case dgx_buffer_sync_point_transfer_source:
            *srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            *srcAccess = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case dgx_buffer_sync_point_transfer_destination:
            *srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            *srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case dgx_buffer_sync_point_compute_read:
            *srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            *srcAccess = VK_ACCESS_SHADER_READ_BIT;
            break;

        case dgx_buffer_sync_point_compute_write:
            *srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            *srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
            break;

        case dgx_buffer_sync_point_vertex_shader_read:
            *srcStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            *srcAccess = VK_ACCESS_SHADER_READ_BIT;
            break;

        case dgx_buffer_sync_point_fragment_shader_read:
            *srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            *srcAccess = VK_ACCESS_SHADER_READ_BIT;
            break;

        case dgx_buffer_sync_point_vertex_buffer:
            *srcStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            *srcAccess = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            break;

        case dgx_buffer_sync_point_index_buffer:
            *srcStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            *srcAccess = VK_ACCESS_INDEX_READ_BIT;
            break;

        default: assert(0 && "Invalid dgx_buffer_sync_point!");
    }

    // next usage
    switch (next) {
        case dgx_buffer_sync_point_this_command:
            *srcStage  = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            *srcAccess = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            break;

        case dgx_buffer_sync_point_transfer_source:
            *dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            *dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case dgx_buffer_sync_point_transfer_destination:
            *dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            *dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case dgx_buffer_sync_point_compute_read:
            *dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            *dstAccess = VK_ACCESS_SHADER_READ_BIT;
            break;

        case dgx_buffer_sync_point_compute_write:
            *dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            *dstAccess = VK_ACCESS_SHADER_WRITE_BIT;
            break;

        case dgx_buffer_sync_point_vertex_shader_read:
            *dstStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            *dstAccess = VK_ACCESS_SHADER_READ_BIT;
            break;

        case dgx_buffer_sync_point_fragment_shader_read:
            *dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            *dstAccess = VK_ACCESS_SHADER_READ_BIT;
            break;

        case dgx_buffer_sync_point_vertex_buffer:
            *dstStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            *dstAccess = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            break;

        case dgx_buffer_sync_point_index_buffer:
            *dstStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            *dstAccess = VK_ACCESS_INDEX_READ_BIT;
            break;

        default: assert(0 && "Invalid dgx_buffer_sync_point!");
    }
}

static inline void dgx_translate_texture_usage(
    dgx_texture_sync_point  prev,
    dgx_texture_sync_point  next,
    VkPipelineStageFlags*   srcStage,
    VkPipelineStageFlags*   dstStage,
    VkAccessFlags*          srcAccess,
    VkAccessFlags*          dstAccess,
    VkImageLayout*          oldLayout,
    VkImageLayout*          newLayout
) {
    // previous usage
    switch (prev) {
        case dgx_texture_sync_point_this_command:
            *srcStage  = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            *srcAccess = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            *oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            break;

        case dgx_texture_sync_point_transfer_source:
            *srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            *srcAccess = VK_ACCESS_TRANSFER_READ_BIT;
            *oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            break;

        case dgx_texture_sync_point_transfer_destination:
            *srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            *srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
            *oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            break;

        case dgx_texture_sync_point_compute_read:
            *srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            *srcAccess = VK_ACCESS_SHADER_READ_BIT;
            *oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            break;

        case dgx_texture_sync_point_compute_write:
            *srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            *srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
            *oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            break;

        case dgx_texture_sync_point_vertex_shader_read:
            *srcStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            *srcAccess = VK_ACCESS_SHADER_READ_BIT;
            *oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;

        case dgx_texture_sync_point_fragment_shader_read:
            *srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            *srcAccess = VK_ACCESS_SHADER_READ_BIT;
            *oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;

        case dgx_texture_sync_point_color_attachment:
            *srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            *srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            *oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;

        case dgx_texture_sync_point_depth_attachment:
            *srcStage =
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

            *srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            *oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;

        default: assert(0 && "Invalid dgx_texture_sync_point!");
    }

    // next usage
    switch (next) {
        case dgx_texture_sync_point_this_command:
            *srcStage  = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            *srcAccess = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            *oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            break;

        case dgx_texture_sync_point_transfer_source:
            *dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            *dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
            *newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            break;

        case dgx_texture_sync_point_transfer_destination:
            *dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            *dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
            *newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            break;

        case dgx_texture_sync_point_compute_read:
            *dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            *dstAccess = VK_ACCESS_SHADER_READ_BIT;
            *newLayout = VK_IMAGE_LAYOUT_GENERAL;
            break;

        case dgx_texture_sync_point_compute_write:
            *dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            *dstAccess = VK_ACCESS_SHADER_WRITE_BIT;
            *newLayout = VK_IMAGE_LAYOUT_GENERAL;
            break;

        case dgx_texture_sync_point_vertex_shader_read:
            *dstStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            *dstAccess = VK_ACCESS_SHADER_READ_BIT;
            *newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;

        case dgx_texture_sync_point_fragment_shader_read:
            *dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            *dstAccess = VK_ACCESS_SHADER_READ_BIT;
            *newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;

        case dgx_texture_sync_point_color_attachment:
            *dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            *dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            *newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;

        case dgx_texture_sync_point_depth_attachment:
            *dstStage =
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

            *dstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            *newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;

        default: assert(0 && "Invalid dgx_texture_sync_point!");
    }
}


/* ===== buffer.c ===== */

dgx_buffer* dgx_create_buffer(dgx_hardware* hardware, const dgx_buffer_create_info* info) {
    VkBufferCreateInfo buffer_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = info->size_bytes,
        .usage       = dgx_buffer_usage_to_vk_buffer_usage(info->usage) | dgx_memory_access_to_vk_buffer_usage(info->memory_access),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer vkbuffer; if (vkCreateBuffer(hardware->logical_device, &buffer_info, 0, &vkbuffer) != VK_SUCCESS) {
        return 0x0; // ("failed to create vertex buffer!");
    }

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(hardware->logical_device, vkbuffer, &memory_requirements);

    VkDeviceMemory given_memory; uint32_t given_offset;
    int success = try_allocate_hardware_memory(
        hardware,
        info->memory_strategy,
        memory_requirements,
        &given_memory, 
        &given_offset
    );

    if (!success) {
        vkDestroyBuffer(hardware->logical_device, vkbuffer, 0);
        return 0x0; // failed to allocate memory
    }

    vkBindBufferMemory(hardware->logical_device, vkbuffer, given_memory, given_offset);

    dgx_buffer* buffer = malloc(sizeof(dgx_buffer));
    *buffer = (dgx_buffer){
        .owning_hardware    = hardware,
        .buffer             = vkbuffer,
        .memory             = given_memory,
        .size_bytes         = info->size_bytes
    };

    return buffer;
}

void dgx_free_buffer(dgx_buffer* buffer) {
    if (!buffer) return;
    vkDestroyBuffer(buffer->owning_hardware->logical_device, buffer->buffer, 0);
    vkFreeMemory(buffer->owning_hardware->logical_device, buffer->memory, 0); // todo if paged
    free(buffer);
}

uint64_t dgx_buffer_get_size_bytes(dgx_buffer* buffer) {
    return buffer->size_bytes;
}

int dgx_buffer_sync_upload(dgx_buffer* buffer, uint64_t buffer_offset, const void* data, uint64_t data_size_bytes) {
    // temporary objects:
    dgx_staging_memory*             staging_memory = NULL;
    dgx_command_lists_allocator*    allocator = NULL;
    dgx_command_list*               list = NULL;
    dgx_cpu_signal*                 signal = NULL;
    dgx_hardware_queue*             queue = NULL;
    int success = 1;

    dgx_staging_memory_create_info staging_info = {
        .size_bytes = data_size_bytes
    };
    staging_memory = dgx_create_staging_memory(buffer->owning_hardware, &staging_info);
    if (!staging_memory) { success = 0; goto _cleanup; }

    void* mapped = dgx_staging_memory_map(staging_memory, 0, staging_info.size_bytes);
    if (!mapped) { success = 0; goto _cleanup; }
        memcpy(mapped, data, data_size_bytes);
    dgx_staging_memory_unmap(staging_memory);

    // command allocator
    dgx_command_lists_allocator_create_info alloc_info = {
        .target_queue_type = dgx_hardware_queue_type_graphics,
        .often_recorded = 0
    };
    allocator = dgx_create_command_lists_allocator(buffer->owning_hardware, &alloc_info);
    if (!allocator) { success = 0; goto _cleanup; }

    // command list
    list = dgx_command_lists_allocator_alloc_command_list(allocator);
    if (!list) { success = 0; goto _cleanup; }

    dgx_begin_command_list_recording(list);
    dgx_cmd_copy_staging_memory_to_buffer(
        list, staging_memory, buffer,
        0, buffer_offset, data_size_bytes
    );
    dgx_finish_command_list_recording(list);

    // queue
    dgx_hardware_query_queues(buffer->owning_hardware, dgx_hardware_queue_type_graphics, 0, 1, &queue);

    // submit
    dgx_cpu_signal_create_info signal_info = {.initialy_signaled = 0};
    signal = dgx_create_cpu_signal(buffer->owning_hardware, &signal_info);
    if (!signal) { success = 0; goto _cleanup; }

    dgx_submit_info submit_info = {
        .command_lists_count        = 1,
        .command_lists              = &list,
        .wait_gpu_signals_count     = 0,
        .wait_gpu_signals           = 0,
        .signal_gpu_signals_count   = 0,
        .cpu_signal                 = signal
    };

    dgx_submit_command_list(queue, &submit_info);
    dgx_cpu_signal_wait(signal);

    // cleanup
_cleanup:
    dgx_free_cpu_signal(signal);
    dgx_free_command_lists_allocator(allocator);
    dgx_free_staging_memory(staging_memory);
    return success;
}

uint64_t dgx_buffer_multi_upload(
    dgx_buffer_multi_upload_region* regions,
    uint32_t                        region_count,
    dgx_command_list*               command_list,
    dgx_hardware_queue*             queue_for_uploads,
    dgx_staging_memory*             staging_memory,
    uint64_t                        staging_memory_region_offset,
    uint64_t                        staging_memory_region_size,
    dgx_cpu_signal*                 upload_finished_cpu,
    dgx_gpu_signal*                 upload_finished_gpu
) {
    if (region_count == 0) return 0;
    int provided_cpu_signal = (upload_finished_cpu != NULL);

    // Per-region progress, staging offsets, and per-batch byte counts
    uint32_t* copy_positions  = calloc(region_count, sizeof(uint32_t));
    uint32_t* batch_bytes     = malloc(region_count * sizeof(uint32_t));
    uint64_t* batch_stage_off = malloc(region_count * sizeof(uint64_t));

    uint64_t all_bytes        = 0;
    int      multiple_batches = 0;

    int has_remaining = 1;
    while (has_remaining) {
        // Write staging buffer
        char* mapped = dgx_staging_memory_map(staging_memory, staging_memory_region_offset, staging_memory_region_size);
        uint64_t staging_used = 0;

        for (uint32_t r = 0; r < region_count; r++) {
            uint32_t remaining = regions[r].source_bytes - copy_positions[r];
            uint64_t available = staging_memory_region_size - staging_used;
            uint32_t to_copy   = (remaining < (uint32_t)available) ? remaining : (uint32_t)available;

            batch_stage_off[r] = staging_used;
            batch_bytes[r]     = to_copy;

            if (to_copy > 0) {
                memcpy(mapped + staging_used, (char*)regions[r].source_data + copy_positions[r], to_copy);
                staging_used += to_copy;
            }
        }

        dgx_staging_memory_unmap(staging_memory);

        // Reocrd command lists
        dgx_begin_command_list_recording(command_list);
        for (uint32_t r = 0; r < region_count; r++) {
            if (batch_bytes[r] == 0) continue;
            dgx_cmd_copy_staging_memory_to_buffer(
                command_list,
                staging_memory,
                regions[r].buffer,
                staging_memory_region_offset + batch_stage_off[r],
                copy_positions[r] + regions[r].buffer_offset,
                batch_bytes[r]
            );
            all_bytes += batch_bytes[r];
        }
        dgx_finish_command_list_recording(command_list);

        // Advance positions
        for (uint32_t r = 0; r < region_count; r++)
            copy_positions[r] += batch_bytes[r];

        // Check remaining work
        has_remaining = 0;
        for (uint32_t r = 0; r < region_count; r++) {
            if (copy_positions[r] < regions[r].source_bytes) {
                has_remaining = 1;
                break;
            }
        }

        int is_final_batch = !has_remaining;
        multiple_batches |= !is_final_batch;

        // ensure cpu signal exist if multiple batch are to be sent
        if (!is_final_batch && upload_finished_cpu == NULL) {
            upload_finished_cpu = dgx_create_cpu_signal(staging_memory->owning_hardware, &(dgx_cpu_signal_create_info){ 
                .initialy_signaled = 0 
            });
        }

        // submit
        dgx_submit_info submit = {
            .command_lists_count      = 1,
            .command_lists            = &command_list,
            .cpu_signal               = upload_finished_cpu,
            .signal_gpu_signals_count = (is_final_batch && upload_finished_gpu != NULL) ? 1 : 0,
            .signal_gpu_signals       = &upload_finished_gpu,
        };
        dgx_submit_command_list(queue_for_uploads, &submit);

        // sync upload
        if (!is_final_batch) {
            dgx_cpu_signal_wait(upload_finished_cpu);
            dgx_cpu_signal_reset(upload_finished_cpu);
        }
    }

    // free cpu signal if owned
    if (!provided_cpu_signal && upload_finished_cpu != NULL) {
        dgx_cpu_signal_wait(upload_finished_cpu);
        dgx_free_cpu_signal(upload_finished_cpu);
    }

    free(copy_positions);
    free(batch_bytes);
    free(batch_stage_off);

    if (multiple_batches) return all_bytes;
    return 0;
}

/* ===== command_lists_allocator.c ===== */

// Allocator implements freelist
#define block_capacity 128
struct command_lists_block {
    command_lists_block*    previous;
    uint8_t                 free_count;
    uint8_t                 used_count;
    uint8_t                 free_indices[block_capacity];
    dgx_command_list        lists       [block_capacity];
};

static inline uint32_t command_pool_family_index_from_create_info(dgx_hardware* hardware, const dgx_command_lists_allocator_create_info* info) {
    // If you stumbled on memory error here, you probably tried to create command_lists_allocator
    // for a family of queues that is no available on this hardware - it is prohibited to do so.
    switch (info->target_queue_type) {
    case dgx_hardware_queue_type_graphics:          return hardware->graphics_queues[0].family_index;
    case dgx_hardware_queue_type_transfer:          return hardware->transfer_queues[0].family_index;
    case dgx_hardware_queue_type_compute:           return hardware->compute_queues[0].family_index;
    case dgx_hardware_queue_type_transfer_compute:  return hardware->trs_cmp_queues[0].family_index;
    }

    assert(0 && "Invalid dgx_hardware_queue_type!");
    return VK_IMAGE_VIEW_TYPE_2D;
}

static inline VkCommandPoolCreateFlags command_pool_flags_from_create_info(const dgx_command_lists_allocator_create_info* info) {
    VkCommandPoolCreateFlags flag = 0;
    flag |= (info->often_recorded ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 0);
    flag |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    return flag;
}

dgx_command_lists_allocator* dgx_create_command_lists_allocator(dgx_hardware* hardware, const dgx_command_lists_allocator_create_info* info) {
    VkCommandPoolCreateInfo command_pool_create_info = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = command_pool_family_index_from_create_info(hardware, info),
        .flags            = command_pool_flags_from_create_info(info),
    };

    VkCommandPool command_pool;
    if (vkCreateCommandPool(hardware->logical_device, &command_pool_create_info, 0, &command_pool) != VK_SUCCESS) {
        return 0x0; // failed to create command pool
    }

    dgx_command_lists_allocator* allocator = calloc(1, sizeof(dgx_command_lists_allocator));
    *(allocator) = (dgx_command_lists_allocator){
        .owning_hardware    = hardware,
        .command_pool       = command_pool,
        .front_block        = 0x0
    };

    return allocator;
}

void dgx_free_command_lists_allocator(dgx_command_lists_allocator* allocator) {
    if (!allocator) return;

    command_lists_block* block = allocator->front_block;
    while (block) {
        command_lists_block* prev = block->previous;
        free(block); block = prev;
    }

    vkDestroyCommandPool(
        allocator->owning_hardware->logical_device,
        allocator->command_pool, 0
    );

    free(allocator);
}

static inline int block_has_space(command_lists_block* block) {
    return block->free_count > 0 || block->used_count < block_capacity;
}

static inline void block_release_index(command_lists_block* block, uint32_t index) {
    block->free_indices[block->free_count++] = (uint8_t)index;
}

static inline uint32_t block_acquire_index(command_lists_block* block) {
    if (block->free_count > 0) {    // reuse freed slot first
        return block->free_indices[--block->free_count];
    }
    return block->used_count++;     // or grow
}

command_lists_block* get_block_with_available_slot(dgx_command_lists_allocator* allocator) {
    command_lists_block* block = allocator->front_block;

    while (block) {
        if (block_has_space(block)) return block;
        block = block->previous;
    }

    command_lists_block* new_block = calloc(1, sizeof(command_lists_block));
    new_block->previous = allocator->front_block;
    allocator->front_block = new_block;

    return new_block;
}

dgx_command_list* dgx_command_lists_allocator_alloc_command_list(dgx_command_lists_allocator* allocator) {
    command_lists_block* block = get_block_with_available_slot(allocator);
    uint32_t             index = block_acquire_index(block);
    dgx_command_list*    list  = &block->lists[index];

    VkCommandBufferAllocateInfo allocation_info = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = allocator->command_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    if (vkAllocateCommandBuffers(
        allocator->owning_hardware->logical_device,
        &allocation_info,
        &list->command_buffer) != VK_SUCCESS) {
        return 0;
    }

    *list = (dgx_command_list){
        .owning_allocator   = allocator,
        .command_buffer     = list->command_buffer,
        .in_block_index     = index 
    };

    return list;
}

void dgx_command_lists_allocator_free_command_list(dgx_command_list* list) {
    if (!list) return;

    dgx_command_lists_allocator* allocator = list->owning_allocator;
    command_lists_block*         block = allocator->front_block;

    while (block) {
        if (list >= block->lists && list <  block->lists + block_capacity) {
            uint32_t index = (uint32_t)(list - block->lists);

            vkFreeCommandBuffers(
                allocator->owning_hardware->logical_device,
                allocator->command_pool,
                1,
                &list->command_buffer
            );

            block_release_index(block, index);
            return;
        }

        block = block->previous;
    }
}

void dgx_begin_command_list_recording(dgx_command_list* list) {
    VkCommandBufferBeginInfo begin_info = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags              = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        .pInheritanceInfo   = 0
    };

    if (vkBeginCommandBuffer(list->command_buffer, &begin_info) != VK_SUCCESS) {
        return; // failed to begin recording command buffer
    }
}

void dgx_finish_command_list_recording(dgx_command_list* list) {
    if (vkEndCommandBuffer(list->command_buffer) != VK_SUCCESS) {
        return; // failed to record command buffer
    }
}

void dgx_submit_command_list(
    dgx_hardware_queue*     queue,
    const dgx_submit_info*  info
) {
    uint32_t tlom_itr = 0;

    // Gather command buffer objects
    VkCommandBuffer* cmd_bufs = tlom_alloc(&tlom_itr, sizeof(VkCommandBuffer) * info->command_lists_count);
    for (uint32_t i = 0; i < info->command_lists_count; i++) {
        cmd_bufs[i] = info->command_lists[i]->command_buffer;
    }

    // Gather wait semaphores
    VkSemaphore*            wait_sems   = tlom_alloc(&tlom_itr, sizeof(VkSemaphore) * info->wait_gpu_signals_count);
    VkPipelineStageFlags*   wait_stages = tlom_alloc(&tlom_itr, sizeof(VkPipelineStageFlags) * info->wait_gpu_signals_count);
    for (uint32_t i = 0; i < info->wait_gpu_signals_count; i++) {
        wait_sems[i]   = info->wait_gpu_signals[i]->semaphore;
        wait_stages[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    // Gather signal semaphores
    VkSemaphore* sig_sems = tlom_alloc(&tlom_itr, sizeof(VkSemaphore) * info->signal_gpu_signals_count);
    for (uint32_t i = 0; i < info->signal_gpu_signals_count; i++) {
        sig_sems[i] = info->signal_gpu_signals[i]->semaphore;
    }

    // Gather fence
    VkFence fence = VK_NULL_HANDLE;
    if (info->cpu_signal) fence = info->cpu_signal->fence;

    // Submit

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount     = (uint32_t)info->wait_gpu_signals_count,
        .pWaitSemaphores        = wait_sems,
        .pWaitDstStageMask      = wait_stages,
        .commandBufferCount     = (uint32_t)info->command_lists_count,
        .pCommandBuffers        = cmd_bufs,
        .signalSemaphoreCount   = info->signal_gpu_signals_count,
        .pSignalSemaphores      = sig_sems
    };

    vkQueueSubmit(queue->handle, 1, &submit_info, fence);
}


/* ===== commands.c ===== */

// Generic

void dgx_cmd_sync_buffers(
    dgx_command_list*       target,

    dgx_buffer_sync_point   previous_use,
    dgx_buffer_sync_point   next_use,

    uint32_t                buffers_count,
    dgx_buffer**            buffers
) {
    VkPipelineStageFlags    srcStageMask;
    VkPipelineStageFlags    dstStageMask;
    VkAccessFlags           srcAccessMask;
    VkAccessFlags           dstAccessMask;

    dgx_translate_buffer_usage(
        previous_use,
        next_use,
        &srcStageMask,
        &dstStageMask,
        &srcAccessMask,
        &dstAccessMask
    );

    VkBufferMemoryBarrier barriers[buffers_count];

    for (uint32_t i = 0; i < buffers_count; i++) {
        barriers[i] = (VkBufferMemoryBarrier){
            .sType					= VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext					= NULL,
            .srcAccessMask			= srcAccessMask,
            .dstAccessMask			= dstAccessMask,
            .srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED,
            .buffer					= buffers[i]->buffer,
            .offset					= 0,
            .size					= VK_WHOLE_SIZE
        };
    }

    vkCmdPipelineBarrier(
        target->command_buffer,
        srcStageMask,
        dstStageMask,
        0,
        0, NULL,
        buffers_count, barriers,
        0, NULL
    );
}

void dgx_cmd_sync_textures(
    dgx_command_list*       target,

    dgx_texture_sync_point  previous_use,
    dgx_texture_sync_point  next_use,

    uint32_t                textures_count,
    dgx_texture**           textures
) {
    VkPipelineStageFlags    srcStage;
    VkPipelineStageFlags    dstStage;
    VkAccessFlags           srcAccess;
    VkAccessFlags           dstAccess;
    VkImageLayout           oldLayout;
    VkImageLayout           newLayout;

    dgx_translate_texture_usage(
        previous_use,
        next_use,
        &srcStage,
        &dstStage,
        &srcAccess,
        &dstAccess,
        &oldLayout,
        &newLayout
    );

    VkImageMemoryBarrier barriers[textures_count];

    for (uint32_t i = 0; i < textures_count; i++) {
        barriers[i] = (VkImageMemoryBarrier){
            .sType					= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext					= NULL,

            .srcAccessMask			= srcAccess,
            .dstAccessMask			= dstAccess,

            .oldLayout				= oldLayout,
            .newLayout				= newLayout,

            .srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED,

            .image					= textures[i]->image,

            .subresourceRange = {
                .aspectMask = (next_use == dgx_texture_sync_point_depth_attachment)
                    ? VK_IMAGE_ASPECT_DEPTH_BIT
                    : VK_IMAGE_ASPECT_COLOR_BIT,

                .baseMipLevel	= 0,
                .levelCount		= VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer	= 0,
                .layerCount		= VK_REMAINING_ARRAY_LAYERS
            }
        };
    }

    vkCmdPipelineBarrier(
        target->command_buffer,
        srcStage,
        dstStage,
        0,
        0, NULL,
        0, NULL,
        textures_count, barriers
    );
}

void dgx_cmd_copy_staging_memory_to_buffer(
    dgx_command_list*       target,
    dgx_staging_memory*     staging_memory,
    dgx_buffer*             target_buffer,
    uint32_t                staging_memory_region_offset,
    uint32_t                buffer_write_region_offset,
    uint32_t                buffer_write_region_size
) {
    VkBufferCopy copy_region = {
        .srcOffset  = staging_memory_region_offset,
        .size       = buffer_write_region_size,
        .dstOffset  = buffer_write_region_offset
    };

    vkCmdCopyBuffer(
        target->command_buffer, 
        staging_memory->buffer, 
        target_buffer->buffer, 
        1, &copy_region
    );
}

void dgx_cmd_copy_staging_memory_to_texture(
    dgx_command_list*       target,
    dgx_staging_memory*     staging_memory,
    dgx_texture*            target_texture,
    uint32_t                staging_memory_region_offset,
    dgx_texture_dimensions  texture_write_region_offset,
    dgx_texture_dimensions  texture_write_region_size
) {
    VkBufferImageCopy region = {
        .bufferOffset       = staging_memory_region_offset,
        .bufferRowLength    = 0,
        .bufferImageHeight  = 0,

        .imageSubresource.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT,
        .imageSubresource.mipLevel          = 0,
        .imageSubresource.baseArrayLayer    = 0,
        .imageSubresource.layerCount        = 1,

        .imageOffset = {
            texture_write_region_offset.x,
            texture_write_region_offset.y,
            texture_write_region_offset.z
        },

        .imageExtent = {
            texture_write_region_size.x,
            texture_write_region_size.y,
            texture_write_region_size.z
        }
    };

    vkCmdCopyBufferToImage(
        target->command_buffer,
        staging_memory->buffer,
        target_texture->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region
    );
}

// Graphics

void dgx_gcmd_begin_render_target_write(
    dgx_command_list* target,
    dgx_gcmd_begin_render_target_write_info* info
) {
    VkClearValue clear_values[info->clear_colors_count];

    for (uint32_t i = 0; i < info->clear_colors_count; ++i) {
        clear_values[i].color.float32[0] = info->clear_colors[i].r;
        clear_values[i].color.float32[1] = info->clear_colors[i].g;
        clear_values[i].color.float32[2] = info->clear_colors[i].b;
        clear_values[i].color.float32[3] = info->clear_colors[i].a;
    }

    VkRenderPassBeginInfo renderpass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass  = info->render_target->layout->renderpass,
        .framebuffer = info->render_target->framebuffer,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = {
                info->render_target->width,
                info->render_target->height
            }
        },
        .clearValueCount = (uint32_t)info->clear_colors_count,
        .pClearValues    = clear_values,
    };

    vkCmdBeginRenderPass(
        target->command_buffer,
        &renderpass_info,
        VK_SUBPASS_CONTENTS_INLINE
    );
}

void dgx_gcmd_end_render_target_write(
    dgx_command_list* target
) {
    vkCmdEndRenderPass(target->command_buffer);
}

void dgx_gcmd_bind_graphics_pipeline(
    dgx_command_list*   target,
    dgx_pipeline*       pipeline
) {
    vkCmdBindPipeline(
        target->command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->pipeline
    );
}

void dgx_gcmd_bind_graphics_pipeline_vertex_buffer(
    dgx_command_list*   target,
    dgx_buffer*         buffer,
    uint32_t            offset,
    uint32_t            binding
) {
    VkDeviceSize doffset = offset;
    vkCmdBindVertexBuffers(
        target->command_buffer, 
        binding, 1, 
        &buffer->buffer, 
        &doffset
    );
}

void dgx_gcmd_bind_graphics_pipeline_index_buffer(
    dgx_command_list*   target,
    dgx_buffer*         buffer,
    uint32_t            offset,
    int                 uint32_not_uint16
) {
    VkDeviceSize doffset = offset;
    vkCmdBindIndexBuffer(
        target->command_buffer, 
        buffer->buffer, 
        doffset,
        uint32_not_uint16 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16
    );
}

void dgx_gcmd_bind_graphics_pipeline_descriptors(
    dgx_command_list*                   target,
    dgx_pipeline_descriptors_layout*    layout,
    uint32_t                            first_descriptor_index,
    uint32_t                            descriptors_count,
    dgx_descriptor**                    descriptors
) {
    VkDescriptorSet* sets = tlom_alloc_only(sizeof(VkDescriptorSet) * descriptors_count);

    for (uint32_t i = 0; i < descriptors_count; i++) {
        sets[i] = descriptors[i]->dsc_set;
    }
    
    vkCmdBindDescriptorSets(
        target->command_buffer, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, 
        layout->layout, 
        first_descriptor_index, 
        descriptors_count, 
        sets, 
        0, 0
    );
}

void dgx_gcmd_draw_vertices(
    dgx_command_list*   target,

    uint32_t            vertices_count,
    uint32_t            vertices_buffer_offset_index,

    uint32_t            instances_count,
    uint32_t            instances_id_values_offset
) {
    vkCmdDraw(
        target->command_buffer,
        (uint32_t)vertices_count,
        (uint32_t)instances_count,
        (uint32_t)vertices_buffer_offset_index,
        (uint32_t)instances_id_values_offset
    );
}

void dgx_gcmd_draw_indexed(
    dgx_command_list*   target,

    uint32_t            indices_count,
    uint32_t            indicies_buffer_offset_index,
    int32_t             indicies_values_offset,

    uint32_t            instances_count,
    uint32_t            instances_id_values_offset
) {
    vkCmdDrawIndexed(
        target->command_buffer,
        (uint32_t)indices_count,
        (uint32_t)instances_count,
        (uint32_t)indicies_buffer_offset_index,
        indicies_values_offset,
        (uint32_t)instances_id_values_offset
    );
}

void dgx_gcmd_set_scissors(
    dgx_command_list* target,
    float root_x,  float root_y,
    float width,   float height
) {
    VkRect2D scissor = {
        .offset = {
            .x = (int32_t)root_x,
            .y = (int32_t)root_y
        },
        .extent = {
            .width  = (uint32_t)width,
            .height = (uint32_t)height
        }
    };

    vkCmdSetScissor(target->command_buffer, 0, 1, &scissor);
}

void dgx_gcmd_set_viewport(
    dgx_command_list* target,
    float root_x,  float root_y,
    float width,   float height
) {
    VkViewport viewport = {
        .x        = root_x,
        .y        = root_y,
        .width    = width,
        .height   = height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    vkCmdSetViewport(target->command_buffer, 0, 1, &viewport);
}


/* ===== common.c ===== */

// ===========================
// Config

// Instance Extensions

const char**    instance_extensions_array = NULL;
const uint32_t  instance_extensions_count = 0;

// Validation Layers

#ifdef DEMIGURG_GRAPHICS_VALIDATE
    static const char* validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
    const char**    config_validation_layers_array = &validation_layers[0];
    const uint32_t  config_validation_layers_count = 1;
#else 
    const char**    config_validation_layers_array = NULL;
    const uint32_t  config_validation_layers_count = 0;
#endif

// Device Extensions

static const char* required_device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const char**   config_required_device_extensions_array = &required_device_extensions[0];
const uint32_t config_required_device_extensions_count = 1;

// Pipelines State

static const VkDynamicState all_pipelines_dynamic_state[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
};

const VkDynamicState* config_all_pipelines_dynamic_state_array = &all_pipelines_dynamic_state[0];
const uint32_t config_all_pipelines_dynamic_state_count = 2;

// ===========================
// Thread Local Operational Memory

thread_local char tlom[tlom_size];

// ===========================
// Vulkan Methods

queues_family_info get_physical_device_queues_family_info(VkPhysicalDevice device, VkSurfaceKHR surface) {
    // Enumerate queue families
    uint32_t family_count = 0; vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, 0);

    // Pull families info
    VkQueueFamilyProperties* families = malloc(sizeof(VkQueueFamilyProperties) * family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, families);

    // Identify queue families, get their info
    queues_family_info info = {0};

    for (uint32_t i = 0; i < family_count; i++) {
        VkQueueFlags flags = families[i].queueFlags;
        int has_graphics = (flags & VK_QUEUE_GRAPHICS_BIT);
        int has_compute  = (flags & VK_QUEUE_COMPUTE_BIT);
        int has_transfer = (flags & VK_QUEUE_TRANSFER_BIT);

        if (info.presentation_queues_count == 0 && surface != VK_NULL_HANDLE) {
            VkBool32 presentation_support = 0;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentation_support);
            info.presentation_queues_family = i;
            info.presentation_queues_count = families[i].queueCount;
        }

        if (has_graphics && info.graphics_queues_count == 0) {
            info.graphics_queues_family = i;
            info.graphics_queues_count  = families[i].queueCount;
        } 
        else if (!has_graphics && has_transfer && !has_compute && info.transfer_queues_count == 0) {
            info.transfer_queues_family = i;
            info.transfer_queues_count  = families[i].queueCount;
        } 
        else if (!has_graphics && has_compute && !has_transfer && info.compute_queues_count == 0) {
            info.compute_queues_family = i;
            info.compute_queues_count  = families[i].queueCount;
        } 
        else if (!has_graphics && has_compute && has_transfer && info.transfer_compute_queues_count == 0) {
            info.transfer_compute_queues_family = i;
            info.transfer_compute_queues_count  = families[i].queueCount;
        }
    }

    // Free families info and return
    free(families);
    return info;
}

// remember to free details!
swapchain_support_details get_swapchain_support_details(VkPhysicalDevice device, VkSurfaceKHR surface) {
    swapchain_support_details details;

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


/* ===== descriptor_allocator.c ===== */

dgx_descriptor_allocator* dgx_create_descriptor_allocator(dgx_hardware* hardware, const dgx_descriptor_allocator_create_info* info) {
    dgx_descriptor_layout* layout = info->descriptor_layout;

    VkDescriptorPoolSize pool_sizes[4];
    uint32_t             pool_size_count = 0;

    if (layout->sampler_bindings > 0) {
        pool_sizes[pool_size_count++] = (VkDescriptorPoolSize){
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = layout->sampler_bindings * info->max_descriptors_allocated
        };
    }

    if (layout->sampled_textures_bindings > 0) {
        pool_sizes[pool_size_count++] = (VkDescriptorPoolSize){
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = layout->sampled_textures_bindings * info->max_descriptors_allocated
        };
    }

    if (layout->storage_buffers_bindings > 0) {
        pool_sizes[pool_size_count++] = (VkDescriptorPoolSize){
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = layout->storage_buffers_bindings * info->max_descriptors_allocated
        };
    }

    if (layout->uniform_buffers_bindings > 0) {
        pool_sizes[pool_size_count++] = (VkDescriptorPoolSize){
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = layout->uniform_buffers_bindings * info->max_descriptors_allocated
        };
    }

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets        = info->max_descriptors_allocated,
        .poolSizeCount  = pool_size_count,
        .pPoolSizes     = pool_sizes,
        .flags          = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
    };

    VkDescriptorPool vkpool; if (vkCreateDescriptorPool(hardware->logical_device, &pool_info, NULL, &vkpool) != VK_SUCCESS) {
        return 0x0; // Failed to create descriptor pool
    }

    dgx_descriptor* dgxpool = calloc(info->max_descriptors_allocated, sizeof(dgx_descriptor));
    if (!dgxpool) {
        vkDestroyDescriptorPool(hardware->logical_device, vkpool, 0);
        return 0x0;
    }

    dgx_descriptor_allocator* allocator = calloc(1, sizeof(*allocator));
    *allocator = (dgx_descriptor_allocator){
        .owning_hardware    = hardware,
        .max_descriptors    = info->max_descriptors_allocated,
        .target_layout      = info->descriptor_layout,
        .vk_pool            = vkpool,
        .dgx_pool           = dgxpool
    };

    return allocator;
}

void dgx_free_descriptor_allocator(dgx_descriptor_allocator* allocator) {
    if (!allocator) return;
    free(allocator->dgx_pool);
    vkDestroyDescriptorPool(allocator->owning_hardware->logical_device, allocator->vk_pool, NULL);
    free(allocator);
}

dgx_descriptor* dgx_descriptor_allocator_alloc_descriptor(dgx_descriptor_allocator* allocator) {
    VkDevice device = allocator->owning_hardware->logical_device;

    for (uint32_t i = 0; i < allocator->max_descriptors; i++) {
        if (allocator->dgx_pool[i].alive) continue;
        dgx_descriptor* desc = &allocator->dgx_pool[i];

        VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = allocator->vk_pool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &allocator->target_layout->dsc_set_layout,
        };

        VkDescriptorSet set; if (vkAllocateDescriptorSets(device, &alloc_info, &set) != VK_SUCCESS) {
            return 0x0; // failed to allocate descriptor sets!
        }

        *desc = (dgx_descriptor){
            .owning_allocator   = allocator,
            .dsc_set            = set,
            .alive              = 1,
        };

        return desc;
    }

    return 0x0;
}

void dgx_descriptor_allocator_free_descriptor(dgx_descriptor* descriptor) {
    descriptor->alive = 0;
    vkFreeDescriptorSets(
        descriptor->owning_allocator->owning_hardware->logical_device,
        descriptor->owning_allocator->vk_pool,
        1, &descriptor->dsc_set
    );
}


void dgx_descriptors_write(dgx_hardware* hardware, uint32_t writes_count, dgx_descriptor_write_info* write_infos) {
    uint32_t itr = 0; VkWriteDescriptorSet* vkwrites = tlom_alloc(&itr, writes_count * sizeof(VkWriteDescriptorSet));

    for (uint32_t i = 0; i < writes_count; i++) {
        dgx_descriptor_write_info* info = &write_infos[i];

        VkDescriptorBufferInfo* write_buffer_info = NULL;
        VkDescriptorImageInfo*  write_image_info  = NULL;

        switch (info->binding_type) {
        case dgx_descriptor_binding_type_uniform_buffer:
        case dgx_descriptor_binding_type_storage_buffer: {
            write_buffer_info = tlom_alloc(&itr, info->array_elements_count * sizeof(VkDescriptorBufferInfo));
            for (uint32_t j = 0; j < info->array_elements_count; j++) {
                const dgx_descriptor_buffer_write_info* dgx_elem_info = &info->infos.for_buffers[j];
                write_buffer_info[j] = (VkDescriptorBufferInfo){
                    .buffer = dgx_elem_info->buffer->buffer,
                    .offset = dgx_elem_info->offset,
                    .range  = dgx_elem_info->length
                };
            }
        } break;
        case dgx_descriptor_binding_type_sampled_texture: {
        write_image_info = tlom_alloc(&itr, info->array_elements_count * sizeof(VkDescriptorImageInfo));
            for (uint32_t j = 0; j < info->array_elements_count; j++) {
                const dgx_descriptor_sampled_texture_write_info* dgx_elem_info = &info->infos.for_sampled_textures[j];
                write_image_info[j] = (VkDescriptorImageInfo){
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .imageView = dgx_elem_info->sampled_texture->view
                };
            }
        } break;
        case dgx_descriptor_binding_type_sampler: {
            write_image_info = tlom_alloc(&itr, info->array_elements_count * sizeof(VkDescriptorImageInfo));
            for (uint32_t j = 0; j < info->array_elements_count; j++) {
                const dgx_descriptor_sampler_write_info* dgx_elem_info = &info->infos.for_samplers[j];
                write_image_info[j] = (VkDescriptorImageInfo){
                    .sampler = dgx_elem_info->sampler->sampler,
                };
            }
        } break;
        }

        vkwrites[i] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,

            .dstSet             = info->descriptor->dsc_set,
            .dstBinding         = info->binding_index,
            .dstArrayElement    = info->array_element_index,

            .descriptorType     = dgx_to_vk_descriptor_type(info->binding_type),
            .descriptorCount    = info->array_elements_count,

            .pBufferInfo        = write_buffer_info,
            .pImageInfo         = write_image_info,
            .pTexelBufferView   = NULL,
        };
    }

    vkUpdateDescriptorSets(
        hardware->logical_device,
        writes_count,
        vkwrites,
        0,
        NULL
    );
}

/* ===== descriptor_layout.c ===== */

dgx_descriptor_layout* dgx_create_descriptor_layout(dgx_hardware* hardware, const dgx_descriptor_layout_create_info* info) {
    if (!hardware || !info || !info->bindings || info->bindings_count == 0) return NULL;

    uint32_t samplers         = 0;
    uint32_t sampled_textures = 0;
    uint32_t storage_buffers  = 0;
    uint32_t uniform_buffers  = 0;

    VkDescriptorSetLayoutBinding* vk_bindings = tlom_alloc_only(sizeof(VkDescriptorSetLayoutBinding) * info->bindings_count);
    for (uint32_t i = 0; i < info->bindings_count; i++) {
        const dgx_descriptor_binding* src = &info->bindings[i];
        vk_bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding            = (uint32_t)src->binding,
            .descriptorType     = dgx_to_vk_descriptor_type(src->type),
            .descriptorCount    = (uint32_t)src->count,
            .stageFlags         = dgx_to_vk_shader_stage(src->stages),
            .pImmutableSamplers = NULL,
        };

        switch (src->type) {
        case dgx_descriptor_binding_type_sampler:           samplers++;         break;
        case dgx_descriptor_binding_type_sampled_texture:   sampled_textures++; break;
        case dgx_descriptor_binding_type_storage_buffer:    storage_buffers++;  break;
        case dgx_descriptor_binding_type_uniform_buffer:    uniform_buffers++;  break;
        }
    }

    VkDescriptorSetLayoutCreateInfo vk_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .flags          = 0,
        .bindingCount   = (uint32_t)info->bindings_count,
        .pBindings      = vk_bindings,
    };

    VkDescriptorSetLayout vklayout; 
    if (vkCreateDescriptorSetLayout(hardware->logical_device, &vk_info, 0, &vklayout) != VK_SUCCESS) return NULL;

    dgx_descriptor_layout* layout = calloc(1, sizeof(dgx_descriptor_layout));
    *layout = (dgx_descriptor_layout){
        .owning_hardware            = hardware,
        .dsc_set_layout             = vklayout,
        .sampler_bindings           = samplers,
        .sampled_textures_bindings  = sampled_textures,
        .storage_buffers_bindings   = storage_buffers,
        .uniform_buffers_bindings   = uniform_buffers
    };

    return layout;
}

void dgx_free_descriptor_layout(dgx_descriptor_layout* layout) {
    if (!layout) return;
    VkDevice device = layout->owning_hardware->logical_device;
    if (layout->dsc_set_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, layout->dsc_set_layout, NULL);
    free(layout);
}


/* ===== graphics_pipeline.c ===== */

// ===========================
// Graphics Pipeline

dgx_pipeline* dgx_create_pipeline(dgx_hardware* hardware, const dgx_pipeline_create_info* info) {
    // Dynamic State

    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = config_all_pipelines_dynamic_state_count,
        .pDynamicStates    = config_all_pipelines_dynamic_state_array
    };

    // Shader Stages

    uint32_t stages_count = 0;
    VkPipelineShaderStageCreateInfo stages[3];

    if (info->shader_stages.vertex) {
        stages[stages_count++] = (VkPipelineShaderStageCreateInfo){
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = info->shader_stages.vertex->module,
            .pName  = "main"
        };
    }

    if (info->shader_stages.geometry) {
        stages[stages_count++] = (VkPipelineShaderStageCreateInfo){
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_GEOMETRY_BIT,
            .module = info->shader_stages.geometry->module,
            .pName  = "main"
        };
    }

    if (info->shader_stages.pixel) {
        stages[stages_count++] = (VkPipelineShaderStageCreateInfo){
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = info->shader_stages.pixel->module,
            .pName  = "main"
        };
    }

    // Vertex Input

    uint32_t binding_count = 0;
    VkVertexInputBindingDescription bindings[16];

    for (uint32_t i = 0; i < info->vertex_layout.bindings_count; i++) {
        const dgx_vertex_input_binding_info* bind = &info->vertex_layout.bindings[i];

        bindings[binding_count++] = (VkVertexInputBindingDescription){
            .binding   = (uint32_t)bind->binding,
            .stride    = (uint32_t)bind->stride,
            .inputRate = dgx_to_vk_vertex_input_rate(bind->input_rate)
        };
    }

    uint32_t attribute_count = 0;
    VkVertexInputAttributeDescription attributes[16];

    for (uint32_t i = 0; i < info->vertex_layout.attributes_count; i++) {
        const dgx_vertex_input_attribute_info* attr = &info->vertex_layout.attributes[i];

        attributes[attribute_count++] = (VkVertexInputAttributeDescription){
            .location = (uint32_t)attr->location,
            .binding  = (uint32_t)attr->binding,
            .format   = dgx_to_vk_format(attr->type),
            .offset   = (uint32_t)attr->offset
        };
    }

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = binding_count,
        .pVertexBindingDescriptions      = bindings,
        .vertexAttributeDescriptionCount = attribute_count,
        .pVertexAttributeDescriptions    = attributes
    };

    // Input Assembly

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = dgx_to_vk_primitive_topology(info->input_assembly.topology),
        .primitiveRestartEnable = VK_FALSE
    };

    // Rasterizer

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = (info->rasterizer.fill_mode == dgx_fill_mode_wireframe)
            ? VK_POLYGON_MODE_LINE
            : VK_POLYGON_MODE_FILL,
        .cullMode = dgx_to_vk_cull_mode(info->rasterizer.cull_mode),
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
        .depthClampEnable = info->rasterizer.depth_clamp_enable,
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
        .depthTestEnable = info->depth_stencil.depth_test_enable,
        .depthWriteEnable = info->depth_stencil.depth_write_enable,
    };

    // Color Blend

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .blendEnable         = info->blend.blend_enable ? VK_TRUE : VK_FALSE,
        .srcColorBlendFactor = dgx_to_vk_blend_factor(info->blend.src_factor),
        .dstColorBlendFactor = dgx_to_vk_blend_factor(info->blend.dst_factor),
        .colorBlendOp        = dgx_to_vk_blend_op(info->blend.blend_op),
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment
    };

    // Graphics Pipeline

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,

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

        .layout                 = info->descriptor_layout->layout,

        .renderPass             = info->render_target_layout->renderpass,
        .subpass                = 0,

        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    VkPipeline vkpipeline; 
    if (vkCreateGraphicsPipelines(hardware->logical_device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &vkpipeline) != VK_SUCCESS) {
        return 0x0; // failed to create pipeline
    }

    // Assemble object
    
    dgx_pipeline* pipeline = calloc(1, sizeof(dgx_pipeline));
    *pipeline = (dgx_pipeline){
        .owning_hardware    = hardware,
        .pipeline           = vkpipeline
    };

    return pipeline;
}

void dgx_free_pipeline(dgx_pipeline* pipeline) {
    VkDevice device = pipeline->owning_hardware->logical_device;
    vkDestroyPipeline(device, pipeline->pipeline, NULL);
    free(pipeline);
}


/* ===== hardware.c ===== */

// ===========================
// Hardware - Device Part

static int physical_device_rate_type(VkPhysicalDeviceProperties* props, const dgx_hardware_create_info* info) {
    if (info->desired_hardware_type == dgx_hardware_type_discrete &&
        props->deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        return 1000;

    if (info->desired_hardware_type == dgx_hardware_type_integrated &&
        props->deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        return 1000;

    return 0;
}

static int physical_device_rate_availabile_queues_families(
    VkPhysicalDevice                device,
    const dgx_hardware_create_info* info,
    VkSurfaceKHR                    surface
) {
    int score = 0;

    queues_family_info queues_info = get_physical_device_queues_family_info(device, surface);

    if (info->desired_graphics_queues) {
        float graphics_precent = (float)queues_info.graphics_queues_count / info->desired_graphics_queues;
        score += min_u32(graphics_precent * 2000, 2000);
    }
    // required yet none
    else if (info->require_graphics_queues) {
        return -1;
    }

    if (info->desired_transfer_queues) {
        float transfer_precent = (float)queues_info.transfer_queues_count / info->desired_transfer_queues;
        score += min_u32(transfer_precent * 1000, 1000);
    }

    if (info->desired_compute_queues) {
        float compute_precent = (float)queues_info.compute_queues_count / info->desired_compute_queues;
        score += min_u32(compute_precent * 1000, 1000);
    }

    if (info->desired_transfer_compute_queues) {
        float trs_cmp_precent = (float)queues_info.transfer_compute_queues_count / info->desired_transfer_compute_queues;
        score += min_u32(trs_cmp_precent * 1000, 1000);
    }

    // check presentation support
    if (info->require_presentation_queue) {
        if (queues_info.presentation_queues_count == 0) return -1; // no presentation queue
    }

    return score;
}

static int physical_device_rate_extensions_availability(VkPhysicalDevice device) {
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

    if (unfound_required_extensions == 0) return 0;
    return -1;
}

// perform after extension check!
static int physical_device_rate_swapchain_adequality(VkPhysicalDevice device, VkSurfaceKHR surface) {
    swapchain_support_details details = get_swapchain_support_details(device, surface);

    if (details.formats_count == 0) {
        free_swapchain_support_details(details);
        return -1;
    }

    if (details.present_modes_count == 0) {
        free_swapchain_support_details(details);
        return -1;
    }

    free_swapchain_support_details(details);
    return 0;
}

static const VkPhysicalDeviceDescriptorIndexingFeatures required_indexing_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,

    .runtimeDescriptorArray                         = VK_TRUE,
    .descriptorBindingVariableDescriptorCount       = VK_TRUE,
    .descriptorBindingPartiallyBound                = VK_TRUE,

    .descriptorBindingUniformBufferUpdateAfterBind  = VK_TRUE,
    .descriptorBindingSampledImageUpdateAfterBind   = VK_TRUE,
    .descriptorBindingStorageBufferUpdateAfterBind  = VK_TRUE,
    .descriptorBindingStorageImageUpdateAfterBind   = VK_TRUE
};

// Determine in abstract points, how does the device align with requirements
// Returns -1 if devide does not met requirements
static int physical_device_score(
    VkPhysicalDevice                device, 
    const dgx_hardware_create_info* info,
    VkSurfaceKHR                    surface
) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    int score = 0;
    int gained;

    gained = physical_device_rate_type(&props, info);
    if (gained == -1) return -1;
    score += gained;

    gained = physical_device_rate_availabile_queues_families(device, info, surface);
    if (gained == -1) return -1;
    score += gained;

    gained = physical_device_rate_extensions_availability(device);
    if (gained == -1) return -1;
    score += gained;

    gained = physical_device_rate_swapchain_adequality(device, surface);
    if (gained == -1) return -1;
    score += gained;
    
    // large max image dimension signals a more capable device
    score += props.limits.maxImageDimension2D / 1024;

    return score;
}

static dgx_hardware_queue* load_queues(
    VkDevice device,
    uint32_t family_index,
    uint32_t count
) {
    if (count == 0) return NULL;

    dgx_hardware_queue* queues = malloc(count * sizeof(dgx_hardware_queue));
    for (uint32_t i = 0; i < count; i++) {
        vkGetDeviceQueue(device, family_index, (uint32_t)i, &queues[i].handle);
        queues[i].family_index = family_index;
        queues[i].queue_index  = (uint32_t)i;
    }
    return queues;
}

// hardware->physical_device
int hardware_pick_physical_device(dgx_hardware* hardware, const dgx_hardware_create_info* info, VkSurfaceKHR surface) {
    // Pull device info count
    uint32_t device_count = 0; vkEnumeratePhysicalDevices(hardware->owning_library->instance, &device_count, 0);
    if (device_count == 0) return 0x0;

    // Pull device info
    VkPhysicalDevice* array = malloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(hardware->owning_library->instance, &device_count, array);

    // Find best device
    int              score  = 0;
    VkPhysicalDevice handle = VK_NULL_HANDLE;

    for (uint32_t i = 0; i < device_count; i++) {
        int this_score = physical_device_score(array[i], info, surface);
        if (this_score > score) {
            score  = this_score;
            handle = array[i];
        }
    }

    // Free temporary memory 
    free(array);

    // No suitable device found
    if (handle == VK_NULL_HANDLE) {
        return 0;
    }

    hardware->physical_device = handle;
    vkGetPhysicalDeviceProperties(hardware->physical_device, &hardware->physical_device_properties);

    return 1;
}

int hardware_create_device_and_queues(dgx_hardware* hardware, const dgx_hardware_create_info* info, VkSurfaceKHR surface) {
    // Get queues info
    queues_family_info qf_info = get_physical_device_queues_family_info(hardware->physical_device, surface);

    // Create hardware

    hardware->graphics_queues_count = min_u32(qf_info.graphics_queues_count, info->desired_graphics_queues);
    hardware->transfer_queues_count = min_u32(qf_info.transfer_queues_count, info->desired_transfer_queues);
    hardware->compute_queues_count  = min_u32(qf_info.compute_queues_count,  info->desired_compute_queues);
    hardware->trs_cmp_queues_count  = min_u32(qf_info.transfer_compute_queues_count, info->desired_transfer_compute_queues);

    // Queues Create Info

    uint32_t                queues_create_info_itr = 0;
    VkDeviceQueueCreateInfo queues_create_infos[5];
    
    // assing all queues equal priority
    uint32_t max_queues = max_u32(
        max_u32(
            max_u32(hardware->graphics_queues_count, hardware->transfer_queues_count), 
            max_u32(hardware->compute_queues_count, hardware->trs_cmp_queues_count)
        ),
        info->require_presentation_queue && qf_info.graphics_queues_family != qf_info.presentation_queues_family ? 1 : 0
    );
    float* queues_priorities = malloc(max_queues * sizeof(float));
    for (uint32_t i = 0; i < max_queues; i++) queues_priorities[i] = 1.0f;

    if (hardware->graphics_queues_count) queues_create_infos[queues_create_info_itr++] = (VkDeviceQueueCreateInfo){
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = qf_info.graphics_queues_family,
        .queueCount       = hardware->graphics_queues_count,
        .pQueuePriorities = queues_priorities,
    };

    if (hardware->transfer_queues_count) queues_create_infos[queues_create_info_itr++] = (VkDeviceQueueCreateInfo){
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = qf_info.transfer_queues_family,
        .queueCount       = hardware->transfer_queues_count,
        .pQueuePriorities = queues_priorities,
    };

    if (hardware->compute_queues_count) queues_create_infos[queues_create_info_itr++] = (VkDeviceQueueCreateInfo){
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = qf_info.compute_queues_family,
        .queueCount       = hardware->compute_queues_count,
        .pQueuePriorities = queues_priorities,
    };

    if (hardware->trs_cmp_queues_count) queues_create_infos[queues_create_info_itr++] = (VkDeviceQueueCreateInfo){
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = qf_info.transfer_compute_queues_family,
        .queueCount       = hardware->trs_cmp_queues_count,
        .pQueuePriorities = queues_priorities,
    };

    // if presentation required and does not overlap with graphics
    if (info->require_presentation_queue && qf_info.graphics_queues_family != qf_info.presentation_queues_family) 
    queues_create_infos[queues_create_info_itr++] = (VkDeviceQueueCreateInfo){
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = qf_info.presentation_queues_family,
        .queueCount       = 1,
        .pQueuePriorities = queues_priorities,
    };

    // Device Creation
    // Enable required extensions and features

    VkDeviceCreateInfo device_create_info = {
        .sType                      = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount       = queues_create_info_itr,
        .pQueueCreateInfos          = queues_create_infos,
        .enabledExtensionCount      = config_required_device_extensions_count,
        .ppEnabledExtensionNames    = config_required_device_extensions_array,
    };

    VkDevice device;
    if (vkCreateDevice(hardware->physical_device, &device_create_info, 0, &device) != VK_SUCCESS) {
        return 0;   // failed to create logical device
    }
    hardware->logical_device = device;

    // Free temporary memory
    free(queues_priorities);

    // Finish hardware

    hardware->graphics_queues = load_queues(device, qf_info.graphics_queues_family,          hardware->graphics_queues_count);
    hardware->transfer_queues = load_queues(device, qf_info.transfer_queues_family,          hardware->transfer_queues_count);
    hardware->compute_queues  = load_queues(device, qf_info.compute_queues_family,           hardware->compute_queues_count);
    hardware->trs_cmp_queues  = load_queues(device, qf_info.transfer_compute_queues_family,  hardware->trs_cmp_queues_count);
    if (info->require_presentation_queue) hardware->presentation_queue = load_queues(device, qf_info.presentation_queues_family, 1);

    return 1;
}

int hardware_create_memory_allocator_state(dgx_hardware*);

dgx_hardware* dgx_create_hardware(dgx_library* library, const dgx_hardware_create_info* info) {
    dgx_hardware* hardware = calloc(1, sizeof(dgx_hardware));
    hardware->owning_library = library;

    // Create temporary window and surface to test presentation availability
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* test_window = glfwCreateWindow(100, 100, "Vulkan test window", NULL, NULL);
    if (!test_window) goto _fail;

    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(hardware->owning_library->instance, test_window, 0, &surface) != VK_SUCCESS) {
        glfwDestroyWindow(test_window); goto _fail; // failed to create surface
    }

    // Construct hardware
    if (!hardware_pick_physical_device(hardware, info, surface)) goto _fail;
    if (!hardware_create_device_and_queues(hardware, info, surface)) goto _fail;

    // Free temporary objects
    vkDestroySurfaceKHR(hardware->owning_library->instance, surface, 0); 
    glfwDestroyWindow(test_window);

    // Init hardware allocator
    if (!hardware_create_memory_allocator_state(hardware)) goto _fail;

    // Return
    return hardware;

_fail:
    vkDestroySurfaceKHR(hardware->owning_library->instance, surface, 0); glfwDestroyWindow(test_window);
    dgx_free_hardware(hardware); return 0x0;
}

void hardware_free_memory_allocator_state(dgx_hardware*);

void dgx_free_hardware(dgx_hardware* hardware) {
    if (!hardware) return;
    hardware_free_memory_allocator_state(hardware);
    vkDestroyDevice(hardware->logical_device, 0);
    free(hardware->presentation_queue);
    free(hardware->graphics_queues);
    free(hardware->transfer_queues);
    free(hardware->compute_queues);
    free(hardware->trs_cmp_queues);
    free(hardware);
}

uint32_t dgx_hardware_query_queues_count(dgx_hardware* hardware, dgx_hardware_queue_type type) {
    if (!hardware) return 0;

    switch (type) {
    case dgx_hardware_queue_type_graphics:          return hardware->graphics_queues_count;
    case dgx_hardware_queue_type_transfer:          return hardware->transfer_queues_count;
    case dgx_hardware_queue_type_compute:           return hardware->compute_queues_count;
    case dgx_hardware_queue_type_transfer_compute:  return hardware->trs_cmp_queues_count;
    default: return 0;
    }
}

void dgx_hardware_query_queues
(dgx_hardware* hardware, dgx_hardware_queue_type type, uint32_t queues_offset, uint32_t queues_count, dgx_hardware_queue** queues) {
    if (!hardware || !queues) return;

    dgx_hardware_queue* src = NULL;
    switch (type) {
    case dgx_hardware_queue_type_graphics:          src = hardware->graphics_queues;   break;
    case dgx_hardware_queue_type_transfer:          src = hardware->transfer_queues;   break;
    case dgx_hardware_queue_type_compute:           src = hardware->compute_queues;    break;
    case dgx_hardware_queue_type_transfer_compute:  src = hardware->trs_cmp_queues;    break;
    }

    for (uint32_t i = queues_offset; i < queues_offset + queues_count; i++) queues[i] = &src[i];
}

uint64_t dgx_hardware_query_limit(dgx_hardware* hardware, dgx_hardware_limit limit) {
    VkPhysicalDeviceLimits* l = &hardware->physical_device_properties.limits;

    switch (limit) {
        // Textures

        case dgx_hardware_limit_max_texture_dimension_1d:   return l->maxImageDimension1D;
        case dgx_hardware_limit_max_texture_dimension_2d:   return l->maxImageDimension2D;
        case dgx_hardware_limit_max_texture_dimension_3d:   return l->maxImageDimension3D;
        case dgx_hardware_limit_max_texture_dimension_cube: return l->maxImageDimensionCube;
        case dgx_hardware_limit_max_texture_array_layers:   return l->maxImageArrayLayers;

        // Descriptors (pipeline-level)

        case dgx_hardware_limit_max_descriptor_per_pipeline:    return l->maxBoundDescriptorSets;

        // Per-stage descriptor limits

        case dgx_hardware_limit_max_descriptor_uniform_buffers_per_stage:   return l->maxPerStageDescriptorUniformBuffers;
        case dgx_hardware_limit_max_descriptor_storage_buffers_per_stage:   return l->maxPerStageDescriptorStorageBuffers;
        case dgx_hardware_limit_max_descriptor_sampled_images_per_stage:    return l->maxPerStageDescriptorSampledImages;
        case dgx_hardware_limit_max_descriptor_samplers_per_stage:          return l->maxPerStageDescriptorSamplers;

        // Descriptor set layout limits

        case dgx_hardware_limit_max_descriptor_uniform_buffers: return l->maxDescriptorSetUniformBuffers;
        case dgx_hardware_limit_max_descriptor_storage_buffers: return l->maxDescriptorSetStorageBuffers;
        case dgx_hardware_limit_max_descriptor_sampled_images:  return l->maxDescriptorSetSampledImages;
        case dgx_hardware_limit_max_descriptor_samplers:        return l->maxDescriptorSetSamplers;

        // Buffer range limits

        case dgx_hardware_limit_max_descriptor_bound_uniform_buffer_length: return l->maxUniformBufferRange;
        case dgx_hardware_limit_max_descriptor_bound_storage_buffer_length: return l->maxStorageBufferRange;

        // Vertex input

        case dgx_hardware_limit_max_vertex_input_attributes:        return l->maxVertexInputAttributes;
        case dgx_hardware_limit_max_vertex_input_bindings:          return l->maxVertexInputBindings;
        case dgx_hardware_limit_max_vertex_input_attribute_offset:  return l->maxVertexInputAttributeOffset;
    }

    return 0;
}

void dgx_hardware_wait_idle(dgx_hardware* hardware) {
    vkDeviceWaitIdle(hardware->logical_device);
}

// ===========================
// Hardware - Memory Allocator Part

struct hardware_paged_memory {
    uint32_t                memory_size_bytes;  // 0 means memory not allocated
    VkDeviceMemory          vk_device_memory;   // the page memory
    hardware_paged_memory*  next_page;          // next page of same memory type
};

int hardware_create_memory_allocator_state(dgx_hardware* hardware) {
    vkGetPhysicalDeviceMemoryProperties(hardware->physical_device, &hardware->memory_properties);

    uint32_t types_count = hardware->memory_properties.memoryTypeCount;
    hardware->paged_per_type = calloc(types_count, sizeof(hardware_paged_memory*));

    return hardware->paged_per_type != 0x0;
}

void hardware_free_memory_allocator_state(dgx_hardware* hardware) {
    for (uint32_t memory_type = 0; memory_type < hardware->memory_properties.memoryTypeCount; memory_type++) {
        hardware_paged_memory* page = hardware->paged_per_type[memory_type];

        while (page) {
            if (page->memory_size_bytes != 0) vkFreeMemory(hardware->logical_device, page->vk_device_memory, 0);
            hardware_paged_memory* next_page = page->next_page;
            free(page); page = next_page;
        }
    }

    free(hardware->paged_per_type);
}

int try_alloc_dedicated(
    dgx_hardware*           hardware,
    uint32_t                memory_type_index,
    uint32_t                size,
    VkDeviceMemory*         result_memory,
    uint32_t*               result_offset
) {
    VkMemoryAllocateInfo alloc_info = {
        .sType              = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize     = size,
        .memoryTypeIndex    = memory_type_index
    };

    VkDeviceMemory vkmemory; if (vkAllocateMemory(hardware->logical_device, &alloc_info, 0, &vkmemory) != VK_SUCCESS) return 0;

    *result_memory = vkmemory;
    *result_offset = 0;

    return 1;
}

int try_alloc_paged(
    hardware_paged_memory*  page,
    uint32_t                size,
    VkDeviceMemory*         result_memory,
    uint32_t*               result_offset
) {
    // supress unused parameter warnings for now
    (void)(page); (void)(size);
    (void)(result_memory); (void)(result_offset);
    return 0;
}

// 1 at success
int try_allocate_hardware_memory(
    dgx_hardware*                   hardware,
    dgx_memory_allocation_strategy  strategy,
    VkMemoryRequirements            memory_requirements,
    VkDeviceMemory*                 result_memory,
    uint32_t*                       result_offset
) {
    uint32_t memory_size  = memory_requirements.size;
    uint32_t search_start = 0;

    // try all memory types, 
    // starting from optimal one
    do {
        uint32_t memory_type = find_memory_type(
            &hardware->memory_properties, 
            memory_requirements.memoryTypeBits, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            search_start
        );

        // failure, no memory type
        if (memory_type == UINT32_MAX) return 0;

        switch (strategy) {
        case dgx_memory_allocation_strategy_paged:
            if (try_alloc_paged(
                hardware->paged_per_type[memory_type], 
                memory_size, 
                result_memory, 
                result_offset
            )) return 1;
            // intended fallback to dedicated allocation in case paged failed
            __attribute__ ((fallthrough));
        case dgx_memory_allocation_strategy_dedicated:
            if (try_alloc_dedicated(
                hardware,
                memory_type,
                memory_size,
                result_memory, 
                result_offset
            )) return 1;
        }
    } while(1);

    return 0;
}


/* ===== library.c ===== */

// ===========================
// Platform Context

void platform_init() {
    glfwInit();
}

void platform_terminate() {
    glfwTerminate();
}

void get_platform_required_extensions(uint32_t* count, const char*** array) {
    *array = glfwGetRequiredInstanceExtensions(count);
}

// ===========================
// Library

dgx_library* dgx_create_library(const dgx_library_create_info* info) {
    VkApplicationInfo appInfo = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "tgw app",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "No Engine",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo create_info = {
        .sType                      = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo           = &appInfo,
        .enabledLayerCount          = config_validation_layers_count,
        .ppEnabledLayerNames        = config_validation_layers_array,
        .enabledExtensionCount      = instance_extensions_count,
        .ppEnabledExtensionNames    = instance_extensions_array
    };

    if (info->platform_code_enabled) {
        glfwInit();

        uint32_t     extension_count;
        const char** extension_names;
        
        get_platform_required_extensions(&extension_count, &extension_names);

        create_info.enabledExtensionCount   = extension_count;
        create_info.ppEnabledExtensionNames = extension_names;
    }

    VkInstance instance;
    if (vkCreateInstance(&create_info, 0, &instance) != VK_SUCCESS) return 0x0;

    dgx_library* library = calloc(1, sizeof(dgx_library));
    *library = (dgx_library){
        .instance           = instance,
        .platform_enabled   = info->platform_code_enabled
    };

    return library;
}

void dgx_free_library(dgx_library* library) {
    vkDestroyInstance(library->instance, 0);

    if (library->platform_enabled) {
        platform_terminate();
    }
    
    free(library);
}


/* ===== pipeline_descriptors_layout.c ===== */

dgx_pipeline_descriptors_layout* dgx_create_pipeline_descriptors_layout(
    dgx_hardware* hardware, const dgx_pipeline_descriptors_layout_create_info* info
) {
    VkDescriptorSetLayout* desc_sets_layouts = tlom_alloc_only(info->layouts_count * sizeof(VkDescriptorSetLayout));

    for (uint32_t i = 0; i < info->layouts_count; i++) {
        desc_sets_layouts[i] = info->layouts[i]->dsc_set_layout;
    }

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = info->layouts_count,
        .pSetLayouts    = desc_sets_layouts
    };

    VkPipelineLayout vklayout; if (vkCreatePipelineLayout(hardware->logical_device, &layout_info, NULL, &vklayout) != VK_SUCCESS) {
        return 0x0; // failed to create pipeline layout
    }

    dgx_pipeline_descriptors_layout* pd_layout = calloc(1, sizeof(dgx_pipeline_descriptors_layout));
    *pd_layout = (dgx_pipeline_descriptors_layout){
        .owning_hardware    = hardware,
        .layout             = vklayout,
    };

    return pd_layout;
}

void dgx_free_pipeline_descriptors_layout(dgx_pipeline_descriptors_layout* layout) {
    if (!layout) return;
    vkDestroyPipelineLayout(layout->owning_hardware->logical_device, layout->layout, 0);
    free(layout);
}


/* ===== render_target.c ===== */

dgx_render_target* dgx_create_render_target(
    dgx_hardware*                           hardware,
    const dgx_render_target_create_info*    info
) {
    assert(hardware); assert(info);

    uint32_t width, height;

    // Prepare attachments array for Vulkan
    uint32_t total_attachments_count = info->color_attachments_count + (info->depth_stencil_attachment ? 1 : 0);
    VkImageView* attachments = tlom_alloc_only(sizeof(VkImageView) * total_attachments_count);

    for (uint32_t i = 0; i < info->color_attachments_count; i++) {
        attachments[i] = info->color_attachments[i].texture->view;
        width  = info->color_attachments[i].texture->dimensions.x;
        height = info->color_attachments[i].texture->dimensions.y;
    }
    if (info->depth_stencil_attachment) {
        attachments[info->color_attachments_count] = info->depth_stencil_attachment->texture->view;
        width  = info->depth_stencil_attachment->texture->dimensions.x;
        height = info->depth_stencil_attachment->texture->dimensions.y;
    }

    // Create Framebuffer

    VkFramebufferCreateInfo fb_info = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext           = NULL,
        .flags           = 0,
        .renderPass      = info->render_target_layout->renderpass,
        .attachmentCount = (uint32_t)total_attachments_count,
        .pAttachments    = attachments,
        .width           = width,
        .height          = height,
        .layers          = 1,
    };

    VkFramebuffer framebuffer;
    if (vkCreateFramebuffer(hardware->logical_device, &fb_info, NULL, &framebuffer) != VK_SUCCESS) {
        assert(0 && "Failed to create framebuffer!");
    };

    // Assemble render target

    dgx_render_target* render_target = calloc(1, sizeof(dgx_render_target));
    *render_target = (dgx_render_target){
        .owning_hardware    = hardware,
        .layout             = info->render_target_layout,
        .framebuffer        = framebuffer,
        .width              = width,
        .height             = height
    };
    
    return render_target;
}

void dgx_free_render_target(dgx_render_target* render_target) {
    if (!render_target) return;
    vkDestroyFramebuffer(render_target->owning_hardware->logical_device, render_target->framebuffer, NULL);
    free(render_target);
}


/* ===== render_target_layout.c ===== */

dgx_render_target_layout* dgx_create_render_target_layout(
    dgx_hardware*                               hardware,
    const dgx_render_target_layout_create_info* info
) {
    assert(hardware && info);
    uint32_t tlom_itr = 0;

    // Prepare attachments

    uint32_t color_attachment_count = info->color_attachments_count;
    uint32_t total_attachments = color_attachment_count + (info->depth_stencil_attachment ? 1 : 0);
    VkAttachmentDescription* attachments = tlom_alloc(&tlom_itr, sizeof(VkAttachmentDescription) * total_attachments);

    // Color attachments

    for (uint32_t i = 0; i < color_attachment_count; i++) {
        dgx_render_target_layout_attachment* a = &info->color_attachments[i];
        attachments[i] = (VkAttachmentDescription){
            .flags          = 0,
            .format         = dgx_to_vk_texture_format(a->format),
            .samples        = (VkSampleCountFlagBits)a->sample_count,
            .loadOp         = dgx_to_vk_load_op(a->load_op),
            .storeOp        = dgx_to_vk_store_op(a->store_op),
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };
    }

    // Depth/stencil attachment

    if (info->depth_stencil_attachment) {
        dgx_render_target_layout_attachment* a = info->depth_stencil_attachment;
        attachments[color_attachment_count] = (VkAttachmentDescription){
            .flags          = 0,
            .format         = dgx_to_vk_texture_format(a->format),
            .samples        = (VkSampleCountFlagBits)a->sample_count,
            .loadOp         = dgx_to_vk_load_op(a->load_op),
            .storeOp        = dgx_to_vk_store_op(a->store_op),
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };
    }

    // References

    VkAttachmentReference* color_refs = tlom_alloc(&tlom_itr, sizeof(VkAttachmentReference) * color_attachment_count);
    if (!color_refs) return NULL;

    for (uint32_t i = 0; i < color_attachment_count; i++) {
        color_refs[i] = (VkAttachmentReference){
            .attachment = i,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };
    }

    VkAttachmentReference depth_ref = { 0 };
    if (info->depth_stencil_attachment) {
        depth_ref = (VkAttachmentReference){
            .attachment = color_attachment_count,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };
    }

    // Subpass

    VkSubpassDescription subpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = color_attachment_count,
        .pColorAttachments       = color_refs,
        .pDepthStencilAttachment = info->depth_stencil_attachment ? &depth_ref : NULL
    };

    // Render pass
    
    VkRenderPassCreateInfo rp_info = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = total_attachments,
        .pAttachments    = attachments,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 0,
        .pDependencies   = NULL
    };

    VkRenderPass renderpass;
    if (vkCreateRenderPass(hardware->logical_device, &rp_info, NULL, &renderpass) != VK_SUCCESS) return NULL;

    // Assemble layout
    dgx_render_target_layout* layout = calloc(1, sizeof(dgx_render_target_layout));
    *layout = (dgx_render_target_layout){
        .owning_hardware = hardware,
        .renderpass      = renderpass
    };

    return layout;
}

void dgx_free_render_target_layout(dgx_render_target_layout* layout) {
    if (!layout) return;
    vkDestroyRenderPass(layout->owning_hardware->logical_device, layout->renderpass, NULL);
    free(layout);
}


/* ===== sampler.c ===== */

dgx_sampler* dgx_create_sampler(dgx_hardware* hardware, const dgx_sampler_create_info* info) {
    VkSamplerCreateInfo create_info = {
        .sType  = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        
        .magFilter               = dgx_to_vk_filter(info->mag_filter),
        .minFilter               = dgx_to_vk_filter(info->min_filter),
        .mipmapMode              = dgx_to_vk_mipmap_mode(info->mipmap_filter),

        .addressModeU            = dgx_to_vk_sampler_wrapping(info->x_coord_wrapping),
        .addressModeV            = dgx_to_vk_sampler_wrapping(info->y_coord_wrapping),
        .addressModeW            = dgx_to_vk_sampler_wrapping(info->z_coord_wrapping),

        .mipLodBias              = info->mip_lod_bias,

        // todo!
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = hardware->physical_device_properties.limits.maxSamplerAnisotropy,

        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,

        .minLod                  = info->min_lod,
        .maxLod                  = info->max_lod,

        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = info->unnormalized_coordinates ? VK_TRUE : VK_FALSE
    };

    VkSampler vksampler; if (vkCreateSampler(hardware->logical_device, &create_info, 0, &vksampler) != VK_SUCCESS) {
        return 0x0; // failed to create sampler
    }

    dgx_sampler* sampler = calloc(1, sizeof(dgx_sampler));
    *sampler = (dgx_sampler){
        .owning_hardware    = hardware,
        .sampler            = vksampler
    };

    return sampler;
}

void dgx_free_sampler(dgx_sampler* sampler) {
    if (!sampler) return;
    vkDestroySampler(sampler->owning_hardware->logical_device, sampler->sampler, 0);
    free(sampler);
}


/* ===== shader.c ===== */

dgx_shader* dgx_create_shader(dgx_hardware* hardware, const dgx_shader_create_info* info) {
    VkShaderModuleCreateInfo shader_module_create_info = {
        .sType      = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize   = info->source_size,
        .pCode      = (uint32_t*)info->source_code
    };

    VkShaderModule shader_module;
    if (vkCreateShaderModule(hardware->logical_device, &shader_module_create_info, 0, &shader_module) != VK_SUCCESS) {
        return 0x0; // failed to create shader module
    }

    dgx_shader* shader = calloc(1, sizeof(dgx_shader));
    *shader = (dgx_shader){
        .owning_hardware = hardware,
        .module          = shader_module
    };

    return shader;
}

void dgx_free_shader(dgx_shader* shader) {
    if (shader == 0x0) return;
    vkDestroyShaderModule(shader->owning_hardware->logical_device, shader->module, 0);
    free(shader);
}


/* ===== signal.c ===== */

// Cpu Signal

dgx_cpu_signal* dgx_create_cpu_signal(dgx_hardware* hardware, const dgx_cpu_signal_create_info* info) {
    VkFenceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = (info->initialy_signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0)
    };

    VkFence fence;
    if (vkCreateFence(hardware->logical_device, &create_info, NULL, &fence) != VK_SUCCESS) {
        return 0x0; // failed to create fence
    }

    dgx_cpu_signal* signal = calloc(1, sizeof(dgx_cpu_signal));
    *signal = (dgx_cpu_signal){
        .owning_hardware = hardware,
        .fence           = fence
    };

    return signal;
}

void dgx_free_cpu_signal(dgx_cpu_signal* signal) {
    if (!signal) return;
    vkDestroyFence(signal->owning_hardware->logical_device, signal->fence, NULL);
    free(signal);
}

int dgx_cpu_signal_signaled(dgx_cpu_signal* signal) {
    VkResult res = vkGetFenceStatus(
        signal->owning_hardware->logical_device,
        signal->fence
    );

    return res == VK_SUCCESS;
}

void dgx_cpu_signal_wait(dgx_cpu_signal* signal) {
    vkWaitForFences(signal->owning_hardware->logical_device, 1, &signal->fence, VK_TRUE,UINT64_MAX);
}

void dgx_cpu_signal_reset(dgx_cpu_signal* signal) {
    vkResetFences(signal->owning_hardware->logical_device, 1, &signal->fence);
}

// Gpu Signal

dgx_gpu_signal* dgx_create_gpu_signal(dgx_hardware* hardware) {
    VkSemaphoreCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkSemaphore semaphore;
    if (vkCreateSemaphore(hardware->logical_device, &create_info, NULL, &semaphore) != VK_SUCCESS) {
        return 0x0; // failed to create semaphore
    }

    dgx_gpu_signal* signal = calloc(1, sizeof(dgx_gpu_signal));
    *signal = (dgx_gpu_signal){
        .owning_hardware = hardware,
        .semaphore       = semaphore
    };

    return signal;
}

void dgx_free_gpu_signal(dgx_gpu_signal* signal) {
    if (!signal) return;
    vkDestroySemaphore(signal->owning_hardware->logical_device, signal->semaphore, NULL);
    free(signal);
}


/* ===== staging_memory.c ===== */

dgx_staging_memory* dgx_create_staging_memory(dgx_hardware* hardware, const dgx_staging_memory_create_info* info) {
    VkBufferCreateInfo buffer_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = info->size_bytes,
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer vkbuffer; if (vkCreateBuffer(hardware->logical_device, &buffer_info, 0, &vkbuffer) != VK_SUCCESS) {
        return 0x0; // failed to create buffer
    }

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(hardware->logical_device, vkbuffer, &memory_requirements);

    uint32_t memory_type = find_memory_type(
        &hardware->memory_properties,
        memory_requirements.memoryTypeBits, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        0
    );

    if (memory_type == UINT32_MAX) {
        vkDestroyBuffer(hardware->logical_device, vkbuffer, 0);
        return 0x0; // failed to find suitable memory type
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType              = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize     = memory_requirements.size,
        .memoryTypeIndex    = memory_type
    };

    VkDeviceMemory vkmemory; if (vkAllocateMemory(hardware->logical_device, &alloc_info, 0, &vkmemory) != VK_SUCCESS) {
        vkDestroyBuffer(hardware->logical_device, vkbuffer, 0);
        return 0x0; // failed to allocate memory
    }

    vkBindBufferMemory(hardware->logical_device, vkbuffer, vkmemory, 0);

    dgx_staging_memory* memory = calloc(1, sizeof(dgx_staging_memory));
    *memory = (dgx_staging_memory){
        .owning_hardware    = hardware,
        .buffer             = vkbuffer,
        .memory             = vkmemory
    };

    return memory;
}

void dgx_free_staging_memory(dgx_staging_memory* memory) {
    if (!memory) return;
    vkDestroyBuffer(memory->owning_hardware->logical_device, memory->buffer, 0);
    vkFreeMemory(memory->owning_hardware->logical_device, memory->memory, 0);
    free(memory);
}

void* dgx_staging_memory_map(dgx_staging_memory* memory, uint64_t region_offset, uint64_t region_size) {
    void* data;
    if (vkMapMemory(
        memory->owning_hardware->logical_device, 
        memory->memory, region_offset, region_size, 0, &data
    ) != VK_SUCCESS) return 0x0;
    return data;
}

void dgx_staging_memory_unmap(dgx_staging_memory* memory) {
    vkUnmapMemory(memory->owning_hardware->logical_device, memory->memory);
}


/* ===== texture.c ===== */

dgx_texture* dgx_create_texture(dgx_hardware* hardware, const dgx_texture_create_info* info) {
    VkFormat texture_format = dgx_to_vk_texture_format(info->format);

    // image

    VkImageCreateInfo image_create_info = {
        .sType          = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType      = VK_IMAGE_TYPE_2D,
        .extent.width   = max_u32(1, (uint32_t)(info->dimensions.x)),
        .extent.height  = max_u32(1, (uint32_t)(info->dimensions.y)),
        .extent.depth   = max_u32(1, (uint32_t)(info->dimensions.z)),
        .mipLevels      = max_u32(1, (uint32_t)(info->mipmap_layers)),
        .arrayLayers    = max_u32(1, (uint32_t)(info->array_length)),
        .format         = texture_format,
        .tiling         = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage          = dgx_texture_usage_to_vk_image_usage(info->usage) | dgx_memory_access_to_vk_image_usage(info->memory_access),
        .sharingMode    = VK_SHARING_MODE_EXCLUSIVE,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .flags          = 0,
    };

    VkImage vkimage; if (vkCreateImage(hardware->logical_device, &image_create_info, 0, &vkimage) != VK_SUCCESS) {
        return 0x0; // "failed to create image!"
    }

    // memory

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(hardware->logical_device, vkimage, &memory_requirements);

    VkDeviceMemory given_memory; uint32_t given_offset;
    int success = try_allocate_hardware_memory(
        hardware,
        info->memory_strategy,
        memory_requirements,
        &given_memory, 
        &given_offset
    );

    if (!success) {
        vkDestroyImage(hardware->logical_device, vkimage, 0);
        return 0x0; // failed to allocate memory
    }

    vkBindImageMemory(hardware->logical_device, vkimage, given_memory, given_offset);
 
    // view

    VkImageViewCreateInfo view_create_info = {
        .sType                              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                              = vkimage,
        .viewType                           = VK_IMAGE_VIEW_TYPE_2D,
        .format                             = texture_format,
        .subresourceRange.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel      = 0,
        .subresourceRange.levelCount        = 1,
        .subresourceRange.baseArrayLayer    = 0,
        .subresourceRange.layerCount        = 1,
    };

    VkImageView vkview; if (vkCreateImageView(hardware->logical_device, &view_create_info, 0, &vkview) != VK_SUCCESS) {
        vkDestroyImage(hardware->logical_device, vkimage, 0);
        vkFreeMemory(hardware->logical_device, given_memory, 0);
        return 0x0; // failed to create image view!
    }

    dgx_texture* texture = calloc(1, sizeof(dgx_texture));
    *texture = (dgx_texture){
        .owning_hardware    = hardware,
        .dimensions         = info->dimensions,
        .bytes_per_pixel    = vk_format_pixel_size(texture_format),
        .image              = vkimage,
        .view               = vkview,
        .memory             = given_memory
    };

    return texture;
}

void dgx_free_texture(dgx_texture* texture) {
    if (!texture) return;
    vkDestroyImage(texture->owning_hardware->logical_device, texture->image, 0);
    vkDestroyImageView(texture->owning_hardware->logical_device, texture->view, 0);
    vkFreeMemory(texture->owning_hardware->logical_device, texture->memory, 0);
    free(texture);
}

dgx_texture_dimensions dgx_texture_get_dimensions(dgx_texture* texture) {
    return texture->dimensions;
}

int dgx_texture_sync_upload(dgx_texture* texture, dgx_texture_dimensions texture_offset, const void* data, dgx_texture_dimensions write_dimensions) {
    dgx_staging_memory*             staging_memory = NULL;
    dgx_command_lists_allocator*    allocator = NULL;
    dgx_command_list*               list = NULL;
    dgx_cpu_signal*                 signal = NULL;
    dgx_hardware_queue*             queue = NULL;
    int success = 1;

    uint32_t texture_data_length = write_dimensions.x * write_dimensions.y * write_dimensions.z * texture->bytes_per_pixel;
    if (!texture_data_length) { goto _cleanup; }

    // Staging Memory

    dgx_staging_memory_create_info staging_memory_create_info = {
        .size_bytes = texture_data_length
    };
    staging_memory = dgx_create_staging_memory(texture->owning_hardware, &staging_memory_create_info);
    if (!staging_memory) { success = 0; goto _cleanup; }
    
    void* mapped = dgx_staging_memory_map(staging_memory, 0, texture_data_length);
    if (!mapped) { success = 0; goto _cleanup; }
        memcpy(mapped, data, texture_data_length);
    dgx_staging_memory_unmap(staging_memory);

    // Command List

    dgx_command_lists_allocator_create_info create_info = {
        .target_queue_type = dgx_hardware_queue_type_graphics
    };
    allocator = dgx_create_command_lists_allocator(texture->owning_hardware, &create_info);
    if (!allocator) { success = 0; goto _cleanup; }

    list = dgx_command_lists_allocator_alloc_command_list(allocator);
    if (!list) { success = 0; goto _cleanup; }

    dgx_begin_command_list_recording(list);
        dgx_cmd_sync_textures(
            list, dgx_texture_sync_point_this_command, dgx_texture_sync_point_transfer_destination,
            1, &texture
        );

        dgx_cmd_copy_staging_memory_to_texture(
            list, staging_memory, texture, 0, 
            texture_offset,
            write_dimensions
        );
    dgx_finish_command_list_recording(list);

    // Submit, Execute, Wait
    dgx_hardware_query_queues(texture->owning_hardware, dgx_hardware_queue_type_graphics, 0, 1, &queue);

    signal = dgx_create_cpu_signal(texture->owning_hardware, &(dgx_cpu_signal_create_info){.initialy_signaled = 0});
    if (!signal) { success = 0; goto _cleanup; }

    dgx_submit_info submit_info = {
        .command_lists_count    = 1,
        .command_lists          = &list,
        .cpu_signal             = signal,
        .wait_gpu_signals_count = 0
    };
    dgx_submit_command_list(queue, &submit_info);
    dgx_cpu_signal_wait(signal);

_cleanup:
    dgx_free_cpu_signal(signal);
    dgx_free_command_lists_allocator(allocator);
    dgx_free_staging_memory(staging_memory);
    return success;
}


/* ===== window.c ===== */

// ===========================
// Query

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
    if (details.capabilities.currentExtent.width != UINT32_MAX) {
        extend = details.capabilities.currentExtent;
    } 
    else {
        extend = (VkExtent2D){
            (uint32_t)(window_framebuffer_width),
            (uint32_t)(window_framebuffer_height)
        };

        extend.width  = clamp_u32(
            extend.width, details.capabilities.minImageExtent.width, details.capabilities.maxImageExtent.width
        );

        extend.height = clamp_u32(
            extend.height, details.capabilities.minImageExtent.height, details.capabilities.maxImageExtent.height
        );
    }

    // Swapchain images count
    uint32_t image_count = desired_images_count; 
    image_count = max_u32(image_count, details.capabilities.minImageCount); // minimal limit
    if (details.capabilities.maxImageCount) 
        image_count = min_u32(image_count, details.capabilities.maxImageCount); // maximal limit (if exists)

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
// Callbacks Forwards

void window_resized_callback(GLFWwindow* window, int width, int height);
void window_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

// ===========================
// Platform Window Creation

int create_window_window(dgx_window* window, const dgx_window_create_info* info) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    window->platform_window = glfwCreateWindow(info->width, info->height, info->title, 0, 0);
    if (!window->platform_window) return 0;

    glfwSetWindowSizeCallback(window->platform_window, window_resized_callback);
    glfwSetScrollCallback(window->platform_window, window_scroll_callback);
    glfwSetWindowUserPointer(window->platform_window, window);  // set glfw payload to owning dgx window

    return 1;
}

int create_window_surface(dgx_window* window) {
    // try to create surface
    if (glfwCreateWindowSurface(
            window->owning_hardware->owning_library->instance, 
            window->platform_window, 
            0, &window->surface
        ) != VK_SUCCESS) return 0; // failed to create surface

    // check if hardware support surface
    VkBool32 presentation_supported;
    vkGetPhysicalDeviceSurfaceSupportKHR(
        window->owning_hardware->physical_device, 
        window->owning_hardware->presentation_queue->family_index, 
        window->surface, 
        &presentation_supported
    );

    if (!presentation_supported) {
        vkDestroySurfaceKHR(window->owning_hardware->owning_library->instance, window->surface, 0);
        return 0; // cannot present on this hardware
    }

    return 1;
}

void free_entire_platform(dgx_window* window) {
    if (window->surface)         vkDestroySurfaceKHR(window->owning_hardware->owning_library->instance, window->surface, 0);
    if (window->platform_window) glfwDestroyWindow(window->platform_window);
}

int create_entire_platform(dgx_window* window, const dgx_window_create_info* info) {
    if (!create_window_window (window, info)) goto _fail;
    if (!create_window_surface(window)) goto _fail;
    return 1;

_fail:
    free_entire_platform(window);
    return 0;
}

// ===========================
// Swapchain Creation

int create_swapchain_swapchain(dgx_window* window, const window_swapchain_settings* settings, VkSwapchainKHR old_swapchain) {
    // Swapchain creation info
    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface            = window->surface,
        .minImageCount      = settings->image_count,
        .imageFormat        = settings->format.format,
        .imageColorSpace    = settings->format.colorSpace,
        .imageExtent        = settings->extend,
        .imageArrayLayers   = 1,
        .imageUsage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .presentMode        = settings->presentation,
        .clipped            = VK_TRUE,
        .compositeAlpha     = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .oldSwapchain       = old_swapchain,
        .preTransform       = settings->pretransform
    };

    uint32_t queue_families_indices[] = {
        window->owning_hardware->graphics_queues[0].family_index, 
        window->owning_hardware->presentation_queue->family_index
    };

    if (window->owning_hardware->graphics_queues[0].family_index != window->owning_hardware->presentation_queue->family_index) {
        swapchain_create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices   = queue_families_indices;
    }
    else {
        swapchain_create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_create_info.queueFamilyIndexCount = 0;
        swapchain_create_info.pQueueFamilyIndices   = 0;
    }

    VkDevice device = window->owning_hardware->logical_device; 
    if (vkCreateSwapchainKHR(device, &swapchain_create_info, 0, &window->current_swapchain.swapchain) != VK_SUCCESS) {
        return 0; // failed to create swapchain
    }

    return 1;
}

int create_swapchain_images_and_views(dgx_window* window, const window_swapchain_settings* settings) {
    VkDevice device = window->owning_hardware->logical_device; 

    // get swapchain images count
    vkGetSwapchainImagesKHR(device, window->current_swapchain.swapchain, &window->current_swapchain.images_count, 0);

    // alloc memory
    window->current_swapchain.images        = malloc(window->current_swapchain.images_count * sizeof(VkImage));
    window->current_swapchain.images_views  = malloc(window->current_swapchain.images_count * sizeof(VkImageView));

    // get swapchain images
    vkGetSwapchainImagesKHR(device, window->current_swapchain.swapchain, &window->current_swapchain.images_count, window->current_swapchain.images);

    // create swapchain images views
    VkImageViewCreateInfo image_view_create_info = {
        .sType                              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType                           = VK_IMAGE_VIEW_TYPE_2D,
        .format                             = settings->format.format,
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
    
    for (uint32_t i = 0; i < window->current_swapchain.images_count; i++) {
        image_view_create_info.image = window->current_swapchain.images[i];
        if (vkCreateImageView(device, &image_view_create_info, 0, &window->current_swapchain.images_views[i]) != VK_SUCCESS) {
            for (uint32_t j = 0; j < i; j++) vkDestroyImageView(device, window->current_swapchain.images_views[j], 0);
            free(window->current_swapchain.images);
            free(window->current_swapchain.images_views);
            return 0; // failed to create image view
        }
    }

    return 1;
}

int create_swapchain_render_targets_layout(dgx_window* window, const window_swapchain_settings* settings) {
    VkAttachmentDescription color_attachment = {
        .format         = settings->format.format,          // copy format of swapchain
        .samples        = VK_SAMPLE_COUNT_1_BIT,            // no multisampling on window
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,      // clear window
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,     // draw to window buffer
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // dont care about clearing stencil
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // dont care to draw to stencil
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,        // dont care about previous image and it's format
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,  // images are to be presented in the swapchain
    };

    VkAttachmentReference color_attachment_ref = {
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // best peformance
        .attachment = 0,                                        // index 0 in color_attachments array
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint      = VK_PIPELINE_BIND_POINT_GRAPHICS,  // will draw at this subpass
        .colorAttachmentCount   = 1,
        .pColorAttachments      = &color_attachment_ref 
    };

    VkRenderPassCreateInfo render_pass_create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount    = 1,
        .pAttachments       = &color_attachment,
        .subpassCount       = 1,
        .pSubpasses         = &subpass,
    };

    VkRenderPass render_pass;
    if (vkCreateRenderPass(window->owning_hardware->logical_device, &render_pass_create_info, 0, &render_pass) != VK_SUCCESS) {
        return 0; // failed to create renderpass
    }

    window->current_swapchain.render_target_layout = calloc(1, sizeof(dgx_render_target_layout));
    *window->current_swapchain.render_target_layout = (dgx_render_target_layout){
        .owning_hardware = window->owning_hardware,
        .renderpass      = render_pass
    };

    return 1;
}

int create_swapchain_render_targets(dgx_window* window, const window_swapchain_settings* settings) {
    VkDevice device = window->owning_hardware->logical_device;
    window->current_swapchain.render_targets = calloc(window->current_swapchain.images_count, sizeof(dgx_render_target*));

    for (size_t i = 0; i < window->current_swapchain.images_count; i++) {
        dgx_render_target* render_target = calloc(1, sizeof(dgx_render_target));
        window->current_swapchain.render_targets[i] = render_target;

        VkImageView attachments[] = {
            window->current_swapchain.images_views[i]
        };

        VkFramebufferCreateInfo framebuffer_create_info_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = window->current_swapchain.render_target_layout->renderpass,
            .attachmentCount = 1,
            .pAttachments    = attachments,
            .width           = settings->extend.width,
            .height          = settings->extend.height,
            .layers          = 1,
        };

        VkFramebuffer framebuffer;
        if (vkCreateFramebuffer(device, &framebuffer_create_info_info, 0, &framebuffer) != VK_SUCCESS) {
            goto _failure; // failed to create framebuffer (render target)
        }

        *render_target = (dgx_render_target){
            .owning_hardware            = window->owning_hardware,
            .width                      = settings->extend.width,
            .height                     = settings->extend.height,
            .framebuffer                = framebuffer,
            .layout                     = window->current_swapchain.render_target_layout,
        };
    }

    return 1;

_failure:
    for (size_t i = 0; i < window->current_swapchain.images_count; i++) {
        dgx_free_render_target(window->current_swapchain.render_targets[i]);
    }

    return 0;
}

void free_entire_window_swapchain(dgx_window* window) {
    VkDevice device = window->owning_hardware->logical_device;

    if (window->current_swapchain.render_targets) {
        for (uint32_t i = 0; i < window->current_swapchain.images_count; i++) dgx_free_render_target(window->current_swapchain.render_targets[i]);
        free(window->current_swapchain.render_targets);
        window->current_swapchain.render_targets = NULL;
    }

    dgx_free_render_target_layout(window->current_swapchain.render_target_layout);
    window->current_swapchain.render_target_layout = NULL;

    if (window->current_swapchain.images_views) {
        for (uint32_t i = 0; i < window->current_swapchain.images_count; i++) vkDestroyImageView(device, window->current_swapchain.images_views[i], 0);
        free(window->current_swapchain.images_views);
        window->current_swapchain.images_views = NULL;
    }

    free(window->current_swapchain.images);
    window->current_swapchain.images = NULL;

    if (window->current_swapchain.swapchain) vkDestroySwapchainKHR(device, window->current_swapchain.swapchain, 0);
    window->current_swapchain.swapchain = VK_NULL_HANDLE;
}

// provided window->platform_window and window->sufrace are valid
int create_entire_swapchain
(dgx_window* window, VkSwapchainKHR old_swapchain, dgx_render_target_layout* old_layout) {    
    int framebuffer_width, framebuffer_height;
    glfwGetFramebufferSize(window->platform_window, &framebuffer_width, &framebuffer_height);
    window->current_swapchain.width  = framebuffer_width;
    window->current_swapchain.height = framebuffer_height;

    window_swapchain_settings swapchain_settings = pick_window_swapchain_settings(
        window->owning_hardware->physical_device,
        window->surface,
        framebuffer_width,
        framebuffer_height,
        window->desired_render_targets
    );

    if (!create_swapchain_swapchain(window, &swapchain_settings, old_swapchain)) goto _fail;
    if (!create_swapchain_images_and_views(window, &swapchain_settings))         goto _fail;

    if (old_layout) window->current_swapchain.render_target_layout = old_layout;
    else if (!create_swapchain_render_targets_layout(window, &swapchain_settings)) goto _fail;

    if (!create_swapchain_render_targets(window, &swapchain_settings))           goto _fail;

    return 1;

_fail:
    free_entire_window_swapchain(window);
    return 0;
}

// ===========================
// Swapchain Recreation

void window_recreate_swapchain(dgx_window* window) {
    vkDeviceWaitIdle(window->owning_hardware->logical_device);

    free_entire_window_swapchain(window);
    create_entire_swapchain(window, NULL, NULL);

    if (window->recreate_callback) 
        window->recreate_callback(window, 1);
}

// ===========================
// Window Changed Callbacks

void window_resized_callback(GLFWwindow* platform_window, int width, int height) {
    dgx_window* window = glfwGetWindowUserPointer(platform_window);
    if (window->current_swapchain.width == (uint32_t)width && window->current_swapchain.height == (uint32_t)height) return;
    window_recreate_swapchain(window);
}

void window_scroll_callback(GLFWwindow* platform_window, double xoffset, double yoffset) {
    dgx_window* window = glfwGetWindowUserPointer(platform_window);
    window->window_scroll_input = yoffset;
}

// ===========================
// Window Api

dgx_window* dgx_create_window(dgx_hardware* hardware, const dgx_window_create_info* info) {
    dgx_window* window = calloc(1, sizeof(dgx_window));
    *window = (dgx_window){
        .owning_hardware        = hardware,
        .desired_render_targets = info->desired_render_targets,
        .recreate_callback      = info->render_target_recreated_callback
    };

    if (!create_entire_platform(window, info)) goto _fail;
    if (!create_entire_swapchain(window, VK_NULL_HANDLE, NULL)) goto _fail;

    return window;

_fail:
    dgx_free_window(window);
    return 0x0;
}

void dgx_free_window(dgx_window* window) {
    if (!window) return;
    free_entire_window_swapchain(window);
    free_entire_platform(window);
    free(window);
}

uint32_t dgx_window_get_render_targets_count(dgx_window* window) {
    return window->current_swapchain.images_count;
}

uint32_t dgx_window_acquire_next_render_target_index(dgx_window* window, dgx_gpu_signal* can_render_signal) {
    uint32_t image_index;

    VkResult result = vkAcquireNextImageKHR(
        window->owning_hardware->logical_device, 
        window->current_swapchain.swapchain, 
        UINT64_MAX,
        can_render_signal->semaphore,
        VK_NULL_HANDLE, 
        &image_index
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        window_recreate_swapchain(window);
        return dgx_window_acquire_next_render_target_index(window, can_render_signal);
    } 
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return 0; // failed to acquire swap chain image
    }

    return image_index;
}

dgx_render_target_layout* dgx_window_get_render_target_layout(dgx_window* window) {
    return window->current_swapchain.render_target_layout;
}

dgx_render_target* dgx_window_get_render_target(dgx_window* window, uint32_t target_index) {
    if (target_index >= window->current_swapchain.images_count) return 0x0;
    return window->current_swapchain.render_targets[target_index];
}

void dgx_window_enqueue_render_target_present(dgx_window* window, uint32_t index, uint32_t wait_signals_count, dgx_gpu_signal** wait_signals) {
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount     = 1,
        .pSwapchains        = &window->current_swapchain.swapchain,
        .pImageIndices      = (uint32_t*)(&index),
        .pResults           = 0
    };

    VkSemaphore* sems = tlom_alloc_only(wait_signals_count * sizeof(VkSemaphore));
    for (uint32_t i = 0; i < wait_signals_count; i++) sems[i] = wait_signals[i]->semaphore;
    
    present_info.waitSemaphoreCount = wait_signals_count;
    present_info.pWaitSemaphores    = sems;

    vkQueuePresentKHR(
        window->owning_hardware->presentation_queue->handle,
        &present_info
    );
}

void dgx_window_update_input(dgx_window* window) {
    window->window_scroll_input = 0.0f;
    glfwPollEvents();
}

int dgx_window_query_shall_close(dgx_window* window) {
    return glfwWindowShouldClose(window->platform_window);
}

void dgx_window_query_is_focused(dgx_window* window, int* is) {
    if (is) *is = glfwGetWindowAttrib(window->platform_window, GLFW_FOCUSED);
}

void dgx_window_query_cursor_pos(dgx_window* window, uint32_t* xpos, uint32_t* ypos) {
    double x, y; glfwGetCursorPos(window->platform_window, &x, &y);
    if (xpos) *xpos = x;
    if (ypos) *ypos = y;
}

void dgx_window_query_input(dgx_window* window, int* left_pressed, int* right_pressed, float* scroll) {
    GLFWwindow* w = (GLFWwindow*)window->platform_window;

    if (left_pressed)   *left_pressed  = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (right_pressed)  *right_pressed = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (scroll)         *scroll        = window->window_scroll_input;
}

void dgx_window_get_size(dgx_window* window, uint32_t* width, uint32_t* height) {
    if (width)  *width  = window->current_swapchain.width;
    if (height) *height = window->current_swapchain.height;
}

#else
    #error No native api for demigurg graphics set!
#endif // DEMIGURG_GRAPHICS_VULKAN
#endif // DEMIGURG_GRAPHICS_IMPL
