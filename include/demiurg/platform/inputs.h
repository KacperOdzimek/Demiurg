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
    - DEMIURG_INPUTS_WINDOWS

----------------------------------------------------------------
Depedencies:
- each OS have own compilation requirements:
    - DEMIURG_INPUTS_LINUX
        - libevdev library installed
    - DEMIURG_INPUTS_WINDOWS
        - windows sdk installed
        - xinput library installed

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
#if defined(DEMIURG_INPUTS_LINUX)

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

#elif defined(DEMIURG_INPUTS_WINDOWS)

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
 
typedef enum din_win_gp_backend {
    din_win_gp_backend_xinput,
    din_win_gp_backend_dinput,
} din_win_gp_backend;
 
typedef struct din_hw_device {
    din_device_type    type;
    char               name[64];
    din_win_gp_backend gp_backend;   // valid only if type == gamepad
    DWORD              xinput_index; // valid only if gp_backend == xinput
    GUID               dinput_guid;  // valid only if gp_backend == dinput
} din_hw_device;
 
struct din_library {
    LPDIRECTINPUT8A dinput; // NULL if init failed; keyboard/mouse/XInput still work
};
 
struct din_devices_list {
    din_hw_device* items;
    size_t         count;
    size_t         capacity;
};
 
struct din_slot {
    din_library*    lib;
    int             connected;
    din_device_type type;
 
    // mouse delta tracking
    POINT last_mouse_pos;
    int   mouse_initialized;
    float mouse_sensitivity;
 
    // gamepad
    din_win_gp_backend    gp_backend;
    DWORD                 xinput_index;
    LPDIRECTINPUTDEVICE8A didevice; // valid only if gp_backend == dinput and connected
 
    float axis_deadzone[din_axis_count];
};
 
typedef struct din_win__enum_ctx {
    din_devices_list* list;
    LPDIRECTINPUT8A   dinput;
} din_win__enum_ctx;
 
// ------------------------------------------------------------
// Devices list: growable array, no arbitrary cap
// ------------------------------------------------------------
 
static din_hw_device* din_win__push(din_devices_list* list) {
    if (list->count == list->capacity) {
        size_t cap = list->capacity ? list->capacity * 2 : 4;
        din_hw_device* items = (din_hw_device*)realloc(list->items, cap * sizeof(din_hw_device));
        if (!items) return NULL;
        list->items = items;
        list->capacity = cap;
    }
 
    din_hw_device* d = &list->items[list->count++];
    memset(d, 0, sizeof(*d));
    return d;
}
 
static din_hw_device* din_win__add(din_devices_list* list, din_device_type type, const char* name) {
    din_hw_device* d = din_win__push(list);
    if (!d) return NULL;
    d->type = type;
    strncpy(d->name, name, sizeof(d->name) - 1);
    return d;
}
 
// Detects whether a DirectInput device instance is really an XInput
// device in disguise, via the "IG_" tag Windows uses for XInput-class
// HID collections in the device interface path.
static int din_win__is_xinput_backed(LPDIRECTINPUT8A dinput, const GUID* guid_instance) {
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
 
static BOOL CALLBACK din_win__enum_joysticks_callback(const DIDEVICEINSTANCEA* inst, VOID* ctxptr) {
    din_win__enum_ctx* ctx = (din_win__enum_ctx*)ctxptr;
    if (din_win__is_xinput_backed(ctx->dinput, &inst->guidInstance)) return DIENUM_CONTINUE;
 
    din_hw_device* gp = din_win__add(ctx->list, din_device_gamepad, inst->tszInstanceName);
    if (gp) {
        gp->gp_backend  = din_win_gp_backend_dinput;
        gp->dinput_guid = inst->guidInstance;
    }
 
    return DIENUM_CONTINUE;
}
 
// axial deadzone: 0 = none, 1 = binary, in-between rescales [deadzone,1] -> [0,1]
static float din_win__apply_deadzone(float value, float deadzone) {
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
 
din_library* din_create_library() {
    din_library* lib = (din_library*)calloc(1, sizeof(din_library));
    if (!lib) return NULL;
 
   if (FAILED(DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8A, (void**)&lib->dinput, NULL))) {
        lib->dinput = NULL;
    }
 
    return lib;
}
 
void din_free_library(din_library* lib) {
    if (!lib) return;
    if (lib->dinput) IDirectInput8_Release(lib->dinput);
    free(lib);
}
 
// ------------------------------------------------------------
// Devices List
// ------------------------------------------------------------
 
din_devices_list* din_library_get_devices_list(din_library* lib) {
    if (!lib) return NULL;
 
    din_devices_list* list = (din_devices_list*)calloc(1, sizeof(din_devices_list));
    if (!list) return NULL;
 
    din_win__add(list, din_device_keyboard, "Keyboard");
    din_win__add(list, din_device_mouse, "Mouse");
 
    // XInput controllers (hard cap of 4 - a real hardware/driver limit, not ours)
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        XINPUT_STATE state = {0};
        if (XInputGetState(i, &state) != ERROR_SUCCESS) continue;
 
        char name[32];
        sprintf(name, "XInput Controller %u", (unsigned int)(i + 1));
 
        din_hw_device* gp = din_win__add(list, din_device_gamepad, name);
        if (gp) {
            gp->gp_backend   = din_win_gp_backend_xinput;
            gp->xinput_index = i;
        }
    }
 
    // Remaining DirectInput gamepads/joysticks (non-XInput only)
    if (lib->dinput) {
        din_win__enum_ctx ctx = { .list = list, .dinput = lib->dinput };
        IDirectInput8_EnumDevices(lib->dinput, DI8DEVCLASS_GAMECTRL,
                                   din_win__enum_joysticks_callback, &ctx, DIEDFL_ATTACHEDONLY);
    }
 
    return list;
}
 
