/*
----------------------------------------------------------------
Contents:

This file implements input system, allowing user to read input from mouses, keyboards and gamepads.

----------------------------------------------------------------
Code info:
- lin prefix
- LIGHT_INPUT_IMPL macro to build
- User must pick target OS system by using of the macros below:
    - LIGHT_INPUT_LINUX

----------------------------------------------------------------
Depedencies:
- each OS have own compilation requirements:
    - LIGHT_INPUT_LINUX
        - libevdev library installed

----------------------------------------------------------------
Usage:
- Create lin_library object once
- Create as many lin_slots as many devices you like to read from
    For example in game for four players, you can create four slots, to read info from four gamepdas
- Query currently visible devices by lin_library_get_devices_list
- Connect selected device from the list to a slot with lin_slot_connect
- Query device input from slot via lin_slot_query_input_state
- Check for button presses by input_state.buttons[button from enum] 
    and axis state by input_state.axes[axis from enum]
*/

#ifndef LIGHT_INPUT_H
#define LIGHT_INPUT_H

#include <stddef.h>

// Library

typedef struct lin_library lin_library;

lin_library* lin_create_library();
void lin_free_library(lin_library*);

// Devices List

typedef struct lin_devices_list lin_devices_list;

// get OS visible devices at the moment
// may return NULL if failed to access
lin_devices_list* lin_library_get_devices_list(lin_library*);
void lin_free_devices_list(lin_devices_list*);

typedef enum lin_device_type {
    lin_device_unknown,
    lin_device_mouse,
    lin_device_keyboard,
    lin_device_gamepad,
} lin_device_type;

size_t          lin_devices_list_get_size   (lin_devices_list*);
lin_device_type lin_devices_list_query_type (lin_devices_list*, size_t dev_index);
const char*     lin_devices_list_query_name (lin_devices_list*, size_t dev_index);

// Slot

typedef struct lin_slot lin_slot;

lin_slot* lin_create_slot(lin_library*);
void lin_free_slot(lin_slot*);

// tries to connect to device, 1 at success, 0 at failure
int  lin_slot_connect(lin_slot*, lin_devices_list*, size_t dev_index);

// drops device connection
void lin_slot_disconnect(lin_slot*);

// returns 1 if connected, 0 if never connected or lost connection
int lin_slot_connected(lin_slot*);

// set conversion factor for translation mouse movement -> mouse axis
void lin_slot_set_mouse_sensitivity(lin_slot*, float sensitivity);

// deadzone shall be between [0, 1]
// 0 means no deadzone, 1 means binary state, either nothing or full press
// if other value is set, till that value, lin_slot_query_input_state[axis] will return 0.0f
// afterwards that limit, axis will interpolate towards full press
void lin_slot_set_axis_deadzone(lin_slot*, unsigned int axis, float deadzone);

typedef struct lin_input_state lin_input_state;
// get current connected device input state
// unconnected device always yields input state, with everything 0
void lin_slot_query_input_state(lin_slot*, lin_input_state*);

// Input State

