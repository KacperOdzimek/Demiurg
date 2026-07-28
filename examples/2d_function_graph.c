#define DEMIURG_SHAPES_IMPL
#include "demiurg/rendering/shapes_rendering.h"

#define DEMIURG_GRAPHICS_IMPL
#define DEMIURG_GRAPHICS_VULKAN
#include "demiurg/platform/graphics.h"

#define DEMIURG_THREADS_IMPL
#include "demiurg/platform/threads.h"

#define DEMIURG_PARTITIONER_IMPL
#include "demiurg/algorithm/partitioner.h"

#include "demiurg/mathematics/camera.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// Displayed Functions
float displayed_function(float x, float z) {
    return sinf(x) * cosf(z);
}

// Config
#define FRAME_IN_FLIGHT_COUNT   3
#define PER_FRAME_STAGING       16 * 1024 * 1024
#define GRAPH_RESOLUTION        0.1f

// Context Objects
dgx_library*        lb;
dgx_hardware*       hw;
dgx_window*         ww;
dgx_staging_memory* sm;
dshp_shared*        sr;
dshp_frames*        sf;

// Timeline
dgx_timeline*       acquire_render_present_timeline;
uint64_t            acquire_render_present_iterator;
dgx_timeline*       shapes_upload_timeline;
uint64_t            shapes_upload_iterator;

// Per frame objects
uint32_t            frame_in_flight_itr = 0;
dgx_command_list*   render_list[FRAME_IN_FLIGHT_COUNT];
uint64_t            present_signal[FRAME_IN_FLIGHT_COUNT];

// Camera
dcam_orbit_camera camera = {
    .camera = {
        .yaw   = 0.0f,
        .pitch = 0.5f,
    },
    .target     = {0.0f, 0.0f, 0.0f},
    .distance   = 5.0f,
    .speed      = 1.0f
};

// Rendering Command List

typedef struct render_params {
    dgx_window*     window;
    uint32_t        index;
    dshp_context*   context;
} render_params;

void render_record(void* raw_params) {
    render_params* params = raw_params;

    uint32_t width, height;
    dgx_window_query_size(params->window, &width, &height);

    dgx_gcmd_begin_rendering(&(dgx_gcmd_rendering_info){
        .area_width = width,
        .area_height = height,
        .color_attachments_count = 1,
        .color_attachments = &(dgx_gcmd_rendering_attachment_info){
            .texture     = dgx_window_get_attachment_color(params->window, params->index),
            .load_op     = dgx_load_op_clear,
            .store_op    = dgx_store_op_store,
            .clear_color = (dgx_color){1, 1, 1, 1}
        }
    });

    dgx_gcmd_set_scissors(0, 0, width, height);
    dgx_gcmd_set_viewport(0, 0, width, height);
    dshp_gcmd_render(params->context);

    dgx_gcmd_finish_rendering();
}

// 3D to 2d projection, returns whether visible
int project_point(dla_vec3 world, dla_mat4* view_projection, dla_vec2* out) {
    dla_vec4 p = {world.x, world.y, world.z, 1.0f};
    p = dla_mat4_mul_vec4(*view_projection, p);
    if (p.w <= 0.0f) return 0;
    p.x /= p.w; p.y /= p.w;
    *out = (dla_vec2){p.x, p.y};
    return 1;
}