void din_free_devices_list(din_devices_list* list) {
    if (!list) return;
    free(list->items);
    free(list);
}
 
size_t din_devices_list_get_size(din_devices_list* list) {
    return list ? list->count : 0;
}
 
din_device_type din_devices_list_query_type(din_devices_list* list, size_t idx) {
    return (list && idx < list->count) ? list->items[idx].type : din_device_unknown;
}
 
const char* din_devices_list_query_name(din_devices_list* list, size_t idx) {
    return (list && idx < list->count) ? list->items[idx].name : NULL;
}
 
// ------------------------------------------------------------
// Slot
// ------------------------------------------------------------
 
din_slot* din_create_slot(din_library* lib) {
    din_slot* slot = (din_slot*)calloc(1, sizeof(din_slot));
    if (!slot) return NULL;
    slot->lib = lib;
    slot->mouse_sensitivity = 1.0f;
    return slot;
}
 
void din_slot_disconnect(din_slot* slot) {
    if (!slot) return;
 
    if (slot->didevice) {
        IDirectInputDevice8_Unacquire(slot->didevice);
        IDirectInputDevice8_Release(slot->didevice);
        slot->didevice = NULL;
    }
 
    slot->connected = 0;
    slot->mouse_initialized = 0;
}
 
void din_free_slot(din_slot* slot) {
    if (!slot) return;
    din_slot_disconnect(slot);
    free(slot);
}
 
