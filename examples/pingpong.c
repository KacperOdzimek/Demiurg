#include "light/graphics.h"
#include "light/synchronised_window.h"
#include "light/shapes_rendering.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
    Config
*/

#define INITIAL_WIN_WIDTH   800
#define INITIAL_WIN_HEIGHT  600
#define FRAMES_IN_FLIGHT    3

#define STAGING_MEMORY_SIZE (FRAMES_IN_FLIGHT * 4 * 1024 * 1024)

#define SHAPES_RENDERING_SHADER_VERT_PATH   "../shaders/shapes_rendering/shader.vert"
#define SHAPES_RENDERING_SHADER_FRAG_PATH   "../shaders/shapes_rendering/shader.frag"

/*
    Pingpong Game Logics
*/

int score_left  = 0;
int score_right = 0;

// [-1 = down, 1 = top]
float left_pos_y  = 0;
float right_pos_y = 0;

/*
    Light Part
*/

lgx_library*                    lgx_lib;
lgx_hardware*                   hardware;
lgx_hardware_queue*             graphics_queue;
lgx_command_lists_allocator*    command_lists_allocator;
lgx_staging_memory*             staging_memory;

lswin_synchronised_window*      swindow;
lgx_window*                     window;

lshp_shared*                    shp_shared;
lshp_frames_contextes*          shp_frames_contextes;

typedef struct per_frame_resources {
    lgx_command_list*   shapes_upload_cl;
    lgx_command_list*   rendering_cl;
    lgx_gpu_signal*     rendering_finished;
} per_frame_resources;

per_frame_resources per_frame[FRAMES_IN_FLIGHT];

void term();

void frame(
    lswin_synchronised_window*  synchronised_window,
    lgx_render_target*          render_target,
    uint32_t                    frame_in_flight_index,
    lgx_gpu_signal*             can_render_signal,
    uint32_t*                   user_wait_signals_count,
    lgx_gpu_signal***           user_wait_signals
);

void compile_shader(const char* path, const char** read_buffer_result, uint32_t* read_buffer_size_result) {
    // Compile shader to a shader.spv file
    const char* command_begin  = "glslc ";
    const char* command_finish = " -o shader.spv";
    
    int   command_itr = 0;
    char* command = malloc((strlen(command_begin) + strlen(path) + strlen(command_finish) + 1) * sizeof(char));

    const char* fragments[] = {command_begin, path, command_finish};
    for (int i = 0; i < sizeof(fragments) / sizeof(const char*); i++) {
        int fragment_length = strlen(fragments[i]);
        memcpy(command + command_itr, fragments[i], fragment_length);
        command_itr += fragment_length;
    }
    command[command_itr] = '\0';
    
    int failure = system(command);
    free(command);
    if (failure) {
        term();
        exit(1);
    }

    // Read shader.spv file
    FILE* f = fopen("shader.spv", "rb");
    if (!f) {
        term();
        exit(1);
    }
    
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* string = malloc(fsize + 1);
    if (!string) {
        term();
        fclose(f);
        exit(1);
    }

    fread(string, fsize, 1, f);
    fclose(f);

    *read_buffer_result      = string;
    *read_buffer_size_result = (uint32_t)fsize;
}

void init() {
    // Gpu Connection

    lgx_library_create_info library_ci = {
        .platform_code_enabled = 1
    };
    lgx_lib = lgx_create_library(&library_ci);
    if (!lgx_lib) goto _fail;

    lgx_hardware_create_info hardware_ci = {
        .require_presentation_queue = 1,
        .require_graphics_queues    = 1,  
        .desired_graphics_queues    = 1,
        // rest 0
    };
    hardware = lgx_create_hardware(lgx_lib, &hardware_ci);
    if (!hardware) goto _fail;
    lgx_hardware_get_queues(hardware, lgx_hardware_queue_type_graphics, 0, 1, &graphics_queue);

    lgx_command_lists_allocator_create_info command_lists_allocator_ci = {
        .target_queue_type          = lgx_hardware_queue_type_graphics,
        .allow_individual_resets    = 1,
        .often_recorded             = 1
    };
    command_lists_allocator = lgx_create_command_lists_allocator(hardware, &command_lists_allocator_ci);
    if (!command_lists_allocator) goto _fail;

    lgx_staging_memory_create_info staging_memory_ci = {
        .size_bytes = STAGING_MEMORY_SIZE
    };
    staging_memory = lgx_create_staging_memory(hardware, &staging_memory_ci);
    if (!staging_memory) goto _fail;

    // Window

    lswin_synchronised_window_create_info synchronised_window_ci = {
        .title                      = "Light Framework Example - Pingpong game",
        .width                      = INITIAL_WIN_WIDTH,
        .height                     = INITIAL_WIN_HEIGHT,
        .desired_frames_in_flight   = FRAMES_IN_FLIGHT,
        .new_frame_callback         = frame
    };
    swindow = lswin_create_synchronised_window(hardware, &synchronised_window_ci);
    if (!swindow) goto _fail;
    window = lswin_synchronised_window_get_window(swindow);

    // Shapes Drawing Pipeline

    lshp_shared_create_info shp_shared_ci = {
        .pipeline_render_target_layout = lgx_window_get_render_target_layout(window)
    };

    compile_shader(
        SHAPES_RENDERING_SHADER_VERT_PATH, 
        &shp_shared_ci.pipeline_vertex_shader_source_code, 
        &shp_shared_ci.pipeline_vertex_shader_source_size
    );

    compile_shader(
        SHAPES_RENDERING_SHADER_FRAG_PATH, 
        &shp_shared_ci.pipeline_pixel_shader_source_code, 
        &shp_shared_ci.pipeline_pixel_shader_source_size
    );

    shp_shared = lshp_create_shared(hardware, &shp_shared_ci);
    free((char*)shp_shared_ci.pipeline_vertex_shader_source_code);
    free((char*)shp_shared_ci.pipeline_pixel_shader_source_code);
    if (!shp_shared) goto _fail;

    lshp_frames_contextes_create_info shp_frames_contextes_ci = {
        .shared                 = shp_shared,
        .frames_in_flight_count = FRAMES_IN_FLIGHT,
    };
    shp_frames_contextes = lshp_create_frames_contextes(hardware, &shp_frames_contextes_ci);
    if (!shp_frames_contextes) goto _fail;

    // Per frame resources

    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        per_frame[i] = (per_frame_resources){
            .shapes_upload_cl   = lgx_command_lists_allocator_alloc_command_list(command_lists_allocator),
            .rendering_cl       = lgx_command_lists_allocator_alloc_command_list(command_lists_allocator),
            .rendering_finished = lgx_create_gpu_signal(hardware)
        };
    }

    return;