// Frame code
void frame() {
    // Wait for previous cycle to complete
    dgx_timeline_wait(acquire_render_present_timeline, present_signal[frame_in_flight_itr]);

    // Acquire next window attachment index
    uint32_t index; if (!dgx_window_acquire_index(
        ww, acquire_render_present_timeline, ++acquire_render_present_iterator, &index)
    ) return;

    // Get window dimensions
    uint32_t width, height; dgx_window_query_size(ww, &width, &height);

    // Calculate view projection
    dla_mat4 view_projection = dla_mat4_mul(
        dcam_perspective(DLA_PI / 3.0f, (float)width / height, 0.1f, 100.0f),
        dcam_orbit_camera_get_view_matrix(&camera)
    );

    // Rotate Camera
    dcam_orbit_camera_rotate(&camera, 0.0002f, 0, 0);

    // Draw Graph
    dshp_context ctx = {.frames = sf, .index = frame_in_flight_itr}; 
    dshp_reset(&ctx); dshp_set_color(&ctx, 0, 0, 0, 1);
    for (float x = -1.0f; x <= 1.0f; x += GRAPH_RESOLUTION) {
        for (float y = -1.0f; y <= 1.0f; y += GRAPH_RESOLUTION) {
            dla_vec3 pos = {x, displayed_function(x, y), y};
            dla_vec2 prj; if (!project_point(pos, &view_projection, &prj)) continue;
            dshp_circle(&ctx, prj, 0.01);
        }
    }

    // Upload to gpu
    if (!dshp_upload(
        &ctx, 0, 0, sm, frame_in_flight_itr * PER_FRAME_STAGING, PER_FRAME_STAGING, shapes_upload_timeline, ++shapes_upload_iterator
    )) return;

    // Create render list
    render_list[frame_in_flight_itr] = dgx_create_command_list(hw, &(dgx_command_list_create_info){
        .domain = dgx_command_domain_graphics,
        .aindex = 0,
        .parent = render_list[frame_in_flight_itr],
        .record = render_record,
        .params = &(render_params){
            .context = &ctx,
            .index   = index,
            .window  = ww
        }
    });
    
    // Render once window attachment can be rendered to
    // and shapes are uploaded
    dgx_command_list_submit(1, &render_list[frame_in_flight_itr], &(dgx_submit_info){
        .domain_work_group  = 0,
        .wait_count         = 1,
        .wait_timelines     = (dgx_timeline*[]){acquire_render_present_timeline, shapes_upload_timeline},
        .wait_values        = (uint64_t[]){acquire_render_present_iterator, shapes_upload_iterator},
        .signal_count       = 1,
        .signal_timelines   = &acquire_render_present_timeline,
        .signal_values      = (uint64_t[]){++acquire_render_present_iterator}
    });

    // Present once rendered
    dgx_window_submit_present(ww, index, acquire_render_present_timeline, acquire_render_present_iterator);
    present_signal[frame_in_flight_itr] = acquire_render_present_iterator;
    frame_in_flight_itr = (frame_in_flight_itr + 1) % FRAME_IN_FLIGHT_COUNT;
}

char* read_file(const char* path, uint32_t* size_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long n = ftell(f); rewind(f);

    char* buf = (n < 0) ? NULL : malloc((size_t)n + 1);
    if (!buf || fread(buf, 1, n, f) != (size_t)n) {
        free(buf); fclose(f);
        return NULL;
    }

    fclose(f); buf[n] = 0;
    if (size_out) *size_out = (uint32_t)n;
    return buf;
}

// Prompts the user for the shaders directory and builds full paths
int get_shader_paths(char* vert_path_out, char* frag_path_out, size_t buf_size) {
    char dir[512];

    printf("Enter path to shapes_rendering spirv shaders directory: ");
    if (!fgets(dir, sizeof(dir), stdin)) return 0;

    // Strip trailing newline
    size_t len = strlen(dir);
    if (len > 0 && dir[len - 1] == '\n') dir[len - 1] = '\0';

    if (dir[0] == '\0') return 0; // empty input

    snprintf(vert_path_out, buf_size, "%s/shader.vert.spirv", dir);
    snprintf(frag_path_out, buf_size, "%s/shader.frag.spirv", dir);

    return 1;
}