typedef enum lin_button {
    // Mouse (ms)
    lin_btn_ms_left,
    lin_btn_ms_right,
    lin_btn_ms_middle,
    lin_btn_ms_back,
    lin_btn_ms_forward,
    
    // Keyboard (kb)
    lin_btn_kb_escape,
    lin_btn_kb_tab,
    lin_btn_kb_enter,
    lin_btn_kb_space,
    lin_btn_kb_backspace,
    lin_btn_kb_delete,

    lin_btn_kb_up,
    lin_btn_kb_down,
    lin_btn_kb_left,
    lin_btn_kb_right,

    lin_btn_kb_shift,
    lin_btn_kb_ctrl,
    lin_btn_kb_alt,
    lin_btn_kb_super, // Windows / Command

    lin_btn_kb_a,
    lin_btn_kb_b,
    lin_btn_kb_c,
    lin_btn_kb_d,
    lin_btn_kb_e,
    lin_btn_kb_f,
    lin_btn_kb_g,
    lin_btn_kb_h,
    lin_btn_kb_i,
    lin_btn_kb_j,
    lin_btn_kb_k,
    lin_btn_kb_l,
    lin_btn_kb_m,
    lin_btn_kb_n,
    lin_btn_kb_o,
    lin_btn_kb_p,
    lin_btn_kb_q,
    lin_btn_kb_r,
    lin_btn_kb_s,
    lin_btn_kb_t,
    lin_btn_kb_u,
    lin_btn_kb_v,
    lin_btn_kb_w,
    lin_btn_kb_x,
    lin_btn_kb_y,
    lin_btn_kb_z,

    lin_btn_kb_0,
    lin_btn_kb_1,
    lin_btn_kb_2,
    lin_btn_kb_3,
    lin_btn_kb_4,
    lin_btn_kb_5,
    lin_btn_kb_6,
    lin_btn_kb_7,
    lin_btn_kb_8,
    lin_btn_kb_9,

    // GAMEPAD

    // face buttons
    lin_btn_gp_north,
    lin_btn_gp_east,
    lin_btn_gp_south,
    lin_btn_gp_west,

    // shoulder buttons
    lin_btn_gp_lb,
    lin_btn_gp_rb,

    // triggers as buttons
    //  some controllers does not use those - use in alternative to analog
    lin_btn_gp_lt,
    lin_btn_gp_rt,

    // sticks buttons
    lin_btn_gp_l3,
    lin_btn_gp_r3,

    // menu buttons
    lin_btn_gp_start,
    lin_btn_gp_select,
    lin_btn_gp_home,

    // dpad buttons
    lin_btn_gp_dpad_up,
    lin_btn_gp_dpad_right,
    lin_btn_gp_dpad_down,
    lin_btn_gp_dpad_left,

    lin_btn_count,
} lin_button;

typedef enum lin_axis {
    // Mouse
    lin_axis_ms_x,
    lin_axis_ms_y,

    // GAMEPAD
    lin_axis_gp_lx,   // left  stick  X  -1..+1
    lin_axis_gp_ly,   // left  stick  Y  -1..+1
    lin_axis_gp_rx,   // right stick  X  -1..+1
    lin_axis_gp_ry,   // right stick  Y  -1..+1
    lin_axis_gp_lt,   // left  trigger    0..+1 
    lin_axis_gp_rt,   // right trigger    0..+1 

    lin_axis_count,
} lin_axis;

typedef struct lin_input_state {
    char  buttons[lin_btn_count];
    float axes   [lin_axis_count];
} lin_input_state;

#endif // LIGHT_INPUT_H

#ifdef LIGHT_INPUT_IMPL
#ifdef LIGHT_INPUT_LINUX

/*
    The caller needs read access to /dev/input/event* devices.
    Running as root or adding the user to the `input` group is sufficient.
*/

#include <libevdev/libevdev.h>
#include <linux/input.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
    Config
*/

static const float mouse_inner_sensitivity_factor = 1;
static const float mouse_inner_damping_factor     = 0.99;

/*
    Mappings and Helpers
*/

