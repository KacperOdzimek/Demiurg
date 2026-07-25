/*
----------------------------------------------------------------
Contents:
This file implements input system, allowing user to read input from mouses, keyboards and gamepads.

----------------------------------------------------------------
Code info:
- din prefix
- DEMIURG_INPUTS_IMPL macro to build
- User must pick target OS system by using of the macros below:
    - DEMIURG_INPUTS_LINUX

----------------------------------------------------------------
Depedencies:
- each OS have own compilation requirements:
    - DEMIURG_INPUTS_LINUX
        - libevdev library installed

----------------------------------------------------------------
Usage: See dedicated documentation
*/

#ifndef DEMIURG_INPUTS_H
#define DEMIURG_INPUTS_H

#include <stddef.h>

// Library

typedef struct din_library din_library;

din_library* din_create_library();
void din_free_library(din_library*);

// Devices List

typedef struct din_devices_list din_devices_list;

// get OS visible devices at the moment
// may return NULL if failed to access
din_devices_list* din_library_get_devices_list(din_library*);
void din_free_devices_list(din_devices_list*);

typedef enum din_device_type {
    din_device_unknown,
    din_device_mouse,
    din_device_keyboard,
    din_device_gamepad,
} din_device_type;

size_t          din_devices_list_get_size   (din_devices_list*);
din_device_type din_devices_list_query_type (din_devices_list*, size_t dev_index);
const char*     din_devices_list_query_name (din_devices_list*, size_t dev_index);

// Slot

typedef struct din_slot din_slot;

din_slot* din_create_slot(din_library*);
void din_free_slot(din_slot*);

// tries to connect to device, 1 at success, 0 at failure
int  din_slot_connect(din_slot*, din_devices_list*, size_t dev_index);

// drops device connection
void din_slot_disconnect(din_slot*);

// returns 1 if connected, 0 if never connected or lost connection
int din_slot_connected(din_slot*);

// set conversion factor for translation mouse movement -> mouse axis
void din_slot_set_mouse_sensitivity(din_slot*, float sensitivity);

// deadzone shall be between [0, 1]
// 0 means no deadzone, 1 means binary state, either nothing or full press
// if other value is set, till that value, din_slot_query_input_state[axis] will return 0.0f
// afterwards that limit, axis will interpolate towards full press
void din_slot_set_axis_deadzone(din_slot*, unsigned int axis, float deadzone);

typedef struct din_input_state din_input_state;
// get current connected device input state
// unconnected device always yields input state, with everything 0
void din_slot_query_input_state(din_slot*, din_input_state*);

// Input State

typedef enum din_button {
    // Mouse (ms)
    din_btn_ms_left,
    din_btn_ms_right,
    din_btn_ms_middle,
    din_btn_ms_back,
    din_btn_ms_forward,
    
    // Keyboard (kb)
    din_btn_kb_escape,
    din_btn_kb_tab,
    din_btn_kb_enter,
    din_btn_kb_space,
    din_btn_kb_backspace,
    din_btn_kb_delete,

    din_btn_kb_up,
    din_btn_kb_down,
    din_btn_kb_left,
    din_btn_kb_right,

    din_btn_kb_shift,
    din_btn_kb_ctrl,
    din_btn_kb_alt,
    din_btn_kb_super, // Windows / Command

    din_btn_kb_a,
    din_btn_kb_b,
    din_btn_kb_c,
    din_btn_kb_d,
    din_btn_kb_e,
    din_btn_kb_f,
    din_btn_kb_g,
    din_btn_kb_h,
    din_btn_kb_i,
    din_btn_kb_j,
    din_btn_kb_k,
    din_btn_kb_l,
    din_btn_kb_m,
    din_btn_kb_n,
    din_btn_kb_o,
    din_btn_kb_p,
    din_btn_kb_q,
    din_btn_kb_r,
    din_btn_kb_s,
    din_btn_kb_t,
    din_btn_kb_u,
    din_btn_kb_v,
    din_btn_kb_w,
    din_btn_kb_x,
    din_btn_kb_y,
    din_btn_kb_z,

    din_btn_kb_0,
    din_btn_kb_1,
    din_btn_kb_2,
    din_btn_kb_3,
    din_btn_kb_4,
    din_btn_kb_5,
    din_btn_kb_6,
    din_btn_kb_7,
    din_btn_kb_8,
    din_btn_kb_9,

    // GAMEPAD

    // face buttons
    din_btn_gp_north,
    din_btn_gp_east,
    din_btn_gp_south,
    din_btn_gp_west,

    // shoulder buttons
    din_btn_gp_lb,
    din_btn_gp_rb,

    // triggers as buttons
    //  some controllers does not use those - use in alternative to analog
    din_btn_gp_lt,
    din_btn_gp_rt,

    // sticks buttons
    din_btn_gp_l3,
    din_btn_gp_r3,

    // menu buttons
    din_btn_gp_start,
    din_btn_gp_select,
    din_btn_gp_home,

    // dpad buttons
    din_btn_gp_dpad_up,
    din_btn_gp_dpad_right,
    din_btn_gp_dpad_down,
    din_btn_gp_dpad_left,

    din_btn_count,
} din_button;