// Returns non-zero at success
int init() {
    // Library
    lb = dgx_create_library(&(dgx_library_create_info){}); if (!lb) return 0;

    // Hardware
    for (uint32_t i = 0; i < dgx_library_query_hardware_count(lb); i++) {
        if (!dgx_library_query_hardware_supported(lb, i))          continue;
        if (!dgx_library_query_hardware_windowing_support(lb, i))  continue;

        hw = dgx_create_hardware(lb, &(dgx_hardware_create_info){
            .hardware_index                                             = i,
            .enable_windowing                                           = 1,
            .work_groups_per_domain[dgx_command_domain_transfer]        = 1,
            .work_groups_per_domain[dgx_command_domain_compute]         = 0,
            .work_groups_per_domain[dgx_command_domain_graphics]        = 1,
            .shader_resources_limit[dgx_resource_type_storage_buffer]   = 10
        }); if (hw) break;;
    } if (!hw) return 0;

    // Window
    ww = dgx_create_window(hw, &(dgx_window_create_info){
        .title       = "DEMIURG 2D Function Graph Example",
        .width       = 800,
        .height      = 600,
        .attachments = FRAME_IN_FLIGHT_COUNT
    }); if (!ww) return 0;

    // Staging Memory
    sm = dgx_create_staging_memory(hw, &(dgx_staging_memory_create_info){
        .bytes = FRAME_IN_FLIGHT_COUNT * PER_FRAME_STAGING
    }); if (!sm) return 0;

    // Ask user for shader paths
    char vert_path[600], frag_path[600];
    if (!get_shader_paths(vert_path, frag_path, sizeof(vert_path))) return 0;

    // Shapes Rendering Vertex Shader
    dgx_shader_create_info vertex_shader_info;
    vertex_shader_info.source_code = read_file(vert_path, &vertex_shader_info.source_size);
    if (!vertex_shader_info.source_code) {
        printf("Failed to read vertex shader at: %s\n", vert_path); return 0;
    }

    // Shapes Rendering Pixel Shader
    dgx_shader_create_info pixel_shader_info;
    pixel_shader_info.source_code = read_file(frag_path, &pixel_shader_info.source_size);
    if (!pixel_shader_info.source_code) {
        printf("Failed to read pixel shader at: %s\n", frag_path); free((char*)vertex_shader_info.source_code); return 0;
    }

    // Shapes Rendering Shared
    sr = dshp_create_shared(hw, &(dshp_shared_create_info){
        .attachment_state = {
            .color_attachments_count    = 1,
            .color_attachments_formats  = (dgx_texture_format[]){dgx_window_get_attachment_format(ww)},
            .depth_stencil_format       = NULL
        },
        .vertex_shader_info = vertex_shader_info,
        .pixel_shader_info  = pixel_shader_info
    }); 
    
    // Free shader sources
    free((char*)vertex_shader_info.source_code);
    free((char*)pixel_shader_info.source_code);

    // Ensure shapes rendering created
    if (!sr) return 0;

    // Shapes Rendering Frames
    sf = dshp_create_frames(hw, &(dshp_frames_create_info){
        .shared = sr,
        .count  = FRAME_IN_FLIGHT_COUNT,
    }); if (!sf) return 0;

    // Acquire Render Present Timeline
    acquire_render_present_timeline = dgx_create_timeline(hw, &(dgx_timeline_create_info){
        .initial_value = 0
    }); if (!acquire_render_present_timeline) return 0;

    // Shapes Upload Timeline
    shapes_upload_timeline = dgx_create_timeline(hw, &(dgx_timeline_create_info){
        .initial_value = 0
    }); if (!shapes_upload_timeline) return 0;

    return 1;
}

void term() {
    for (int i = 0; i < FRAME_IN_FLIGHT_COUNT; i++) {
        dgx_free_command_list(render_list[i]);
    }
    dgx_hardware_wait_idle(hw);
    dshp_free_frames(sf);
    dshp_free_shared(sr);
    dgx_free_staging_memory(sm);
    dgx_free_timeline(acquire_render_present_timeline);
    dgx_free_timeline(shapes_upload_timeline);
    dgx_free_window(ww);
    dgx_free_hardware(hw);
    dgx_free_library(lb);
}

int main() {
    if (!init()) {
        term(); return 1;
    }

    while (!dgx_window_query_shall_close(ww)) {
        frame();
    }

    term();
}