static const unsigned int button_mapping[lin_btn_count] = {
    // Mouse
    [lin_btn_ms_left]       = BTN_LEFT,
    [lin_btn_ms_right]      = BTN_RIGHT,
    [lin_btn_ms_middle]     = BTN_MIDDLE,
    [lin_btn_ms_back]       = BTN_SIDE,
    [lin_btn_ms_forward]    = BTN_EXTRA,

    // Keyboard
    [lin_btn_kb_escape]     = KEY_ESC,
    [lin_btn_kb_tab]        = KEY_TAB,
    [lin_btn_kb_enter]      = KEY_ENTER,
    [lin_btn_kb_space]      = KEY_SPACE,
    [lin_btn_kb_backspace]  = KEY_BACKSPACE,
    [lin_btn_kb_delete]     = KEY_DELETE,
    [lin_btn_kb_up]         = KEY_UP,
    [lin_btn_kb_down]       = KEY_DOWN,
    [lin_btn_kb_left]       = KEY_LEFT,
    [lin_btn_kb_right]      = KEY_RIGHT,

    [lin_btn_kb_shift]      = KEY_LEFTSHIFT,
    [lin_btn_kb_ctrl]       = KEY_LEFTCTRL,
    [lin_btn_kb_alt]        = KEY_LEFTALT,
    [lin_btn_kb_super]      = KEY_LEFTMETA,

    [lin_btn_kb_a]          = KEY_A,
    [lin_btn_kb_b]          = KEY_B,
    [lin_btn_kb_c]          = KEY_C,
    [lin_btn_kb_d]          = KEY_D,
    [lin_btn_kb_e]          = KEY_E,
    [lin_btn_kb_f]          = KEY_F,
    [lin_btn_kb_g]          = KEY_G,
    [lin_btn_kb_h]          = KEY_H,
    [lin_btn_kb_i]          = KEY_I,
    [lin_btn_kb_j]          = KEY_J,
    [lin_btn_kb_k]          = KEY_K,
    [lin_btn_kb_l]          = KEY_L,
    [lin_btn_kb_m]          = KEY_M,
    [lin_btn_kb_n]          = KEY_N,
    [lin_btn_kb_o]          = KEY_O,
    [lin_btn_kb_p]          = KEY_P,
    [lin_btn_kb_q]          = KEY_Q,
    [lin_btn_kb_r]          = KEY_R,
    [lin_btn_kb_s]          = KEY_S,
    [lin_btn_kb_t]          = KEY_T,
    [lin_btn_kb_u]          = KEY_U,
    [lin_btn_kb_v]          = KEY_V,
    [lin_btn_kb_w]          = KEY_W,
    [lin_btn_kb_x]          = KEY_X,
    [lin_btn_kb_y]          = KEY_Y,
    [lin_btn_kb_z]          = KEY_Z,

    [lin_btn_kb_0]          = KEY_0,
    [lin_btn_kb_1]          = KEY_1,
    [lin_btn_kb_2]          = KEY_2,
    [lin_btn_kb_3]          = KEY_3,
    [lin_btn_kb_4]          = KEY_4,
    [lin_btn_kb_5]          = KEY_5,
    [lin_btn_kb_6]          = KEY_6,
    [lin_btn_kb_7]          = KEY_7,
    [lin_btn_kb_8]          = KEY_8,
    [lin_btn_kb_9]          = KEY_9,

    // Gamepad
    [lin_btn_gp_north]      = BTN_NORTH,
    [lin_btn_gp_east]       = BTN_EAST,
    [lin_btn_gp_south]      = BTN_SOUTH,
    [lin_btn_gp_west]       = BTN_WEST,

    [lin_btn_gp_lb]         = BTN_TL,
    [lin_btn_gp_rb]         = BTN_TR,
    [lin_btn_gp_lt]         = BTN_TL2,
    [lin_btn_gp_rt]         = BTN_TR2,

    [lin_btn_gp_l3]         = BTN_THUMBL,
    [lin_btn_gp_r3]         = BTN_THUMBR,

    [lin_btn_gp_start]      = BTN_START,
    [lin_btn_gp_select]     = BTN_SELECT,
    [lin_btn_gp_home]       = BTN_MODE,

    [lin_btn_gp_dpad_up]    = BTN_DPAD_UP,
    [lin_btn_gp_dpad_down]  = BTN_DPAD_DOWN,
    [lin_btn_gp_dpad_left]  = BTN_DPAD_LEFT,
    [lin_btn_gp_dpad_right] = BTN_DPAD_RIGHT,
};

// Some controllers expose HAT0 axes instead of BTN_DPAD buttons
typedef struct { lin_button btn; unsigned int code; unsigned int pressed_when_positve; } dpad_axis_map;
static const dpad_axis_map dpad_key_abs_hat_fallback_mapping[] = {
    { lin_btn_gp_dpad_up,    ABS_HAT0Y, 0 },
    { lin_btn_gp_dpad_down,  ABS_HAT0Y, 1 },
    { lin_btn_gp_dpad_left,  ABS_HAT0X, 0 },
    { lin_btn_gp_dpad_right, ABS_HAT0X, 1 }
};

