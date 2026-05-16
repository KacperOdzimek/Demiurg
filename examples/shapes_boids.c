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

// realtive to examples/build
#define SHAPES_RENDERING_SHADER_VERT_PATH   "../../shaders/shapes_rendering/shader.vert"
#define SHAPES_RENDERING_SHADER_FRAG_PATH   "../../shaders/shapes_rendering/shader.frag"

/*
    Background Drawing
*/

#include <math.h>

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static void draw_cloud(
    lshp_frame_context* ctx,
    float x,
    float y,
    float s
) {
    // shadow
    lshp_set_color(ctx, 0.75f, 0.82f, 0.95f, 0.25f);

    lshp_circle(ctx, x + 0.03f*s, y - 0.02f*s, 0.10f*s);
    lshp_circle(ctx, x + 0.12f*s, y - 0.01f*s, 0.12f*s);
    lshp_circle(ctx, x + 0.22f*s, y - 0.02f*s, 0.09f*s);

    // body
    lshp_set_color(ctx, 1.0f, 1.0f, 1.0f, 0.95f);

    lshp_circle(ctx, x,           y,            0.10f*s);
    lshp_circle(ctx, x + 0.10f*s, y + 0.05f*s, 0.14f*s);
    lshp_circle(ctx, x + 0.22f*s, y + 0.03f*s, 0.11f*s);
    lshp_circle(ctx, x + 0.32f*s, y,            0.08f*s);

    // highlight
    lshp_set_color(ctx, 1.0f, 1.0f, 1.0f, 0.18f);

    lshp_circle(ctx, x + 0.06f*s, y + 0.09f*s, 0.04f*s);
    lshp_circle(ctx, x + 0.17f*s, y + 0.11f*s, 0.05f*s);
}

void draw_sky(lshp_frame_context* ctx, float dt) {

    static float t = 0.0f;
    t += dt;

    // Sky gradient
    const int layers = 64;
    for (int i = 0; i < layers; ++i) {
        float k = (float)i / (float)(layers - 1);

        // deep orange/red sunset
        float r = 0.90f + 0.10f * k;
        float g = 0.12f + 0.38f * k;
        float b = 0.02f + 0.12f * k;

        lshp_set_color(ctx, r, g, b, 1.0f);

        float y0 = -1.0f + (2.0f * k);
        float y1 = y0 + (2.0f / layers) + 0.1f;

        lshp_rect(ctx, -1.0f, y0, 1.0f, y1);
    }

    // Haze
    for (int i = 0; i < 10; ++i) {

        float k = (float)i / 9.0f;

        float y0 = -1.0f + k * 0.35f;
        float y1 = y0 + 0.05f;

        lshp_set_color(ctx, 1.0f, 1.0f, 1.0f, 0.02f);

        lshp_rect(ctx, -1.0f, y0, 1.0f, y1);
    }
}

/*
    Boids Example
*/

#include <math.h>

#define BOID_COUNT      3000
#define BOID_SPEED      0.2f
#define BOID_SIZE       0.005f
#define BOID_VIEW       0.12f
#define BOID_TURN       1.5f

#define BOID_MARGIN     0.1f
#define BOID_WALL_FORCE 1.0f

void boids_simulate_and_draw(lshp_frame_context* ctx, float dt) {
    lshp_set_color(ctx, 0, 0, 0, 1);

    static int boid_init;
    static struct {
        float x, y;
        float vx, vy;
    } b[BOID_COUNT];

    if (!boid_init) {
        boid_init = 1;

        for (int i = 0; i < BOID_COUNT; i++) {
            float a = (float)rand() / RAND_MAX * 6.28318f;

            b[i].x  = (float)rand() / RAND_MAX * 2.f - 1.f;
            b[i].y  = (float)rand() / RAND_MAX * 2.f - 1.f;

            b[i].vx = cosf(a) * BOID_SPEED;
            b[i].vy = sinf(a) * BOID_SPEED;
        }
    }

    for (int i = 0; i < BOID_COUNT; i++) {
        float ax = 0.f;
        float ay = 0.f;

        for (int j = 0; j < BOID_COUNT; j++) {
            if (i == j)
                continue;

            float dx = b[j].x - b[i].x;
            float dy = b[j].y - b[i].y;

            float d2 = dx * dx + dy * dy;

            if (d2 < BOID_VIEW * BOID_VIEW) {
                // cohesion + alignment
                ax += dx * 0.2f + b[j].vx * 0.05f;
                ay += dy * 0.2f + b[j].vy * 0.05f;

                // separation
                float sep = BOID_SIZE * 1.5;
                if (d2 < sep) {
                    ax -= dx * 4.f;
                    ay -= dy * 4.f;
                }
            }
        }

        // screen edge avoidance
        if (b[i].x < -1.f + BOID_MARGIN)
            ax += BOID_WALL_FORCE;

        if (b[i].x > 1.f - BOID_MARGIN)
            ax -= BOID_WALL_FORCE;

        if (b[i].y < -1.f + BOID_MARGIN)
            ay += BOID_WALL_FORCE;

        if (b[i].y > 1.f - BOID_MARGIN)
            ay -= BOID_WALL_FORCE;

        // integrate velocity
        b[i].vx += ax * BOID_TURN * dt;
        b[i].vy += ay * BOID_TURN * dt;

        // normalize speed
        float s = sqrtf(
            b[i].vx * b[i].vx +
            b[i].vy * b[i].vy
        );

        b[i].vx *= BOID_SPEED / s;
        b[i].vy *= BOID_SPEED / s;

        // integrate position
        b[i].x += b[i].vx * dt;
        b[i].y += b[i].vy * dt;

        // facing triangle
        float nx = b[i].vx / BOID_SPEED;
        float ny = b[i].vy / BOID_SPEED;

        float px = -ny;
        float py =  nx;

        float sx = BOID_SIZE;
        float sy = BOID_SIZE * 0.5f;

        lshp_triangle(
            ctx,

            b[i].x + nx * sx,
            b[i].y + ny * sx,

            b[i].x - nx * sx + px * sy,
            b[i].y - ny * sx + py * sy,

            b[i].x - nx * sx - px * sy,
            b[i].y - ny * sx - py * sy
        );
    }
}

/*
    Little Backend Part
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
    lgx_hardware_query_queues(hardware, lgx_hardware_queue_type_graphics, 0, 1, &graphics_queue);

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
        .title                      = "Light Framework Example - Shape Rendering",
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

    float dt = 1.0f / 1000 * 16; // not to code much hardcoded time delta 16ms
    lshp_reset(&shp_frame_context, 1, 1);
    draw_sky(&shp_frame_context, dt);
    boids_simulate_and_draw(&shp_frame_context, dt);

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
    while (!lgx_window_query_shall_close(window)) {
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
