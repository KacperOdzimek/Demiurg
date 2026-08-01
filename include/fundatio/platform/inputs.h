/*
----------------------------------------------------------------
Contents:
This file implements input system, allowing user to read input from mouses, keyboards and gamepads.

----------------------------------------------------------------
Code info:
- fnd_inp prefix
- FUNDATIO_INPUTS_IMPL macro to build
- User must pick target OS system by using of the macros below:
    - FUNDATIO_INPUTS_LINUX
    - FUNDATIO_INPUTS_WINDOWS

----------------------------------------------------------------
Depedencies:
- each OS have own compilation requirements:
    - FUNDATIO_INPUTS_LINUX
        - libevdev library installed
    - FUNDATIO_INPUTS_WINDOWS
        - windows sdk installed
        - xinput library installed

----------------------------------------------------------------
Usage: See dedicated documentation
*/

#ifndef FUNDATIO_INPUTS_H
#define FUNDATIO_INPUTS_H

#include <stddef.h>

// Library

typedef struct fnd_inp_library fnd_inp_library;

fnd_inp_library* fnd_inp_create_library();
void fnd_inp_free_library(fnd_inp_library*);

// Devices List

typedef struct fnd_inp_devices_list fnd_inp_devices_list;

// get OS visible devices at the moment
// may return NULL if failed to access
fnd_inp_devices_list* fnd_inp_library_get_devices_list(fnd_inp_library*);
void fnd_inp_free_devices_list(fnd_inp_devices_list*);

typedef enum fnd_inp_device_type {
    fnd_inp_device_unknown,
    fnd_inp_device_mouse,
    fnd_inp_device_keyboard,
    fnd_inp_device_gamepad,
} fnd_inp_device_type;

size_t          fnd_inp_devices_list_get_size   (fnd_inp_devices_list*);
fnd_inp_device_type fnd_inp_devices_list_query_type (fnd_inp_devices_list*, size_t dev_index);
const char*     fnd_inp_devices_list_query_name (fnd_inp_devices_list*, size_t dev_index);

// Slot

typedef struct fnd_inp_slot fnd_inp_slot;

fnd_inp_slot* fnd_inp_create_slot(fnd_inp_library*);
void fnd_inp_free_slot(fnd_inp_slot*);

// tries to connect to device, 1 at success, 0 at failure
int  fnd_inp_slot_connect(fnd_inp_slot*, fnd_inp_devices_list*, size_t dev_index);

// drops device connection
void fnd_inp_slot_disconnect(fnd_inp_slot*);

// returns 1 if connected, 0 if never connected or lost connection
int fnd_inp_slot_connected(fnd_inp_slot*);

// set conversion factor for translation mouse movement -> mouse axis
void fnd_inp_slot_set_mouse_sensitivity(fnd_inp_slot*, float sensitivity);

// deadzone shall be between [0, 1]
// 0 means no deadzone, 1 means binary state, either nothing or full press
// if other value is set, till that value, fnd_inp_slot_query_input_state[axis] will return 0.0f
// afterwards that limit, axis will interpolate towards full press
void fnd_inp_slot_set_axis_deadzone(fnd_inp_slot*, unsigned int axis, float deadzone);

typedef struct fnd_inp_input_state fnd_inp_input_state;
// get current connected device input state
// unconnected device always yields input state, with everything 0
void fnd_inp_slot_query_input_state(fnd_inp_slot*, fnd_inp_input_state*);

// Input State

typedef enum fnd_inp_button {
    // Mouse (ms)
    fnd_inp_btn_ms_left,
    fnd_inp_btn_ms_right,
    fnd_inp_btn_ms_middle,
    fnd_inp_btn_ms_back,
    fnd_inp_btn_ms_forward,
    
    // Keyboard (kb)
    fnd_inp_btn_kb_escape,
    fnd_inp_btn_kb_tab,
    fnd_inp_btn_kb_enter,
    fnd_inp_btn_kb_space,
    fnd_inp_btn_kb_backspace,
    fnd_inp_btn_kb_delete,

    fnd_inp_btn_kb_up,
    fnd_inp_btn_kb_down,
    fnd_inp_btn_kb_left,
    fnd_inp_btn_kb_right,

    fnd_inp_btn_kb_shift,
    fnd_inp_btn_kb_ctrl,
    fnd_inp_btn_kb_alt,
    fnd_inp_btn_kb_super, // Windows / Command

    fnd_inp_btn_kb_a,
    fnd_inp_btn_kb_b,
    fnd_inp_btn_kb_c,
    fnd_inp_btn_kb_d,
    fnd_inp_btn_kb_e,
    fnd_inp_btn_kb_f,
    fnd_inp_btn_kb_g,
    fnd_inp_btn_kb_h,
    fnd_inp_btn_kb_i,
    fnd_inp_btn_kb_j,
    fnd_inp_btn_kb_k,
    fnd_inp_btn_kb_l,
    fnd_inp_btn_kb_m,
    fnd_inp_btn_kb_n,
    fnd_inp_btn_kb_o,
    fnd_inp_btn_kb_p,
    fnd_inp_btn_kb_q,
    fnd_inp_btn_kb_r,
    fnd_inp_btn_kb_s,
    fnd_inp_btn_kb_t,
    fnd_inp_btn_kb_u,
    fnd_inp_btn_kb_v,
    fnd_inp_btn_kb_w,
    fnd_inp_btn_kb_x,
    fnd_inp_btn_kb_y,
    fnd_inp_btn_kb_z,

    fnd_inp_btn_kb_0,
    fnd_inp_btn_kb_1,
    fnd_inp_btn_kb_2,
    fnd_inp_btn_kb_3,
    fnd_inp_btn_kb_4,
    fnd_inp_btn_kb_5,
    fnd_inp_btn_kb_6,
    fnd_inp_btn_kb_7,
    fnd_inp_btn_kb_8,
    fnd_inp_btn_kb_9,

    // GAMEPAD

    // face buttons
    fnd_inp_btn_gp_north,
    fnd_inp_btn_gp_east,
    fnd_inp_btn_gp_south,
    fnd_inp_btn_gp_west,

    // shoulder buttons
    fnd_inp_btn_gp_lb,
    fnd_inp_btn_gp_rb,

    // triggers as buttons
    //  some controllers does not use those - use in alternative to analog
    fnd_inp_btn_gp_lt,
    fnd_inp_btn_gp_rt,

    // sticks buttons
    fnd_inp_btn_gp_l3,
    fnd_inp_btn_gp_r3,

    // menu buttons
    fnd_inp_btn_gp_start,
    fnd_inp_btn_gp_select,
    fnd_inp_btn_gp_home,

    // dpad buttons
    fnd_inp_btn_gp_dpad_up,
    fnd_inp_btn_gp_dpad_right,
    fnd_inp_btn_gp_dpad_down,
    fnd_inp_btn_gp_dpad_left,

    fnd_inp_btn_count,
} fnd_inp_button;