typedef enum din_axis {
    // Mouse
    din_axis_ms_x,
    din_axis_ms_y,

    // GAMEPAD
    din_axis_gp_lx,   // left  stick  X  -1..+1
    din_axis_gp_ly,   // left  stick  Y  -1..+1
    din_axis_gp_rx,   // right stick  X  -1..+1
    din_axis_gp_ry,   // right stick  Y  -1..+1
    din_axis_gp_lt,   // left  trigger    0..+1 
    din_axis_gp_rt,   // right trigger    0..+1 

    din_axis_count,
} din_axis;

typedef struct din_input_state {
    char  buttons[din_btn_count];
    float axes   [din_axis_count];
} din_input_state;

#endif // DEMIURG_INPUTS_H

#ifdef DEMIURG_INPUTS_IMPL
#ifdef DEMIURG_INPUTS_LINUX

/*
    The caller needs read access to /dev/input/event* devices.
    Running as root or adding the user to the `input` group is sufficient.
*/

#include <libevdev/libevdev.h>
#include <dinux/input.h>

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

static const unsigned int button_mapping[din_btn_count] = {
    // Mouse
    [din_btn_ms_left]       = BTN_LEFT,
    [din_btn_ms_right]      = BTN_RIGHT,
    [din_btn_ms_middle]     = BTN_MIDDLE,
    [din_btn_ms_back]       = BTN_SIDE,
    [din_btn_ms_forward]    = BTN_EXTRA,

    // Keyboard
    [din_btn_kb_escape]     = KEY_ESC,
    [din_btn_kb_tab]        = KEY_TAB,
    [din_btn_kb_enter]      = KEY_ENTER,
    [din_btn_kb_space]      = KEY_SPACE,
    [din_btn_kb_backspace]  = KEY_BACKSPACE,
    [din_btn_kb_delete]     = KEY_DELETE,
    [din_btn_kb_up]         = KEY_UP,
    [din_btn_kb_down]       = KEY_DOWN,
    [din_btn_kb_left]       = KEY_LEFT,
    [din_btn_kb_right]      = KEY_RIGHT,

    [din_btn_kb_shift]      = KEY_LEFTSHIFT,
    [din_btn_kb_ctrl]       = KEY_LEFTCTRL,
    [din_btn_kb_alt]        = KEY_LEFTALT,
    [din_btn_kb_super]      = KEY_LEFTMETA,

    [din_btn_kb_a]          = KEY_A,
    [din_btn_kb_b]          = KEY_B,
    [din_btn_kb_c]          = KEY_C,
    [din_btn_kb_d]          = KEY_D,
    [din_btn_kb_e]          = KEY_E,
    [din_btn_kb_f]          = KEY_F,
    [din_btn_kb_g]          = KEY_G,
    [din_btn_kb_h]          = KEY_H,
    [din_btn_kb_i]          = KEY_I,
    [din_btn_kb_j]          = KEY_J,
    [din_btn_kb_k]          = KEY_K,
    [din_btn_kb_l]          = KEY_L,
    [din_btn_kb_m]          = KEY_M,
    [din_btn_kb_n]          = KEY_N,
    [din_btn_kb_o]          = KEY_O,
    [din_btn_kb_p]          = KEY_P,
    [din_btn_kb_q]          = KEY_Q,
    [din_btn_kb_r]          = KEY_R,
    [din_btn_kb_s]          = KEY_S,
    [din_btn_kb_t]          = KEY_T,
    [din_btn_kb_u]          = KEY_U,
    [din_btn_kb_v]          = KEY_V,
    [din_btn_kb_w]          = KEY_W,
    [din_btn_kb_x]          = KEY_X,
    [din_btn_kb_y]          = KEY_Y,
    [din_btn_kb_z]          = KEY_Z,

    [din_btn_kb_0]          = KEY_0,
    [din_btn_kb_1]          = KEY_1,
    [din_btn_kb_2]          = KEY_2,
    [din_btn_kb_3]          = KEY_3,
    [din_btn_kb_4]          = KEY_4,
    [din_btn_kb_5]          = KEY_5,
    [din_btn_kb_6]          = KEY_6,
    [din_btn_kb_7]          = KEY_7,
    [din_btn_kb_8]          = KEY_8,
    [din_btn_kb_9]          = KEY_9,

    // Gamepad
    [din_btn_gp_north]      = BTN_NORTH,
    [din_btn_gp_east]       = BTN_EAST,
    [din_btn_gp_south]      = BTN_SOUTH,
    [din_btn_gp_west]       = BTN_WEST,

    [din_btn_gp_lb]         = BTN_TL,
    [din_btn_gp_rb]         = BTN_TR,
    [din_btn_gp_lt]         = BTN_TL2,
    [din_btn_gp_rt]         = BTN_TR2,

    [din_btn_gp_l3]         = BTN_THUMBL,
    [din_btn_gp_r3]         = BTN_THUMBR,

    [din_btn_gp_start]      = BTN_START,
    [din_btn_gp_select]     = BTN_SELECT,
    [din_btn_gp_home]       = BTN_MODE,

    [din_btn_gp_dpad_up]    = BTN_DPAD_UP,
    [din_btn_gp_dpad_down]  = BTN_DPAD_DOWN,
    [din_btn_gp_dpad_left]  = BTN_DPAD_LEFT,
    [din_btn_gp_dpad_right] = BTN_DPAD_RIGHT,
};