// Right-hand modifier companions (OR-ed with left-hand counterpart)
typedef struct { lin_button btn; unsigned int right_code; } modifier_pair;
static const modifier_pair modifier_pairs_mapping[] = {
    { lin_btn_kb_shift, KEY_RIGHTSHIFT },
    { lin_btn_kb_ctrl,  KEY_RIGHTCTRL  },
    { lin_btn_kb_alt,   KEY_RIGHTALT   },
    { lin_btn_kb_super, KEY_RIGHTMETA  }
};

// Axis mapping: lin_axis to EV_ABS code
typedef struct { lin_axis axis; unsigned int code; } axis_mapping_entry;
static const axis_mapping_entry axis_mapping[lin_axis_count] = {
    // Gamepad
    { lin_axis_gp_lx, ABS_X  },
    { lin_axis_gp_ly, ABS_Y  },
    { lin_axis_gp_rx, ABS_RX },
    { lin_axis_gp_ry, ABS_RY },
    { lin_axis_gp_lt, ABS_Z  },
    { lin_axis_gp_rt, ABS_RZ },
};

static float apply_deadzone(float v, float deadzone) {
    float sign   = (v < 0.0f) ? -1.0f : 1.0f;
    float abs_v  = v * sign;

    if (abs_v < deadzone) return 0.0f;
    float scaled = (abs_v - deadzone) / (1.0f - deadzone);

    return sign * (scaled > 1.0f ? 1.0f : scaled);
}

// Normalize a raw absolute axis value
static float normalize_abs(const struct input_absinfo* info, int raw, float deadzone) {
    if (!info || info->maximum == info->minimum) return 0.0f;
    
    // Stick: map [min, max] to [-1, +1]
    if (info->minimum < 0) {
        float v = (2.0f *  (float)(raw - info->minimum) / (float)(info->maximum - info->minimum)) - 1.0f;
        return apply_deadzone(v, deadzone);
    }
    // Trigger: map [min, max] to [0, +1]
    else {
        float v = (float)(raw - info->minimum) / (float)(info->maximum - info->minimum);
        return apply_deadzone(v, deadzone);
    }
}

static lin_device_type classify_device(struct libevdev* dev) {
    // Gamepad: has at least one face button from the gamepad cluster
    if (libevdev_has_event_code(dev, EV_KEY, BTN_SOUTH) ||
        libevdev_has_event_code(dev, EV_KEY, BTN_GAMEPAD))
        return lin_device_gamepad;

    // Mouse: relative pointer movement + primary click
    if (libevdev_has_event_code(dev, EV_KEY, BTN_LEFT) &&
        libevdev_has_event_code(dev, EV_REL, REL_X))
        return lin_device_mouse;

    // Keyboard: alphanumeric keys present
    if (libevdev_has_event_code(dev, EV_KEY, KEY_A) &&
        libevdev_has_event_code(dev, EV_KEY, KEY_SPACE))
        return lin_device_keyboard;

    return lin_device_unknown;
}

/*
    Library
*/

struct lin_library {
    int _placeholder;
};

lin_library* lin_create_library(void) {
    return calloc(1, sizeof(lin_library));
}

void lin_free_library(lin_library* lib) {
    free(lib);
}

/*
    Devices List
*/

typedef struct {
    char            path[256];
    char            name[256];
    lin_device_type type;
} device_info;

struct lin_devices_list {
    device_info*    entries;
    size_t          count;
    size_t          capacity;
};