typedef enum fnd_inp_axis {
    // Mouse
    fnd_inp_axis_ms_x,
    fnd_inp_axis_ms_y,

    // GAMEPAD
    fnd_inp_axis_gp_lx,   // left  stick  X  -1..+1
    fnd_inp_axis_gp_ly,   // left  stick  Y  -1..+1
    fnd_inp_axis_gp_rx,   // right stick  X  -1..+1
    fnd_inp_axis_gp_ry,   // right stick  Y  -1..+1
    fnd_inp_axis_gp_lt,   // left  trigger    0..+1 
    fnd_inp_axis_gp_rt,   // right trigger    0..+1 

    fnd_inp_axis_count,
} fnd_inp_axis;

typedef struct fnd_inp_input_state {
    char  buttons[fnd_inp_btn_count];
    float axes   [fnd_inp_axis_count];
} fnd_inp_input_state;

#endif // FUNDATIO_INPUTS_H

#ifdef FUNDATIO_INPUTS_IMPL
#if defined(FUNDATIO_INPUTS_LINUX)

/*
    The caller needs read access to /dev/input/event* devices.
    Running as root or adding the user to the `input` group is sufficient.
*/

#include <libevdev/libevdev.h>

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

static const unsigned int button_mapping[fnd_inp_btn_count] = {
    // Mouse
    [fnd_inp_btn_ms_left]       = BTN_LEFT,
    [fnd_inp_btn_ms_right]      = BTN_RIGHT,
    [fnd_inp_btn_ms_middle]     = BTN_MIDDLE,
    [fnd_inp_btn_ms_back]       = BTN_SIDE,
    [fnd_inp_btn_ms_forward]    = BTN_EXTRA,

    // Keyboard
    [fnd_inp_btn_kb_escape]     = KEY_ESC,
    [fnd_inp_btn_kb_tab]        = KEY_TAB,
    [fnd_inp_btn_kb_enter]      = KEY_ENTER,
    [fnd_inp_btn_kb_space]      = KEY_SPACE,
    [fnd_inp_btn_kb_backspace]  = KEY_BACKSPACE,
    [fnd_inp_btn_kb_delete]     = KEY_DELETE,
    [fnd_inp_btn_kb_up]         = KEY_UP,
    [fnd_inp_btn_kb_down]       = KEY_DOWN,
    [fnd_inp_btn_kb_left]       = KEY_LEFT,
    [fnd_inp_btn_kb_right]      = KEY_RIGHT,

    [fnd_inp_btn_kb_shift]      = KEY_LEFTSHIFT,
    [fnd_inp_btn_kb_ctrl]       = KEY_LEFTCTRL,
    [fnd_inp_btn_kb_alt]        = KEY_LEFTALT,
    [fnd_inp_btn_kb_super]      = KEY_LEFTMETA,

    [fnd_inp_btn_kb_a]          = KEY_A,
    [fnd_inp_btn_kb_b]          = KEY_B,
    [fnd_inp_btn_kb_c]          = KEY_C,
    [fnd_inp_btn_kb_d]          = KEY_D,
    [fnd_inp_btn_kb_e]          = KEY_E,
    [fnd_inp_btn_kb_f]          = KEY_F,
    [fnd_inp_btn_kb_g]          = KEY_G,
    [fnd_inp_btn_kb_h]          = KEY_H,
    [fnd_inp_btn_kb_i]          = KEY_I,
    [fnd_inp_btn_kb_j]          = KEY_J,
    [fnd_inp_btn_kb_k]          = KEY_K,
    [fnd_inp_btn_kb_l]          = KEY_L,
    [fnd_inp_btn_kb_m]          = KEY_M,
    [fnd_inp_btn_kb_n]          = KEY_N,
    [fnd_inp_btn_kb_o]          = KEY_O,
    [fnd_inp_btn_kb_p]          = KEY_P,
    [fnd_inp_btn_kb_q]          = KEY_Q,
    [fnd_inp_btn_kb_r]          = KEY_R,
    [fnd_inp_btn_kb_s]          = KEY_S,
    [fnd_inp_btn_kb_t]          = KEY_T,
    [fnd_inp_btn_kb_u]          = KEY_U,
    [fnd_inp_btn_kb_v]          = KEY_V,
    [fnd_inp_btn_kb_w]          = KEY_W,
    [fnd_inp_btn_kb_x]          = KEY_X,
    [fnd_inp_btn_kb_y]          = KEY_Y,
    [fnd_inp_btn_kb_z]          = KEY_Z,

    [fnd_inp_btn_kb_0]          = KEY_0,
    [fnd_inp_btn_kb_1]          = KEY_1,
    [fnd_inp_btn_kb_2]          = KEY_2,
    [fnd_inp_btn_kb_3]          = KEY_3,
    [fnd_inp_btn_kb_4]          = KEY_4,
    [fnd_inp_btn_kb_5]          = KEY_5,
    [fnd_inp_btn_kb_6]          = KEY_6,
    [fnd_inp_btn_kb_7]          = KEY_7,
    [fnd_inp_btn_kb_8]          = KEY_8,
    [fnd_inp_btn_kb_9]          = KEY_9,

    // Gamepad
    [fnd_inp_btn_gp_north]      = BTN_NORTH,
    [fnd_inp_btn_gp_east]       = BTN_EAST,
    [fnd_inp_btn_gp_south]      = BTN_SOUTH,
    [fnd_inp_btn_gp_west]       = BTN_WEST,

    [fnd_inp_btn_gp_lb]         = BTN_TL,
    [fnd_inp_btn_gp_rb]         = BTN_TR,
    [fnd_inp_btn_gp_lt]         = BTN_TL2,
    [fnd_inp_btn_gp_rt]         = BTN_TR2,

    [fnd_inp_btn_gp_l3]         = BTN_THUMBL,
    [fnd_inp_btn_gp_r3]         = BTN_THUMBR,

    [fnd_inp_btn_gp_start]      = BTN_START,
    [fnd_inp_btn_gp_select]     = BTN_SELECT,
    [fnd_inp_btn_gp_home]       = BTN_MODE,

    [fnd_inp_btn_gp_dpad_up]    = BTN_DPAD_UP,
    [fnd_inp_btn_gp_dpad_down]  = BTN_DPAD_DOWN,
    [fnd_inp_btn_gp_dpad_left]  = BTN_DPAD_LEFT,
    [fnd_inp_btn_gp_dpad_right] = BTN_DPAD_RIGHT,
};

