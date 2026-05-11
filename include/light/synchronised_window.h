/*
----------------------------------------------------------------
Contents

This file provides `managed window` object - window with automatic frames-in-flight synchronization.
The user provides a `lex_managed_window_frame_callback`, called once per frame.

----------------------------------------------------------------
Frame Model

- Each frame acquires an internal render target
- A `can_render_signal` is provided
- All GPU work writing to the render target MUST wait on this signal
- User can return their own signals - those will be waited on before presentation

----------------------------------------------------------------
User Responsibilities

1. Synchronization (required)
   - All rendering must wait on `can_render_signal`

2. Completion signals (required)
   - Return GPU signals that indicate frame completion
   - These are awaited before presentation

3. Resource lifetime
   - Render target and `can_render_signal` are temporary and must not be stored

----------------------------------------------------------------
Guarantees

- Frames-in-flight are handled internally
- Frames only begin after prior usage completes
- Presentation waits on returned and internal signals

----------------------------------------------------------------
Notes

- Callback runs on the enter calling thread
*/

#ifndef LEX_MANAGED_WINDOW_H
#define LEX_MANAGED_WINDOW_H

#include <lgx/gpu.h>

typedef struct lex_managed_window lex_managed_window;

typedef void(*lex_managed_window_frame_callback)(
    lex_managed_window*     managed_window,             // the window
    lgx_render_target*      render_target,              // the target to render to
    uint32_t                frame_in_flight_index,      // looping in range [0, managed_window.desired_frames_in_flight) - used to iterate per frame resources
    lgx_gpu_signal*         can_render_signal,          // gate signal to frame rendering
    uint32_t*               user_wait_signals_count,    // user_wait_signals count (result from function)
    lgx_gpu_signal***       user_wait_signals           // signals to wait on before frame presentation (result from function)
);

typedef struct lex_managed_window_create_info {
    const char*                         title;
    uint32_t                            width;
    uint32_t                            height;
    uint32_t                            desired_frames_in_flight;
    lex_managed_window_frame_callback   new_frame_callback;
} lex_managed_window_create_info;

lex_managed_window* lex_create_managed_window(lgx_hardware*, const lex_managed_window_create_info*);
void lex_free_managed_window(lex_managed_window*);

// Enters frame in flight generation from main loop (may iterate over multiple windows)
// if do_lock is non-zero code will wait till previous rendering work is finished to start a new rendering cycle
// else, if previous rendering work was not finished function returns without doing anything
void lex_managed_window_enter(lex_managed_window*, int do_lock);

// Get basic window out of managed window
lgx_window* lex_managed_window_get_window(lex_managed_window*);

#endif // LEX_MANAGED_WINDOW_H

#ifdef LEX_MANAGED_WINDOW_IMPL

/*
    Implementation note:
    * Sync model:
        - We enter frame of index *current_frame*
        - We wait for render_finished_cpu[current_frame] in case this frame is still in rendering
        - Render Target Acquire enables render_target_ready_for_self[current_frame] and render_target_ready_for_user[current_frame]
        - render_target_ready_for_user[current_frame] is passed to user and enables rendering
        - user provides us with array of gpu_signals to wait for
        - We wait for user provided signals + render_target_ready_for_self[current_frame] 
            - this enables render_finished_gpu[current_frame] and render_finished_cpu[current_frame]
        - On render_finished_gpu[current_frame] render target is enqued to be presented
        - We dont wait for that - next frame may be entered and rendered to
*/

#include <stdlib.h>

struct lex_managed_window {
    lgx_hardware*       owning_hardware;
    lgx_hardware_queue* graphics_queue;
    lgx_window*         window;

    // callback for user to insert their own frame code
    lex_managed_window_frame_callback new_frame_callback;

    // frames_in_flight iterator
    // walks 0, 1, 2, .. frames_in_flight, 0, 1, 2, ... ... 
    uint32_t current_frame;

    // count of frames we work on at same time
    // all signals below exists one per frames_in_flight
    uint32_t frames_in_flight;

    // ensures previous cycle ends before new happens
    lgx_cpu_signal**    render_finished_cpu;

    // ensures presentation of frame after rendering finishes
    lgx_gpu_signal**    render_finished_gpu;

    // ensures sync between acquire and present in case user does not wait on 
    // render_target_ready_for_user or dont give back any signals to wait for
    lgx_gpu_signal**    render_target_ready_for_self;

    // enables user to enqueue rendering as soon as frame is acquired
    lgx_gpu_signal**    render_target_ready_for_user;
};