// Some controllers expose HAT0 axes instead of BTN_DPAD buttons
typedef struct { din_button btn; unsigned int code; unsigned int pressed_when_positve; } dpad_axis_map;
static const dpad_axis_map dpad_key_abs_hat_fallback_mapping[] = {
    { din_btn_gp_dpad_up,    ABS_HAT0Y, 0 },
    { din_btn_gp_dpad_down,  ABS_HAT0Y, 1 },
    { din_btn_gp_dpad_left,  ABS_HAT0X, 0 },
    { din_btn_gp_dpad_right, ABS_HAT0X, 1 }
};

// Right-hand modifier companions (OR-ed with left-hand counterpart)
typedef struct { din_button btn; unsigned int right_code; } modifier_pair;
static const modifier_pair modifier_pairs_mapping[] = {
    { din_btn_kb_shift, KEY_RIGHTSHIFT },
    { din_btn_kb_ctrl,  KEY_RIGHTCTRL  },
    { din_btn_kb_alt,   KEY_RIGHTALT   },
    { din_btn_kb_super, KEY_RIGHTMETA  }
};

// Axis mapping: din_axis to EV_ABS code
typedef struct { din_axis axis; unsigned int code; } axis_mapping_entry;
static const axis_mapping_entry axis_mapping[din_axis_count] = {
    // Gamepad
    { din_axis_gp_lx, ABS_X  },
    { din_axis_gp_ly, ABS_Y  },
    { din_axis_gp_rx, ABS_RX },
    { din_axis_gp_ry, ABS_RY },
    { din_axis_gp_lt, ABS_Z  },
    { din_axis_gp_rt, ABS_RZ },
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

static din_device_type classify_device(struct libevdev* dev) {
    // Gamepad: has at least one face button from the gamepad cluster
    if (libevdev_has_event_code(dev, EV_KEY, BTN_SOUTH) ||
        libevdev_has_event_code(dev, EV_KEY, BTN_GAMEPAD))
        return din_device_gamepad;

    // Mouse: relative pointer movement + primary click
    if (libevdev_has_event_code(dev, EV_KEY, BTN_LEFT) &&
        libevdev_has_event_code(dev, EV_REL, REL_X))
        return din_device_mouse;

    // Keyboard: alphanumeric keys present
    if (libevdev_has_event_code(dev, EV_KEY, KEY_A) &&
        libevdev_has_event_code(dev, EV_KEY, KEY_SPACE))
        return din_device_keyboard;

    return din_device_unknown;
}

/*
    Library
*/

struct din_library {
    int _placeholder;
};

din_library* din_create_library(void) {
    return calloc(1, sizeof(din_library));
}

void din_free_library(din_library* lib) {
    free(lib);
}

/*
    Devices List
*/

typedef struct {
    char            path[256];
    char            name[256];
    din_device_type type;
} device_info;

struct din_devices_list {
    device_info*    entries;
    size_t          count;
    size_t          capacity;
};

din_devices_list* din_library_get_devices_list(din_library* lib) {
    (void)lib;

    din_devices_list* list = calloc(1, sizeof(*list));
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

void din_free_devices_list(din_devices_list* list) {
    if (!list) return;
    free(list->entries);
    free(list);
}

size_t din_devices_list_get_size(din_devices_list* list) {
    return list->count;
}

din_device_type din_devices_list_query_type(din_devices_list* list, size_t idx) {
    if (idx >= list->count) return din_device_unknown;
    return list->entries[idx].type;
}

const char* din_devices_list_query_name(din_devices_list* list, size_t idx) {
    if (idx >= list->count) return NULL;
    return list->entries[idx].name;
}

/*
    Slot
*/

struct din_slot {
    struct libevdev*    dev;
    int                 connected;
    int                 fd;
    float               mouse_sensitivity;
    float               axis_deadzone[din_axis_count];

    // process variables
    float mouse_dx;
    float mouse_dy;
};

din_slot* din_create_slot(din_library* lib) {
    (void)lib;

    din_slot* slot = calloc(1, sizeof(*slot));
    if (!slot) return NULL;

    slot->fd                = -1;
    slot->mouse_sensitivity =  1; // default mouse sensitivity

    return slot;
}

void din_free_slot(din_slot* slot) {
    if (!slot) return;
    din_slot_disconnect(slot);
    free(slot);
}

void din_slot_disconnect(din_slot* slot) {
    if (slot->dev) { libevdev_free(slot->dev); slot->dev = NULL; }
    if (slot->fd >= 0) { close(slot->fd); slot->fd = -1; }
    slot->connected = 0;
}

int din_slot_connect(din_slot* slot, din_devices_list* list, size_t idx) {
    if (idx >= list->count) return 0;
    din_slot_disconnect(slot); // drop any existing slot first

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

int din_slot_connected(din_slot* slot) {
    return slot->connected;
}

void din_slot_set_mouse_sensitivity(din_slot* slot, float sensitivity) {
    slot->mouse_sensitivity = sensitivity;
}

void din_slot_set_axis_deadzone(din_slot* slot, unsigned int axis, float deadzone) {
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
static int drain_events(din_slot* slot, struct libevdev* dev) {
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

void din_slot_query_input_state(din_slot* slot, din_input_state* st) {
    *st = (din_input_state){0}; // 0 init
    if (!slot || !slot->connected || !slot->dev) return;

    struct libevdev* dev = slot->dev;

    // drain pending events, on ENODEV the device was hot-unplugged: mark as disconnected
    if (drain_events(slot, dev) == -ENODEV) {
        slot->connected = 0;
        return;
    }

    // Buttons
    for (int i = 0; i < din_btn_count; i++) {
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
        din_button btn      = modifier_pairs_mapping[i].btn;
        unsigned int rcode  = modifier_pairs_mapping[i].right_code;

        if (!st->buttons[btn] && libevdev_has_event_code(dev, EV_KEY, rcode)) 
            st->buttons[btn] = libevdev_get_event_value(dev, EV_KEY, rcode);
    }

    // Axes
    for (int i = 0; i < din_axis_count; i++) {
        unsigned int code = axis_mapping[i].code;
        if (libevdev_has_event_code(dev, EV_ABS, code)) {
            int raw = libevdev_get_event_value(dev, EV_ABS, code);
            st->axes[axis_mapping[i].axis] = normalize_abs(libevdev_get_abs_info(dev, code), raw, slot->axis_deadzone[i]);
        }
    }

    // Mouse Axes
    float*   delta_var[] = { &slot->mouse_dx,   &slot->mouse_dy };
    din_axis axes[]      = { din_axis_ms_x,     din_axis_ms_y   };
    for (int i = 0; i < 2; i++) {
        din_axis axis = axes[i];

        st->axes[axis] = *delta_var[i] * mouse_inner_sensitivity_factor * slot->mouse_sensitivity;
        if (st->axes[axis] > 1.0f)  st->axes[axis] = 1.0f;
        if (st->axes[axis] < -1.0f) st->axes[axis] = -1.0f;
        st->axes[axis] = apply_deadzone(st->axes[axis], slot->axis_deadzone[axis]);

        *delta_var[i] *= mouse_inner_damping_factor;
    }
}
#else
    #error No OS info provided for demiurg inputs!
#endif // OS IF
#endif // DEMIURG_INPUTS_IMPL