// Some controllers expose HAT0 axes instead of BTN_DPAD buttons
typedef struct { fnd_inp_button btn; unsigned int code; unsigned int pressed_when_positve; } dpad_axis_map;
static const dpad_axis_map dpad_key_abs_hat_fallback_mapping[] = {
    { fnd_inp_btn_gp_dpad_up,    ABS_HAT0Y, 0 },
    { fnd_inp_btn_gp_dpad_down,  ABS_HAT0Y, 1 },
    { fnd_inp_btn_gp_dpad_left,  ABS_HAT0X, 0 },
    { fnd_inp_btn_gp_dpad_right, ABS_HAT0X, 1 }
};

// Right-hand modifier companions (OR-ed with left-hand counterpart)
typedef struct { fnd_inp_button btn; unsigned int right_code; } modifier_pair;
static const modifier_pair modifier_pairs_mapping[] = {
    { fnd_inp_btn_kb_shift, KEY_RIGHTSHIFT },
    { fnd_inp_btn_kb_ctrl,  KEY_RIGHTCTRL  },
    { fnd_inp_btn_kb_alt,   KEY_RIGHTALT   },
    { fnd_inp_btn_kb_super, KEY_RIGHTMETA  }
};

// Axis mapping: fnd_inp_axis to EV_ABS code
typedef struct { fnd_inp_axis axis; unsigned int code; } axis_mapping_entry;
static const axis_mapping_entry axis_mapping[fnd_inp_axis_count] = {
    // Gamepad
    { fnd_inp_axis_gp_lx, ABS_X  },
    { fnd_inp_axis_gp_ly, ABS_Y  },
    { fnd_inp_axis_gp_rx, ABS_RX },
    { fnd_inp_axis_gp_ry, ABS_RY },
    { fnd_inp_axis_gp_lt, ABS_Z  },
    { fnd_inp_axis_gp_rt, ABS_RZ },
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

static fnd_inp_device_type classify_device(struct libevdev* dev) {
    // Gamepad: has at least one face button from the gamepad cluster
    if (libevdev_has_event_code(dev, EV_KEY, BTN_SOUTH) ||
        libevdev_has_event_code(dev, EV_KEY, BTN_GAMEPAD))
        return fnd_inp_device_gamepad;

    // Mouse: relative pointer movement + primary click
    if (libevdev_has_event_code(dev, EV_KEY, BTN_LEFT) &&
        libevdev_has_event_code(dev, EV_REL, REL_X))
        return fnd_inp_device_mouse;

    // Keyboard: alphanumeric keys present
    if (libevdev_has_event_code(dev, EV_KEY, KEY_A) &&
        libevdev_has_event_code(dev, EV_KEY, KEY_SPACE))
        return fnd_inp_device_keyboard;

    return fnd_inp_device_unknown;
}

/*
    Library
*/

struct fnd_inp_library {
    int _placeholder;
};

fnd_inp_library* fnd_inp_create_library(void) {
    return calloc(1, sizeof(fnd_inp_library));
}

void fnd_inp_free_library(fnd_inp_library* lib) {
    free(lib);
}

/*
    Devices List
*/

typedef struct {
    char            path[256];
    char            name[256];
    fnd_inp_device_type type;
} device_info;

struct fnd_inp_devices_list {
    device_info*    entries;
    size_t          count;
    size_t          capacity;
};

fnd_inp_devices_list* fnd_inp_library_get_devices_list(fnd_inp_library* lib) {
    (void)lib;

    fnd_inp_devices_list* list = calloc(1, sizeof(*list));
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

void fnd_inp_free_devices_list(fnd_inp_devices_list* list) {
    if (!list) return;
    free(list->entries);
    free(list);
}

size_t fnd_inp_devices_list_get_size(fnd_inp_devices_list* list) {
    return list->count;
}

fnd_inp_device_type fnd_inp_devices_list_query_type(fnd_inp_devices_list* list, size_t idx) {
    if (idx >= list->count) return fnd_inp_device_unknown;
    return list->entries[idx].type;
}

const char* fnd_inp_devices_list_query_name(fnd_inp_devices_list* list, size_t idx) {
    if (idx >= list->count) return NULL;
    return list->entries[idx].name;
}

/*
    Slot
*/

struct fnd_inp_slot {
    struct libevdev*    dev;
    int                 connected;
    int                 fd;
    float               mouse_sensitivity;
    float               axis_deadzone[fnd_inp_axis_count];

    // process variables
    float mouse_dx;
    float mouse_dy;
};

fnd_inp_slot* fnd_inp_create_slot(fnd_inp_library* lib) {
    (void)lib;

    fnd_inp_slot* slot = calloc(1, sizeof(*slot));
    if (!slot) return NULL;

    slot->fd                = -1;
    slot->mouse_sensitivity =  1; // default mouse sensitivity

    return slot;
}

void fnd_inp_free_slot(fnd_inp_slot* slot) {
    if (!slot) return;
    fnd_inp_slot_disconnect(slot);
    free(slot);
}

void fnd_inp_slot_disconnect(fnd_inp_slot* slot) {
    if (slot->dev) { libevdev_free(slot->dev); slot->dev = NULL; }
    if (slot->fd >= 0) { close(slot->fd); slot->fd = -1; }
    slot->connected = 0;
}

int fnd_inp_slot_connect(fnd_inp_slot* slot, fnd_inp_devices_list* list, size_t idx) {
    if (idx >= list->count) return 0;
    fnd_inp_slot_disconnect(slot); // drop any existing slot first

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

int fnd_inp_slot_connected(fnd_inp_slot* slot) {
    return slot->connected;
}

void fnd_inp_slot_set_mouse_sensitivity(fnd_inp_slot* slot, float sensitivity) {
    slot->mouse_sensitivity = sensitivity;
}

void fnd_inp_slot_set_axis_deadzone(fnd_inp_slot* slot, unsigned int axis, float deadzone) {
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
static int drain_events(fnd_inp_slot* slot, struct libevdev* dev) {
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

void fnd_inp_slot_query_input_state(fnd_inp_slot* slot, fnd_inp_input_state* st) {
    *st = (fnd_inp_input_state){0}; // 0 init
    if (!slot || !slot->connected || !slot->dev) return;

    struct libevdev* dev = slot->dev;

    // drain pending events, on ENODEV the device was hot-unplugged: mark as disconnected
    if (drain_events(slot, dev) == -ENODEV) {
        slot->connected = 0;
        return;
    }

    // Buttons
    for (int i = 0; i < fnd_inp_btn_count; i++) {
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
        fnd_inp_button btn      = modifier_pairs_mapping[i].btn;
        unsigned int rcode  = modifier_pairs_mapping[i].right_code;

        if (!st->buttons[btn] && libevdev_has_event_code(dev, EV_KEY, rcode)) 
            st->buttons[btn] = libevdev_get_event_value(dev, EV_KEY, rcode);
    }

    // Axes
    for (int i = 0; i < fnd_inp_axis_count; i++) {
        unsigned int code = axis_mapping[i].code;
        if (libevdev_has_event_code(dev, EV_ABS, code)) {
            int raw = libevdev_get_event_value(dev, EV_ABS, code);
            st->axes[axis_mapping[i].axis] = normalize_abs(libevdev_get_abs_info(dev, code), raw, slot->axis_deadzone[i]);
        }
    }

    // Mouse Axes
    float*   delta_var[] = { &slot->mouse_dx,   &slot->mouse_dy };
    fnd_inp_axis axes[]      = { fnd_inp_axis_ms_x,     fnd_inp_axis_ms_y   };
    for (int i = 0; i < 2; i++) {
        fnd_inp_axis axis = axes[i];

        st->axes[axis] = *delta_var[i] * mouse_inner_sensitivity_factor * slot->mouse_sensitivity;
        if (st->axes[axis] > 1.0f)  st->axes[axis] = 1.0f;
        if (st->axes[axis] < -1.0f) st->axes[axis] = -1.0f;
        st->axes[axis] = apply_deadzone(st->axes[axis], slot->axis_deadzone[axis]);

        *delta_var[i] *= mouse_inner_damping_factor;
    }
}

#elif defined(FUNDATIO_INPUTS_WINDOWS)

#define DIRECTINPUT_VERSION 0x0800
 
#include <windows.h>
#include <xinput.h>
#include <dinput.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <wchar.h>
 
#ifdef _MSC_VER
    #pragma comment(lib, "xinput.lib")
    #pragma comment(lib, "dinput8.lib")
    #pragma comment(lib, "dxguid.lib")
#endif
 
// ------------------------------------------------------------
// Internal types
// ------------------------------------------------------------
 
typedef enum fnd_inp_win_gp_backend {
    fnd_inp_win_gp_backend_xinput,
    fnd_inp_win_gp_backend_dinput,
} fnd_inp_win_gp_backend;
 
typedef struct fnd_inp_hw_device {
    fnd_inp_device_type    type;
    char               name[64];
    fnd_inp_win_gp_backend gp_backend;   // valid only if type == gamepad
    DWORD              xinput_index; // valid only if gp_backend == xinput
    GUID               dinput_guid;  // valid only if gp_backend == dinput
} fnd_inp_hw_device;
 
struct fnd_inp_library {
    LPDIRECTINPUT8A dinput; // NULL if init failed; keyboard/mouse/XInput still work
};
 
struct fnd_inp_devices_list {
    fnd_inp_hw_device* items;
    size_t         count;
    size_t         capacity;
};
 
struct fnd_inp_slot {
    fnd_inp_library*    lib;
    int             connected;
    fnd_inp_device_type type;
 
    // mouse delta tracking
    POINT last_mouse_pos;
    int   mouse_initialized;
    float mouse_sensitivity;
 
    // gamepad
    fnd_inp_win_gp_backend    gp_backend;
    DWORD                 xinput_index;
    LPDIRECTINPUTDEVICE8A didevice; // valid only if gp_backend == dinput and connected
 
    float axis_deadzone[fnd_inp_axis_count];
};
 
typedef struct fnd_inp_win__enum_ctx {
    fnd_inp_devices_list* list;
    LPDIRECTINPUT8A   dinput;
} fnd_inp_win__enum_ctx;
 
// ------------------------------------------------------------
// Devices list: growable array, no arbitrary cap
// ------------------------------------------------------------
 
static fnd_inp_hw_device* fnd_inp_win__push(fnd_inp_devices_list* list) {
    if (list->count == list->capacity) {
        size_t cap = list->capacity ? list->capacity * 2 : 4;
        fnd_inp_hw_device* items = (fnd_inp_hw_device*)realloc(list->items, cap * sizeof(fnd_inp_hw_device));
        if (!items) return NULL;
        list->items = items;
        list->capacity = cap;
    }
 
    fnd_inp_hw_device* d = &list->items[list->count++];
    memset(d, 0, sizeof(*d));
    return d;
}
 
static fnd_inp_hw_device* fnd_inp_win__add(fnd_inp_devices_list* list, fnd_inp_device_type type, const char* name) {
    fnd_inp_hw_device* d = fnd_inp_win__push(list);
    if (!d) return NULL;
    d->type = type;
    strncpy(d->name, name, sizeof(d->name) - 1);
    return d;
}
 
// Detects whether a DirectInput device instance is really an XInput
// device in disguise, via the "IG_" tag Windows uses for XInput-class
// HID collections in the device interface path.
static int fnd_inp_win__is_xinput_backed(LPDIRECTINPUT8A dinput, const GUID* guid_instance) {
    LPDIRECTINPUTDEVICE8A tmp = NULL;
    if (FAILED(IDirectInput8_CreateDevice(dinput, guid_instance, &tmp, NULL))) return 0;
 
    DIPROPGUIDANDPATH prop = {
        .diph = { .dwSize = sizeof(prop), .dwHeaderSize = sizeof(DIPROPHEADER), .dwHow = DIPH_DEVICE },
    };
 
    int is_xinput = 0;
    if (SUCCEEDED(IDirectInputDevice8_GetProperty(tmp, DIPROP_GUIDANDPATH, &prop.diph))) {
        is_xinput = wcsstr(prop.wszPath, L"IG_") != NULL || wcsstr(prop.wszPath, L"ig_") != NULL;
    }
 
    IDirectInputDevice8_Release(tmp);
    return is_xinput;
}
 
static BOOL CALLBACK fnd_inp_win__enum_joysticks_callback(const DIDEVICEINSTANCEA* inst, VOID* ctxptr) {
    fnd_inp_win__enum_ctx* ctx = (fnd_inp_win__enum_ctx*)ctxptr;
    if (fnd_inp_win__is_xinput_backed(ctx->dinput, &inst->guidInstance)) return DIENUM_CONTINUE;
 
    fnd_inp_hw_device* gp = fnd_inp_win__add(ctx->list, fnd_inp_device_gamepad, inst->tszInstanceName);
    if (gp) {
        gp->gp_backend  = fnd_inp_win_gp_backend_dinput;
        gp->dinput_guid = inst->guidInstance;
    }
 
    return DIENUM_CONTINUE;
}
 
// axial deadzone: 0 = none, 1 = binary, in-between rescales [deadzone,1] -> [0,1]
static float fnd_inp_win__apply_deadzone(float value, float deadzone) {
    float sign = (value < 0.0f) ? -1.0f : 1.0f;
    float mag  = fabsf(value);
 
    if (deadzone >= 1.0f) return (mag >= 1.0f) ? sign : 0.0f;
    if (mag < deadzone)   return 0.0f;
 
    float scaled = (mag - deadzone) / (1.0f - deadzone);
    if (scaled > 1.0f) scaled = 1.0f;
    return sign * scaled;
}
 
// ------------------------------------------------------------
// Library
// ------------------------------------------------------------
 
fnd_inp_library* fnd_inp_create_library() {
    fnd_inp_library* lib = (fnd_inp_library*)calloc(1, sizeof(fnd_inp_library));
    if (!lib) return NULL;
 
   if (FAILED(DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8A, (void**)&lib->dinput, NULL))) {
        lib->dinput = NULL;
    }
 
    return lib;
}
 
void fnd_inp_free_library(fnd_inp_library* lib) {
    if (!lib) return;
    if (lib->dinput) IDirectInput8_Release(lib->dinput);
    free(lib);
}
 
// ------------------------------------------------------------
// Devices List
// ------------------------------------------------------------
 
fnd_inp_devices_list* fnd_inp_library_get_devices_list(fnd_inp_library* lib) {
    if (!lib) return NULL;
 
    fnd_inp_devices_list* list = (fnd_inp_devices_list*)calloc(1, sizeof(fnd_inp_devices_list));
    if (!list) return NULL;
 
    fnd_inp_win__add(list, fnd_inp_device_keyboard, "Keyboard");
    fnd_inp_win__add(list, fnd_inp_device_mouse, "Mouse");
 
    // XInput controllers (hard cap of 4 - a real hardware/driver limit, not ours)
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        XINPUT_STATE state = {0};
        if (XInputGetState(i, &state) != ERROR_SUCCESS) continue;
 
        char name[32];
        sprintf(name, "XInput Controller %u", (unsigned int)(i + 1));
 
        fnd_inp_hw_device* gp = fnd_inp_win__add(list, fnd_inp_device_gamepad, name);
        if (gp) {
            gp->gp_backend   = fnd_inp_win_gp_backend_xinput;
            gp->xinput_index = i;
        }
    }
 
    // Remaining DirectInput gamepads/joysticks (non-XInput only)
    if (lib->dinput) {
        fnd_inp_win__enum_ctx ctx = { .list = list, .dinput = lib->dinput };
        IDirectInput8_EnumDevices(lib->dinput, DI8DEVCLASS_GAMECTRL,
                                   fnd_inp_win__enum_joysticks_callback, &ctx, DIEDFL_ATTACHEDONLY);
    }
 
    return list;
}
 
void fnd_inp_free_devices_list(fnd_inp_devices_list* list) {
    if (!list) return;
    free(list->items);
    free(list);
}
 
size_t fnd_inp_devices_list_get_size(fnd_inp_devices_list* list) {
    return list ? list->count : 0;
}
 
fnd_inp_device_type fnd_inp_devices_list_query_type(fnd_inp_devices_list* list, size_t idx) {
    return (list && idx < list->count) ? list->items[idx].type : fnd_inp_device_unknown;
}
 
const char* fnd_inp_devices_list_query_name(fnd_inp_devices_list* list, size_t idx) {
    return (list && idx < list->count) ? list->items[idx].name : NULL;
}
 
// ------------------------------------------------------------
// Slot
// ------------------------------------------------------------
 
fnd_inp_slot* fnd_inp_create_slot(fnd_inp_library* lib) {
    fnd_inp_slot* slot = (fnd_inp_slot*)calloc(1, sizeof(fnd_inp_slot));
    if (!slot) return NULL;
    slot->lib = lib;
    slot->mouse_sensitivity = 1.0f;
    return slot;
}
 
void fnd_inp_slot_disconnect(fnd_inp_slot* slot) {
    if (!slot) return;
 
    if (slot->didevice) {
        IDirectInputDevice8_Unacquire(slot->didevice);
        IDirectInputDevice8_Release(slot->didevice);
        slot->didevice = NULL;
    }
 
    slot->connected = 0;
    slot->mouse_initialized = 0;
}
 
void fnd_inp_free_slot(fnd_inp_slot* slot) {
    if (!slot) return;
    fnd_inp_slot_disconnect(slot);
    free(slot);
}
 
static int fnd_inp_win__connect_gamepad(fnd_inp_slot* slot, fnd_inp_hw_device* dev) {
    slot->gp_backend = dev->gp_backend;
 
    if (dev->gp_backend == fnd_inp_win_gp_backend_xinput) {
        XINPUT_STATE state = {0};
        if (XInputGetState(dev->xinput_index, &state) != ERROR_SUCCESS) return 0;
        slot->xinput_index = dev->xinput_index;
        slot->connected = 1;
        return 1;
    }
 
    if (!slot->lib || !slot->lib->dinput) return 0;
 
    LPDIRECTINPUTDEVICE8A device = NULL;
    if (FAILED(IDirectInput8_CreateDevice(slot->lib->dinput, &dev->dinput_guid, &device, NULL))) return 0;
 
    IDirectInputDevice8_SetDataFormat(device, &c_dfDIJoystick2);
    IDirectInputDevice8_SetCooperativeLevel(device, GetDesktopWindow(), DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
 
    // Normalize sticks to a known range so the query function can rescale consistently.
    DWORD offsets[] = { DIJOFS_X, DIJOFS_Y, DIJOFS_RX, DIJOFS_RY, DIJOFS_Z, DIJOFS_RZ };
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        DIPROPRANGE range = {
            .diph = { .dwSize = sizeof(range), .dwHeaderSize = sizeof(DIPROPHEADER), .dwObj = offsets[i], .dwHow = DIPH_BYOFFSET },
            .lMin = -1000,
            .lMax =  1000,
        };
        IDirectInputDevice8_SetProperty(device, DIPROP_RANGE, &range.diph);
    }
 
    IDirectInputDevice8_Acquire(device); // best-effort; background devices may acquire lazily
 
    slot->didevice  = device;
    slot->connected = 1;
    return 1;
}
 
int fnd_inp_slot_connect(fnd_inp_slot* slot, fnd_inp_devices_list* list, size_t idx) {
    if (!slot || !list || idx >= list->count) return 0;
    fnd_inp_slot_disconnect(slot);
 
    fnd_inp_hw_device* dev = &list->items[idx];
    slot->type = dev->type;
 
    if (dev->type == fnd_inp_device_keyboard || dev->type == fnd_inp_device_mouse) {
        slot->connected = 1;
        return 1;
    }
    if (dev->type != fnd_inp_device_gamepad) return 0;
 
    return fnd_inp_win__connect_gamepad(slot, dev);
}
 
int fnd_inp_slot_connected(fnd_inp_slot* slot) {
    return slot ? slot->connected : 0;
}
 
void fnd_inp_slot_set_mouse_sensitivity(fnd_inp_slot* slot, float sensitivity) {
    if (!slot) return;
    slot->mouse_sensitivity = sensitivity;
}
 
void fnd_inp_slot_set_axis_deadzone(fnd_inp_slot* slot, unsigned int axis, float deadzone) {
    if (!slot || axis >= fnd_inp_axis_count) return;
    if (deadzone < 0.0f) deadzone = 0.0f;
    if (deadzone > 1.0f) deadzone = 1.0f;
    slot->axis_deadzone[axis] = deadzone;
}
 
// ------------------------------------------------------------
// Input State
// ------------------------------------------------------------
 
static const struct { fnd_inp_button btn; int vk; } fnd_inp_win__key_map[] = {
    { .btn = fnd_inp_btn_kb_escape,    .vk = VK_ESCAPE },
    { .btn = fnd_inp_btn_kb_tab,       .vk = VK_TAB },
    { .btn = fnd_inp_btn_kb_enter,     .vk = VK_RETURN },
    { .btn = fnd_inp_btn_kb_space,     .vk = VK_SPACE },
    { .btn = fnd_inp_btn_kb_backspace, .vk = VK_BACK },
    { .btn = fnd_inp_btn_kb_delete,    .vk = VK_DELETE },
    { .btn = fnd_inp_btn_kb_up,        .vk = VK_UP },
    { .btn = fnd_inp_btn_kb_down,      .vk = VK_DOWN },
    { .btn = fnd_inp_btn_kb_left,      .vk = VK_LEFT },
    { .btn = fnd_inp_btn_kb_right,     .vk = VK_RIGHT },
    { .btn = fnd_inp_btn_kb_shift,     .vk = VK_SHIFT },
    { .btn = fnd_inp_btn_kb_ctrl,      .vk = VK_CONTROL },
    { .btn = fnd_inp_btn_kb_alt,       .vk = VK_MENU },
    { .btn = fnd_inp_btn_kb_super,     .vk = VK_LWIN },
    { .btn = fnd_inp_btn_kb_a, .vk = 'A' }, { .btn = fnd_inp_btn_kb_b, .vk = 'B' }, { .btn = fnd_inp_btn_kb_c, .vk = 'C' },
    { .btn = fnd_inp_btn_kb_d, .vk = 'D' }, { .btn = fnd_inp_btn_kb_e, .vk = 'E' }, { .btn = fnd_inp_btn_kb_f, .vk = 'F' },
    { .btn = fnd_inp_btn_kb_g, .vk = 'G' }, { .btn = fnd_inp_btn_kb_h, .vk = 'H' }, { .btn = fnd_inp_btn_kb_i, .vk = 'I' },
    { .btn = fnd_inp_btn_kb_j, .vk = 'J' }, { .btn = fnd_inp_btn_kb_k, .vk = 'K' }, { .btn = fnd_inp_btn_kb_l, .vk = 'L' },
    { .btn = fnd_inp_btn_kb_m, .vk = 'M' }, { .btn = fnd_inp_btn_kb_n, .vk = 'N' }, { .btn = fnd_inp_btn_kb_o, .vk = 'O' },
    { .btn = fnd_inp_btn_kb_p, .vk = 'P' }, { .btn = fnd_inp_btn_kb_q, .vk = 'Q' }, { .btn = fnd_inp_btn_kb_r, .vk = 'R' },
    { .btn = fnd_inp_btn_kb_s, .vk = 'S' }, { .btn = fnd_inp_btn_kb_t, .vk = 'T' }, { .btn = fnd_inp_btn_kb_u, .vk = 'U' },
    { .btn = fnd_inp_btn_kb_v, .vk = 'V' }, { .btn = fnd_inp_btn_kb_w, .vk = 'W' }, { .btn = fnd_inp_btn_kb_x, .vk = 'X' },
    { .btn = fnd_inp_btn_kb_y, .vk = 'Y' }, { .btn = fnd_inp_btn_kb_z, .vk = 'Z' },
    { .btn = fnd_inp_btn_kb_0, .vk = '0' }, { .btn = fnd_inp_btn_kb_1, .vk = '1' }, { .btn = fnd_inp_btn_kb_2, .vk = '2' },
    { .btn = fnd_inp_btn_kb_3, .vk = '3' }, { .btn = fnd_inp_btn_kb_4, .vk = '4' }, { .btn = fnd_inp_btn_kb_5, .vk = '5' },
    { .btn = fnd_inp_btn_kb_6, .vk = '6' }, { .btn = fnd_inp_btn_kb_7, .vk = '7' }, { .btn = fnd_inp_btn_kb_8, .vk = '8' },
    { .btn = fnd_inp_btn_kb_9, .vk = '9' },
};
 
static void fnd_inp_win__query_keyboard(fnd_inp_input_state* out) {
    for (size_t i = 0; i < sizeof(fnd_inp_win__key_map) / sizeof(fnd_inp_win__key_map[0]); i++) {
        out->buttons[fnd_inp_win__key_map[i].btn] = (GetAsyncKeyState(fnd_inp_win__key_map[i].vk) & 0x8000) ? 1 : 0;
    }
}
 
static void fnd_inp_win__query_mouse(fnd_inp_slot* slot, fnd_inp_input_state* out) {
    out->buttons[fnd_inp_btn_ms_left]    = (GetAsyncKeyState(VK_LBUTTON)  & 0x8000) ? 1 : 0;
    out->buttons[fnd_inp_btn_ms_right]   = (GetAsyncKeyState(VK_RBUTTON)  & 0x8000) ? 1 : 0;
    out->buttons[fnd_inp_btn_ms_middle]  = (GetAsyncKeyState(VK_MBUTTON)  & 0x8000) ? 1 : 0;
    out->buttons[fnd_inp_btn_ms_back]    = (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) ? 1 : 0;
    out->buttons[fnd_inp_btn_ms_forward] = (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) ? 1 : 0;
 
    POINT pos;
    if (!GetCursorPos(&pos)) return;
 
    if (!slot->mouse_initialized) {
        slot->last_mouse_pos = pos;
        slot->mouse_initialized = 1;
    }
 
    out->axes[fnd_inp_axis_ms_x] = (float)(pos.x - slot->last_mouse_pos.x) * slot->mouse_sensitivity;
    out->axes[fnd_inp_axis_ms_y] = (float)(pos.y - slot->last_mouse_pos.y) * slot->mouse_sensitivity;
    slot->last_mouse_pos = pos;
}
 
static void fnd_inp_win__query_xinput(fnd_inp_slot* slot, fnd_inp_input_state* out) {
    XINPUT_STATE state = {0};
    if (XInputGetState(slot->xinput_index, &state) != ERROR_SUCCESS) {
        slot->connected = 0; // controller unplugged
        return;
    }
 
    WORD b = state.Gamepad.wButtons;
    out->buttons[fnd_inp_btn_gp_north] = (b & XINPUT_GAMEPAD_Y) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_east]  = (b & XINPUT_GAMEPAD_B) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_south] = (b & XINPUT_GAMEPAD_A) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_west]  = (b & XINPUT_GAMEPAD_X) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_lb]    = (b & XINPUT_GAMEPAD_LEFT_SHOULDER)  ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_rb]    = (b & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_l3]    = (b & XINPUT_GAMEPAD_LEFT_THUMB)  ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_r3]    = (b & XINPUT_GAMEPAD_RIGHT_THUMB) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_start] = (b & XINPUT_GAMEPAD_START) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_select]= (b & XINPUT_GAMEPAD_BACK)  ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_dpad_up]    = (b & XINPUT_GAMEPAD_DPAD_UP)    ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_dpad_right] = (b & XINPUT_GAMEPAD_DPAD_RIGHT) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_dpad_down]  = (b & XINPUT_GAMEPAD_DPAD_DOWN)  ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_dpad_left]  = (b & XINPUT_GAMEPAD_DPAD_LEFT)  ? 1 : 0;
    // XInput doesn't expose the Guide/Home button; fnd_inp_btn_gp_home stays 0.
 
    out->buttons[fnd_inp_btn_gp_lt] = (state.Gamepad.bLeftTrigger  > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_rt] = (state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? 1 : 0;
 
    float lx = state.Gamepad.sThumbLX / 32767.0f;
    float ly = state.Gamepad.sThumbLY / 32767.0f;
    float rx = state.Gamepad.sThumbRX / 32767.0f;
    float ry = state.Gamepad.sThumbRY / 32767.0f;
    float lt = state.Gamepad.bLeftTrigger  / 255.0f;
    float rt = state.Gamepad.bRightTrigger / 255.0f;
 
    out->axes[fnd_inp_axis_gp_lx] = fnd_inp_win__apply_deadzone(lx, slot->axis_deadzone[fnd_inp_axis_gp_lx]);
    out->axes[fnd_inp_axis_gp_ly] = fnd_inp_win__apply_deadzone(ly, slot->axis_deadzone[fnd_inp_axis_gp_ly]);
    out->axes[fnd_inp_axis_gp_rx] = fnd_inp_win__apply_deadzone(rx, slot->axis_deadzone[fnd_inp_axis_gp_rx]);
    out->axes[fnd_inp_axis_gp_ry] = fnd_inp_win__apply_deadzone(ry, slot->axis_deadzone[fnd_inp_axis_gp_ry]);
    out->axes[fnd_inp_axis_gp_lt] = fnd_inp_win__apply_deadzone(lt, slot->axis_deadzone[fnd_inp_axis_gp_lt]);
    out->axes[fnd_inp_axis_gp_rt] = fnd_inp_win__apply_deadzone(rt, slot->axis_deadzone[fnd_inp_axis_gp_rt]);
}
 