lex_managed_window* lex_create_managed_window(lgx_hardware* hardware, const lex_managed_window_create_info* info) {
    lex_managed_window* managed_window = malloc(sizeof(lex_managed_window));
    *managed_window = (lex_managed_window){
        .owning_hardware    = hardware,
        .new_frame_callback = info->new_frame_callback,
        .current_frame      = 0,
        .frames_in_flight   = info->desired_frames_in_flight
    };

    lgx_window_create_info window_create_info = {
        .title                  = info->title,
        .width                  = info->width,
        .height                 = info->height,
        .desired_render_targets = info->desired_frames_in_flight
    };
    managed_window->window = lgx_create_window(hardware, &window_create_info);

    lgx_cpu_signal_create_info cpu_create_info = {
        .initialy_signaled = 1
    };

    lgx_hardware_get_queues(hardware, lgx_hardware_queue_type_graphics, 0, 1, &managed_window->graphics_queue);

    managed_window->render_finished_cpu = malloc(sizeof(lgx_cpu_signal*) * managed_window->frames_in_flight);
    managed_window->render_finished_gpu = malloc(sizeof(lgx_gpu_signal*) * managed_window->frames_in_flight);
    managed_window->render_target_ready_for_self = malloc(sizeof(lgx_gpu_signal*) * managed_window->frames_in_flight);
    managed_window->render_target_ready_for_user = malloc(sizeof(lgx_gpu_signal*) * managed_window->frames_in_flight);

    for (uint32_t i = 0; i < managed_window->frames_in_flight; i++) {
        managed_window->render_finished_cpu[i] = lgx_create_cpu_signal(hardware, &cpu_create_info);
        managed_window->render_finished_gpu[i] = lgx_create_gpu_signal(hardware);
        managed_window->render_target_ready_for_self[i] = lgx_create_gpu_signal(hardware);
        managed_window->render_target_ready_for_user[i] = lgx_create_gpu_signal(hardware);
    }

    return managed_window;
}

void lex_free_managed_window(lex_managed_window* managed_window) {
    for (uint32_t i = 0; i < managed_window->frames_in_flight; i++) {
        lgx_free_cpu_signal(managed_window->render_finished_cpu[i]);
        lgx_free_gpu_signal(managed_window->render_finished_gpu[i]);
        lgx_free_gpu_signal(managed_window->render_target_ready_for_self[i]);
        lgx_free_gpu_signal(managed_window->render_target_ready_for_user[i]);
    }

    free(managed_window->render_finished_cpu);
    free(managed_window->render_finished_gpu);
    free(managed_window->render_target_ready_for_self);
    free(managed_window->render_target_ready_for_user);

    lgx_free_window(managed_window->window);
    free(managed_window);
}

void lex_managed_window_enter(lex_managed_window* managed_window, int do_lock) {
    uint32_t current_frame = managed_window->current_frame;

    // Wait for previous cycle to finish
    if (do_lock) {
        lgx_cpu_signal_wait (managed_window->render_finished_cpu[current_frame]);
        lgx_cpu_signal_reset(managed_window->render_finished_cpu[current_frame]);
    }
    else {
        if (!lgx_cpu_signal_signaled(managed_window->render_finished_cpu[current_frame])) return;
        lgx_cpu_signal_reset(managed_window->render_finished_cpu[current_frame]);
    }

    // Accquire next frame render target
    uint32_t drawn_target_index = lgx_window_acquire_next_render_target_index(
        managed_window->window, managed_window->render_target_ready_for_self[current_frame]
    );

    // Submit empty call to duplicate render_target_ready
    // Once executed signal render_target_ready_for_self and render_target_ready_for_user
    lgx_gpu_signal* render_target_ready_signals[] = {
        managed_window->render_target_ready_for_self[current_frame],
        managed_window->render_target_ready_for_user[current_frame]
    };

    lgx_submit_info multiply_submit_info = {
        .command_lists_count        = 0,
        .command_lists              = NULL,
        .wait_gpu_signals_count     = 1,
        .wait_gpu_signals           = &managed_window->render_target_ready_for_self[current_frame],
        .signal_gpu_signals_count   = 2,
        .signal_gpu_signals         = render_target_ready_signals
    };
    lgx_submit_command_list(managed_window->graphics_queue, &multiply_submit_info);

    // Execute user code, let user wait on their copy of render_target_ready, get rendering signals from them
    uint32_t            render_finished_signals_count   = 0;
    lgx_gpu_signal**    render_finished_signals         = NULL;
    managed_window->new_frame_callback(
        managed_window,
        lgx_window_get_render_target(managed_window->window, drawn_target_index),
        current_frame,
        managed_window->render_target_ready_for_user[current_frame],
        &render_finished_signals_count,
        &render_finished_signals
    );

    // Wait on all user signals plus render_target_ready_for_self
    lgx_gpu_signal* waits[render_finished_signals_count + 1];
    for (uint32_t i = 0; i < render_finished_signals_count; i++) waits[i] = render_finished_signals[i];
    waits[render_finished_signals_count] = managed_window->render_target_ready_for_self[current_frame];

    // Submit empty call, wait on waits signals
    // Once execute allow next cycle, and allow presentation
    lgx_submit_info render_wait_submit_info = {
        .command_lists_count        = 0,
        .command_lists              = NULL,
        .wait_gpu_signals_count     = render_finished_signals_count + 1,
        .wait_gpu_signals           = waits,
        .signal_gpu_signals_count   = 1,
        .signal_gpu_signals         = &managed_window->render_finished_gpu[current_frame],
        .cpu_signal                 = managed_window->render_finished_cpu[current_frame],
    };
    lgx_submit_command_list(managed_window->graphics_queue, &render_wait_submit_info);

    // Present, and move onto next frame
    lgx_window_enqueue_render_target_present(
        managed_window->window, 
        drawn_target_index, 
        1, &managed_window->render_finished_gpu[current_frame]
    );
    current_frame = (current_frame + 1) % managed_window->frames_in_flight;
    managed_window->current_frame = current_frame;
}

lgx_window* lex_managed_window_get_window(lex_managed_window* managed_window) {
    return managed_window->window;
}

#endif // LEX_MANAGED_WINDOW_IMPL