static int din_win__connect_gamepad(din_slot* slot, din_hw_device* dev) {
    slot->gp_backend = dev->gp_backend;
 
    if (dev->gp_backend == din_win_gp_backend_xinput) {
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
 
int din_slot_connect(din_slot* slot, din_devices_list* list, size_t idx) {
    if (!slot || !list || idx >= list->count) return 0;
    din_slot_disconnect(slot);
 
    din_hw_device* dev = &list->items[idx];
    slot->type = dev->type;
 
    if (dev->type == din_device_keyboard || dev->type == din_device_mouse) {
        slot->connected = 1;
        return 1;
    }
    if (dev->type != din_device_gamepad) return 0;
 
    return din_win__connect_gamepad(slot, dev);
}
 
int din_slot_connected(din_slot* slot) {
    return slot ? slot->connected : 0;
}
 
void din_slot_set_mouse_sensitivity(din_slot* slot, float sensitivity) {
    if (!slot) return;
    slot->mouse_sensitivity = sensitivity;
}
 
void din_slot_set_axis_deadzone(din_slot* slot, unsigned int axis, float deadzone) {
    if (!slot || axis >= din_axis_count) return;
    if (deadzone < 0.0f) deadzone = 0.0f;
    if (deadzone > 1.0f) deadzone = 1.0f;
    slot->axis_deadzone[axis] = deadzone;
}
 
// ------------------------------------------------------------
// Input State
// ------------------------------------------------------------
 
static const struct { din_button btn; int vk; } din_win__key_map[] = {
    { .btn = din_btn_kb_escape,    .vk = VK_ESCAPE },
    { .btn = din_btn_kb_tab,       .vk = VK_TAB },
    { .btn = din_btn_kb_enter,     .vk = VK_RETURN },
    { .btn = din_btn_kb_space,     .vk = VK_SPACE },
    { .btn = din_btn_kb_backspace, .vk = VK_BACK },
    { .btn = din_btn_kb_delete,    .vk = VK_DELETE },
    { .btn = din_btn_kb_up,        .vk = VK_UP },
    { .btn = din_btn_kb_down,      .vk = VK_DOWN },
    { .btn = din_btn_kb_left,      .vk = VK_LEFT },
    { .btn = din_btn_kb_right,     .vk = VK_RIGHT },
    { .btn = din_btn_kb_shift,     .vk = VK_SHIFT },
    { .btn = din_btn_kb_ctrl,      .vk = VK_CONTROL },
    { .btn = din_btn_kb_alt,       .vk = VK_MENU },
    { .btn = din_btn_kb_super,     .vk = VK_LWIN },
    { .btn = din_btn_kb_a, .vk = 'A' }, { .btn = din_btn_kb_b, .vk = 'B' }, { .btn = din_btn_kb_c, .vk = 'C' },
    { .btn = din_btn_kb_d, .vk = 'D' }, { .btn = din_btn_kb_e, .vk = 'E' }, { .btn = din_btn_kb_f, .vk = 'F' },
    { .btn = din_btn_kb_g, .vk = 'G' }, { .btn = din_btn_kb_h, .vk = 'H' }, { .btn = din_btn_kb_i, .vk = 'I' },
    { .btn = din_btn_kb_j, .vk = 'J' }, { .btn = din_btn_kb_k, .vk = 'K' }, { .btn = din_btn_kb_l, .vk = 'L' },
    { .btn = din_btn_kb_m, .vk = 'M' }, { .btn = din_btn_kb_n, .vk = 'N' }, { .btn = din_btn_kb_o, .vk = 'O' },
    { .btn = din_btn_kb_p, .vk = 'P' }, { .btn = din_btn_kb_q, .vk = 'Q' }, { .btn = din_btn_kb_r, .vk = 'R' },
    { .btn = din_btn_kb_s, .vk = 'S' }, { .btn = din_btn_kb_t, .vk = 'T' }, { .btn = din_btn_kb_u, .vk = 'U' },
    { .btn = din_btn_kb_v, .vk = 'V' }, { .btn = din_btn_kb_w, .vk = 'W' }, { .btn = din_btn_kb_x, .vk = 'X' },
    { .btn = din_btn_kb_y, .vk = 'Y' }, { .btn = din_btn_kb_z, .vk = 'Z' },
    { .btn = din_btn_kb_0, .vk = '0' }, { .btn = din_btn_kb_1, .vk = '1' }, { .btn = din_btn_kb_2, .vk = '2' },
    { .btn = din_btn_kb_3, .vk = '3' }, { .btn = din_btn_kb_4, .vk = '4' }, { .btn = din_btn_kb_5, .vk = '5' },
    { .btn = din_btn_kb_6, .vk = '6' }, { .btn = din_btn_kb_7, .vk = '7' }, { .btn = din_btn_kb_8, .vk = '8' },
    { .btn = din_btn_kb_9, .vk = '9' },
};
 
static void din_win__query_keyboard(din_input_state* out) {
    for (size_t i = 0; i < sizeof(din_win__key_map) / sizeof(din_win__key_map[0]); i++) {
        out->buttons[din_win__key_map[i].btn] = (GetAsyncKeyState(din_win__key_map[i].vk) & 0x8000) ? 1 : 0;
    }
}
 
static void din_win__query_mouse(din_slot* slot, din_input_state* out) {
    out->buttons[din_btn_ms_left]    = (GetAsyncKeyState(VK_LBUTTON)  & 0x8000) ? 1 : 0;
    out->buttons[din_btn_ms_right]   = (GetAsyncKeyState(VK_RBUTTON)  & 0x8000) ? 1 : 0;
    out->buttons[din_btn_ms_middle]  = (GetAsyncKeyState(VK_MBUTTON)  & 0x8000) ? 1 : 0;
    out->buttons[din_btn_ms_back]    = (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) ? 1 : 0;
    out->buttons[din_btn_ms_forward] = (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) ? 1 : 0;
 
    POINT pos;
    if (!GetCursorPos(&pos)) return;
 
    if (!slot->mouse_initialized) {
        slot->last_mouse_pos = pos;
        slot->mouse_initialized = 1;
    }
 
    out->axes[din_axis_ms_x] = (float)(pos.x - slot->last_mouse_pos.x) * slot->mouse_sensitivity;
    out->axes[din_axis_ms_y] = (float)(pos.y - slot->last_mouse_pos.y) * slot->mouse_sensitivity;
    slot->last_mouse_pos = pos;
}
 
static void din_win__query_xinput(din_slot* slot, din_input_state* out) {
    XINPUT_STATE state = {0};
    if (XInputGetState(slot->xinput_index, &state) != ERROR_SUCCESS) {
        slot->connected = 0; // controller unplugged
        return;
    }
 
    WORD b = state.Gamepad.wButtons;
    out->buttons[din_btn_gp_north] = (b & XINPUT_GAMEPAD_Y) ? 1 : 0;
    out->buttons[din_btn_gp_east]  = (b & XINPUT_GAMEPAD_B) ? 1 : 0;
    out->buttons[din_btn_gp_south] = (b & XINPUT_GAMEPAD_A) ? 1 : 0;
    out->buttons[din_btn_gp_west]  = (b & XINPUT_GAMEPAD_X) ? 1 : 0;
    out->buttons[din_btn_gp_lb]    = (b & XINPUT_GAMEPAD_LEFT_SHOULDER)  ? 1 : 0;
    out->buttons[din_btn_gp_rb]    = (b & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 1 : 0;
    out->buttons[din_btn_gp_l3]    = (b & XINPUT_GAMEPAD_LEFT_THUMB)  ? 1 : 0;
    out->buttons[din_btn_gp_r3]    = (b & XINPUT_GAMEPAD_RIGHT_THUMB) ? 1 : 0;
    out->buttons[din_btn_gp_start] = (b & XINPUT_GAMEPAD_START) ? 1 : 0;
    out->buttons[din_btn_gp_select]= (b & XINPUT_GAMEPAD_BACK)  ? 1 : 0;
    out->buttons[din_btn_gp_dpad_up]    = (b & XINPUT_GAMEPAD_DPAD_UP)    ? 1 : 0;
    out->buttons[din_btn_gp_dpad_right] = (b & XINPUT_GAMEPAD_DPAD_RIGHT) ? 1 : 0;
    out->buttons[din_btn_gp_dpad_down]  = (b & XINPUT_GAMEPAD_DPAD_DOWN)  ? 1 : 0;
    out->buttons[din_btn_gp_dpad_left]  = (b & XINPUT_GAMEPAD_DPAD_LEFT)  ? 1 : 0;
    // XInput doesn't expose the Guide/Home button; din_btn_gp_home stays 0.
 
    out->buttons[din_btn_gp_lt] = (state.Gamepad.bLeftTrigger  > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? 1 : 0;
    out->buttons[din_btn_gp_rt] = (state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? 1 : 0;
 
    float lx = state.Gamepad.sThumbLX / 32767.0f;
    float ly = state.Gamepad.sThumbLY / 32767.0f;
    float rx = state.Gamepad.sThumbRX / 32767.0f;
    float ry = state.Gamepad.sThumbRY / 32767.0f;
    float lt = state.Gamepad.bLeftTrigger  / 255.0f;
    float rt = state.Gamepad.bRightTrigger / 255.0f;
 
    out->axes[din_axis_gp_lx] = din_win__apply_deadzone(lx, slot->axis_deadzone[din_axis_gp_lx]);
    out->axes[din_axis_gp_ly] = din_win__apply_deadzone(ly, slot->axis_deadzone[din_axis_gp_ly]);
    out->axes[din_axis_gp_rx] = din_win__apply_deadzone(rx, slot->axis_deadzone[din_axis_gp_rx]);
    out->axes[din_axis_gp_ry] = din_win__apply_deadzone(ry, slot->axis_deadzone[din_axis_gp_ry]);
    out->axes[din_axis_gp_lt] = din_win__apply_deadzone(lt, slot->axis_deadzone[din_axis_gp_lt]);
    out->axes[din_axis_gp_rt] = din_win__apply_deadzone(rt, slot->axis_deadzone[din_axis_gp_rt]);
}
 
static void din_win__query_dinput(din_slot* slot, din_input_state* out) {
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
    out->buttons[din_btn_gp_south] = (js.rgbButtons[0]  & 0x80) ? 1 : 0;
    out->buttons[din_btn_gp_east]  = (js.rgbButtons[1]  & 0x80) ? 1 : 0;
    out->buttons[din_btn_gp_west]  = (js.rgbButtons[2]  & 0x80) ? 1 : 0;
    out->buttons[din_btn_gp_north] = (js.rgbButtons[3]  & 0x80) ? 1 : 0;
    out->buttons[din_btn_gp_lb]    = (js.rgbButtons[4]  & 0x80) ? 1 : 0;
    out->buttons[din_btn_gp_rb]    = (js.rgbButtons[5]  & 0x80) ? 1 : 0;
    out->buttons[din_btn_gp_lt]    = (js.rgbButtons[6]  & 0x80) ? 1 : 0;
    out->buttons[din_btn_gp_rt]    = (js.rgbButtons[7]  & 0x80) ? 1 : 0;
    out->buttons[din_btn_gp_select]= (js.rgbButtons[8]  & 0x80) ? 1 : 0;
    out->buttons[din_btn_gp_start] = (js.rgbButtons[9]  & 0x80) ? 1 : 0;
    out->buttons[din_btn_gp_l3]    = (js.rgbButtons[10] & 0x80) ? 1 : 0;
    out->buttons[din_btn_gp_r3]    = (js.rgbButtons[11] & 0x80) ? 1 : 0;
 
    // D-pad via POV hat (hundredths of a degree; 0xFFFF == centered/none)
    DWORD pov = js.rgdwPOV[0];
    if (LOWORD(pov) != 0xFFFF) {
        out->buttons[din_btn_gp_dpad_up]    = (pov > 31500 || pov < 4500);
        out->buttons[din_btn_gp_dpad_right] = (pov > 4500  && pov < 13500);
        out->buttons[din_btn_gp_dpad_down]  = (pov > 13500 && pov < 22500);
        out->buttons[din_btn_gp_dpad_left]  = (pov > 22500 && pov < 31500);
    }
 
    float lx = js.lX  / 1000.0f;
    float ly = js.lY  / 1000.0f;
    float rx = js.lRx / 1000.0f;
    float ry = js.lRy / 1000.0f;
    // Many pads surface triggers on the Z/RZ axes; best-effort 0..1 remap.
    float lt = (js.lZ  + 1000.0f) / 2000.0f;
    float rt = (js.lRz + 1000.0f) / 2000.0f;
 
    out->axes[din_axis_gp_lx] = din_win__apply_deadzone(lx,  slot->axis_deadzone[din_axis_gp_lx]);
    out->axes[din_axis_gp_ly] = din_win__apply_deadzone(-ly, slot->axis_deadzone[din_axis_gp_ly]); // invert to match XInput's +up
    out->axes[din_axis_gp_rx] = din_win__apply_deadzone(rx,  slot->axis_deadzone[din_axis_gp_rx]);
    out->axes[din_axis_gp_ry] = din_win__apply_deadzone(-ry, slot->axis_deadzone[din_axis_gp_ry]);
    out->axes[din_axis_gp_lt] = din_win__apply_deadzone(lt,  slot->axis_deadzone[din_axis_gp_lt]);
    out->axes[din_axis_gp_rt] = din_win__apply_deadzone(rt,  slot->axis_deadzone[din_axis_gp_rt]);
}
 
void din_slot_query_input_state(din_slot* slot, din_input_state* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!slot || !slot->connected) return;
 
    if (slot->type == din_device_keyboard) { din_win__query_keyboard(out); return; }
    if (slot->type == din_device_mouse)    { din_win__query_mouse(slot, out); return; }
    if (slot->type != din_device_gamepad) return;
 
    if (slot->gp_backend == din_win_gp_backend_xinput) din_win__query_xinput(slot, out);
    else din_win__query_dinput(slot, out);
}

#else
    #error No OS info provided for demiurg inputs!
#endif // OS IF
#endif // DEMIURG_INPUTS_IMPL