lin_devices_list* lin_library_get_devices_list(lin_library* lib) {
    (void)lib;

    lin_devices_list* list = calloc(1, sizeof(*list));
    if (!list) return NULL;

    list->capacity = 16;
    list->entries  = malloc(list->capacity*  sizeof(device_info));
    if (!list->entries) { free(list); return NULL; }

    DIR* dir = opendir("/dev/input");
    if (!dir) return list; // return empty list, not NULL

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;

        char path[256]; {
            memcpy(path, "/dev/input/", 11);
            int i = 11; int j = 0;
            while (ent->d_name[j] != '\0' && i != 255) path[i++] = ent->d_name[j++];
            path[i] = '\0';
        }

        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;

        struct libevdev* dev = NULL;
        if (libevdev_new_from_fd(fd, &dev) < 0) {
            close(fd);
            continue;
        }

        // Grow the array if needed
        if (list->count == list->capacity) {
            list->capacity *= 2;
            device_info* tmp = realloc(list->entries, list->capacity*  sizeof(device_info));
            if (!tmp) { libevdev_free(dev); close(fd); break; }
            list->entries = tmp;
        }

        device_info* info = &list->entries[list->count++];
        memset(info, 0, sizeof(*info));
        strncpy(info->path, path, sizeof(info->path) - 1);

        const char* name = libevdev_get_name(dev);
        strncpy(info->name, name ? name : "(unnamed)", sizeof(info->name) - 1);

        info->type = classify_device(dev);

        libevdev_free(dev);
        close(fd);
    }

    closedir(dir);
    return list;
}

void lin_free_devices_list(lin_devices_list* list) {
    if (!list) return;
    free(list->entries);
    free(list);
}

size_t lin_devices_list_get_size(lin_devices_list* list) {
    return list->count;
}

lin_device_type lin_devices_list_query_type(lin_devices_list* list, size_t idx) {
    if (idx >= list->count) return lin_device_unknown;
    return list->entries[idx].type;
}

const char* lin_devices_list_query_name(lin_devices_list* list, size_t idx) {
    if (idx >= list->count) return NULL;
    return list->entries[idx].name;
}

/*
    Slot
*/

struct lin_slot {
    struct libevdev*    dev;
    int                 connected;
    int                 fd;
    float               mouse_sensitivity;
    float               axis_deadzone[lin_axis_count];

    // process variables
    float mouse_dx;
    float mouse_dy;
};

lin_slot* lin_create_slot(lin_library* lib) {
    (void)lib;

    lin_slot* slot = calloc(1, sizeof(*slot));
    if (!slot) return NULL;

    slot->fd                = -1;
    slot->mouse_sensitivity =  1; // default mouse sensitivity

    return slot;
}

void lin_free_slot(lin_slot* slot) {
    if (!slot) return;
    lin_slot_disconnect(slot);
    free(slot);
}

void lin_slot_disconnect(lin_slot* slot) {
    if (slot->dev) { libevdev_free(slot->dev); slot->dev = NULL; }
    if (slot->fd >= 0) { close(slot->fd); slot->fd = -1; }
    slot->connected = 0;
}

int lin_slot_connect(lin_slot* slot, lin_devices_list* list, size_t idx) {
    if (idx >= list->count) return 0;
    lin_slot_disconnect(slot); // drop any existing slot first

    const char* path = list->entries[idx].path;
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return 0;

    struct libevdev* dev = NULL;
    if (libevdev_new_from_fd(fd, &dev) < 0) {
        close(fd);
        return 0;
    }

    slot->dev       = dev;
    slot->fd        = fd;
    slot->connected = 1;

    slot->mouse_dx = 0.0f;
    slot->mouse_dy = 0.0f;

    return 1;
}

int lin_slot_connected(lin_slot* slot) {
    return slot->connected;
}

void lin_slot_set_mouse_sensitivity(lin_slot* slot, float sensitivity) {
    slot->mouse_sensitivity = sensitivity;
}

void lin_slot_set_axis_deadzone(lin_slot* slot, unsigned int axis, float deadzone) {
    if (deadzone < 0.0f) deadzone = 0.0f;
    if (deadzone > 1.0f) deadzone = 1.0f;
    slot->axis_deadzone[axis] = deadzone;
}

/*
    Input state
*/