_fail:
    term();
    exit(1);
}

void frame(
    lswin_synchronised_window*  synchronised_window,
    lgx_render_target*          render_target,
    uint32_t                    frame_in_flight_index,
    lgx_gpu_signal*             can_render_signal,
    uint32_t*                   user_wait_signals_count,
    lgx_gpu_signal***           user_wait_signals
) {
    lshp_frame_context shp_frame_context = {
        .contextes          = shp_frames_contextes,
        .frame_in_flight    = frame_in_flight_index
    };

    lshp_circle(&shp_frame_context, 0, 0, 0.5);
    lshp_rect(&shp_frame_context, -1, -1, 0, 0);

    lshp_upload(
        per_frame[frame_in_flight_index].shapes_upload_cl,
        graphics_queue, &shp_frame_context, staging_memory,
        (STAGING_MEMORY_SIZE / FRAMES_IN_FLIGHT) * frame_in_flight_index,   // divide staging memory into chunks per frame in flight
        STAGING_MEMORY_SIZE / FRAMES_IN_FLIGHT,                             // to avoid overwrite
        NULL, NULL
    );

    lgx_command_list* render_list = per_frame[frame_in_flight_index].rendering_cl;
    lgx_begin_command_list_recording(render_list);
        lgx_color clear_color = {0, 0, 0, 1}; // black

        lgx_gcmd_begin_render_target_write_info rt_write_info = {
            .clear_colors_count = 1,
            .clear_colors       = &clear_color,
            .render_target      = render_target
        };
        lgx_gcmd_begin_render_target_write(render_list, &rt_write_info);
            uint32_t w, h; lgx_window_get_size(window, &w, &h);
            lgx_gcmd_set_viewport(render_list, 0, 0, w, h);
            lgx_gcmd_set_scissors(render_list, 0, 0, w, h);
            lshp_gcmd_render(render_list, &shp_frame_context);
        lgx_gcmd_end_render_target_write(render_list);
    lgx_finish_command_list_recording(render_list);


    lgx_submit_info sb_info = {
        .command_lists_count        = 1,
        .command_lists              = &render_list,
        .cpu_signal                 = NULL,
        .signal_gpu_signals_count   = 1,
        .signal_gpu_signals         = &per_frame[frame_in_flight_index].rendering_finished,
        .wait_gpu_signals_count     = 1,
        .wait_gpu_signals           = &can_render_signal
    };
    lgx_submit_command_list(graphics_queue, &sb_info);

    *user_wait_signals_count = 1;
    *user_wait_signals       = &per_frame[frame_in_flight_index].rendering_finished;
}

void term() {
    if (hardware) {
        lgx_hardware_wait_idle(hardware);

        for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
            lgx_free_gpu_signal(per_frame[i].rendering_finished);
        }

        lshp_free_frames_contextes(shp_frames_contextes);
        lshp_free_shared(shp_shared);

        lswin_free_synchronised_window(swindow);

        lgx_free_staging_memory(staging_memory);
        lgx_free_command_lists_allocator(command_lists_allocator);
        lgx_free_hardware(hardware);
    }

    lgx_free_library(lgx_lib);
}

int main() {
    init();
    while (!lgx_window_shall_close(window)) {
        lswin_synchronised_window_enter(swindow, 1);
        lgx_window_update_input(window);
    }
    term();
}

/*
    Libs Impl
*/

#define LIGHT_GRAPHICS_IMPL
#define LIGHT_GRAPHICS_VULKAN
#define LIGHT_GRAPHICS_VALIDATE
#include "light/graphics.h"
#undef LIGHT_GRAPHICS_IMPL

#define LIGHT_SYNCHRONISED_WINDOW_IMPL
#include "light/synchronised_window.h"
#undef LIGHT_SYNCHRONISED_WINDOW_IMPL

#define LIGHT_SHAPES_IMPL
#include "light/shapes_rendering.h"
#undef LIGHT_SHAPES_IMPL