static void fnd_inp_win__query_dinput(fnd_inp_slot* slot, fnd_inp_input_state* out) {
    if (!slot->didevice) return;
 
    if (FAILED(IDirectInputDevice8_Poll(slot->didevice))) {
        if (FAILED(IDirectInputDevice8_Acquire(slot->didevice))) return; // temporarily unavailable
        IDirectInputDevice8_Poll(slot->didevice);
    }
 
    DIJOYSTATE2 js = {0};
    if (FAILED(IDirectInputDevice8_GetDeviceState(slot->didevice, sizeof(js), &js))) {
        slot->connected = 0; // device lost (e.g. unplugged)
        return;
    }
 
    // Best-effort default layout; DirectInput doesn't standardize button order across vendors.
    out->buttons[fnd_inp_btn_gp_south] = (js.rgbButtons[0]  & 0x80) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_east]  = (js.rgbButtons[1]  & 0x80) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_west]  = (js.rgbButtons[2]  & 0x80) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_north] = (js.rgbButtons[3]  & 0x80) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_lb]    = (js.rgbButtons[4]  & 0x80) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_rb]    = (js.rgbButtons[5]  & 0x80) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_lt]    = (js.rgbButtons[6]  & 0x80) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_rt]    = (js.rgbButtons[7]  & 0x80) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_select]= (js.rgbButtons[8]  & 0x80) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_start] = (js.rgbButtons[9]  & 0x80) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_l3]    = (js.rgbButtons[10] & 0x80) ? 1 : 0;
    out->buttons[fnd_inp_btn_gp_r3]    = (js.rgbButtons[11] & 0x80) ? 1 : 0;
 
    // D-pad via POV hat (hundredths of a degree; 0xFFFF == centered/none)
    DWORD pov = js.rgdwPOV[0];
    if (LOWORD(pov) != 0xFFFF) {
        out->buttons[fnd_inp_btn_gp_dpad_up]    = (pov > 31500 || pov < 4500);
        out->buttons[fnd_inp_btn_gp_dpad_right] = (pov > 4500  && pov < 13500);
        out->buttons[fnd_inp_btn_gp_dpad_down]  = (pov > 13500 && pov < 22500);
        out->buttons[fnd_inp_btn_gp_dpad_left]  = (pov > 22500 && pov < 31500);
    }
 
    float lx = js.lX  / 1000.0f;
    float ly = js.lY  / 1000.0f;
    float rx = js.lRx / 1000.0f;
    float ry = js.lRy / 1000.0f;
    // Many pads surface triggers on the Z/RZ axes; best-effort 0..1 remap.
    float lt = (js.lZ  + 1000.0f) / 2000.0f;
    float rt = (js.lRz + 1000.0f) / 2000.0f;
 
    out->axes[fnd_inp_axis_gp_lx] = fnd_inp_win__apply_deadzone(lx,  slot->axis_deadzone[fnd_inp_axis_gp_lx]);
    out->axes[fnd_inp_axis_gp_ly] = fnd_inp_win__apply_deadzone(-ly, slot->axis_deadzone[fnd_inp_axis_gp_ly]); // invert to match XInput's +up
    out->axes[fnd_inp_axis_gp_rx] = fnd_inp_win__apply_deadzone(rx,  slot->axis_deadzone[fnd_inp_axis_gp_rx]);
    out->axes[fnd_inp_axis_gp_ry] = fnd_inp_win__apply_deadzone(-ry, slot->axis_deadzone[fnd_inp_axis_gp_ry]);
    out->axes[fnd_inp_axis_gp_lt] = fnd_inp_win__apply_deadzone(lt,  slot->axis_deadzone[fnd_inp_axis_gp_lt]);
    out->axes[fnd_inp_axis_gp_rt] = fnd_inp_win__apply_deadzone(rt,  slot->axis_deadzone[fnd_inp_axis_gp_rt]);
}
 
void fnd_inp_slot_query_input_state(fnd_inp_slot* slot, fnd_inp_input_state* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!slot || !slot->connected) return;
 
    if (slot->type == fnd_inp_device_keyboard) { fnd_inp_win__query_keyboard(out); return; }
    if (slot->type == fnd_inp_device_mouse)    { fnd_inp_win__query_mouse(slot, out); return; }
    if (slot->type != fnd_inp_device_gamepad) return;
 
    if (slot->gp_backend == fnd_inp_win_gp_backend_xinput) fnd_inp_win__query_xinput(slot, out);
    else fnd_inp_win__query_dinput(slot, out);
}

#else
    #error No OS info provided for fundatio inputs!
#endif // OS IF
#endif // FUNDATIO_INPUTS_IMPL