// Drain all pending events so libevdev's internal state mirror is current.
//  Returns 0 on success, -ENODEV if the device was physically removed.
// Also pull mouse position delta
static int drain_events(lin_slot* slot, struct libevdev* dev) {
    struct input_event ev; int rc;

    for (;;) {
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

        if (rc == LIBEVDEV_READ_STATUS_SUCCESS ||
            rc == LIBEVDEV_READ_STATUS_SYNC) {

            if (ev.type == EV_REL) {
                if (ev.code == REL_X) 
                    slot->mouse_dx += ev.value;
                if (ev.code == REL_Y)
                    slot->mouse_dy += ev.value;
            }
        }

        if (rc == -EAGAIN) return 0;
        if (rc == -ENODEV) return -ENODEV;
    }
}

void lin_slot_query_input_state(lin_slot* slot, lin_input_state* st) {
    *st = (lin_input_state){0}; // 0 init
    if (!slot || !slot->connected || !slot->dev) return;

    struct libevdev* dev = slot->dev;

    // drain pending events, on ENODEV the device was hot-unplugged: mark as disconnected
    if (drain_events(slot, dev) == -ENODEV) {
        slot->connected = 0;
        return;
    }

    // Buttons
    for (int i = 0; i < lin_btn_count; i++) {
        const unsigned int code = button_mapping[i];
        if (libevdev_has_event_code(dev, EV_KEY, code)) st->buttons[i] = libevdev_get_event_value(dev, EV_KEY, code);
    }

    // HAT0 axis fallback for dpad buttons, in case gamepad does not have them
    for (size_t i = 0; i < sizeof(dpad_key_abs_hat_fallback_mapping) / sizeof(dpad_axis_map); i++) {
        dpad_axis_map map = dpad_key_abs_hat_fallback_mapping[i];
        if (st->buttons[map.btn]) continue; // if already pressed, ignore

        // query axis state
        if (libevdev_has_event_code(dev, EV_ABS, map.code)) {
            int raw = libevdev_get_event_value(dev, EV_ABS, map.code);
            st->buttons[map.btn] = (map.pressed_when_positve) ? (raw > 0) : (raw < 0);
        }
    }

    // OR in right-hand modifier keys so either side registers as pressed
    for (size_t i = 0; i < sizeof(modifier_pairs_mapping) / sizeof(*modifier_pairs_mapping); i++) {
        lin_button btn      = modifier_pairs_mapping[i].btn;
        unsigned int rcode  = modifier_pairs_mapping[i].right_code;

        if (!st->buttons[btn] && libevdev_has_event_code(dev, EV_KEY, rcode)) 
            st->buttons[btn] = libevdev_get_event_value(dev, EV_KEY, rcode);
    }

    // Axes
    for (int i = 0; i < lin_axis_count; i++) {
        unsigned int code = axis_mapping[i].code;
        if (libevdev_has_event_code(dev, EV_ABS, code)) {
            int raw = libevdev_get_event_value(dev, EV_ABS, code);
            st->axes[axis_mapping[i].axis] = normalize_abs(libevdev_get_abs_info(dev, code), raw, slot->axis_deadzone[i]);
        }
    }

    // Mouse Axes
    float*   delta_var[] = { &slot->mouse_dx,   &slot->mouse_dy };
    lin_axis axes[]      = { lin_axis_ms_x,     lin_axis_ms_y   };
    for (int i = 0; i < 2; i++) {
        lin_axis axis = axes[i];

        st->axes[axis] = *delta_var[i] * mouse_inner_sensitivity_factor * slot->mouse_sensitivity;
        if (st->axes[axis] > 1.0f)  st->axes[axis] = 1.0f;
        if (st->axes[axis] < -1.0f) st->axes[axis] = -1.0f;
        st->axes[axis] = apply_deadzone(st->axes[axis], slot->axis_deadzone[axis]);

        *delta_var[i] *= mouse_inner_damping_factor;
    }
}
#else
    #error No OS info provided for light input!
#endif // OS IF
#endif // LIGHT_INPUT_IMPL
