# Light Input System

## Overview

Light input system is a C input library for reading state from mice, keyboards, and gamepads. It abstracts OS-level device access behind a uniform slot-based API: enumerate visible devices, connect one to a slot, and poll its state each frame.

---

## Core Concepts

### Library

The `lin_library` is the root context. Create one instance for the lifetime of your application.

```c
lin_library* lib = lin_create_library();
// ...
lin_free_library(lib);
```

### Devices List

A snapshot of all OS-visible input devices at a given moment. Query it whenever you need to (re)enumerate devices — on startup, or when a device is plugged in.

```c
lin_devices_list* devices = lin_library_get_devices_list(lib);
if (!devices) { /* failed to access devices */ }

size_t count = lin_devices_list_get_size(devices);
for (size_t i = 0; i < count; i++) {
    lin_device_type type = lin_devices_list_query_type(devices, i);
    const char*     name = lin_devices_list_query_name(devices, i);
}

lin_free_devices_list(devices);
```

Device types:

| Value | Device |
|---|---|
| `lin_device_unknown` | Unrecognised device |
| `lin_device_mouse` | Mouse |
| `lin_device_keyboard` | Keyboard |
| `lin_device_gamepad` | Gamepad / controller |

### Slots

A `lin_slot` represents a logical input channel. Connect a physical device to it, then poll that slot every frame. Slots are independent — for a four-player local game, create four slots and connect one gamepad to each.

```c
lin_slot* slot = lin_create_slot(lib);

// Connect device at index 2 from the current devices list
int ok = lin_slot_connect(slot, devices, 2);

// Check connection status at any time
if (lin_slot_connected(slot)) { ... }

// Disconnect manually
lin_slot_disconnect(slot);

lin_free_slot(slot);
```

`lin_slot_connect` returns `1` on success, `0` on failure. `lin_slot_connected` returns `0` both if the slot was never connected and if a previously connected device was lost (e.g. unplugged).

A disconnected slot always returns a zeroed `lin_input_state` — no special-casing needed in game logic.

---

## Polling Input

Call `lin_slot_query_input_state` once per frame to get the current device state:

```c
lin_input_state state;
lin_slot_query_input_state(slot, &state);

// Read a button
if (state.buttons[lin_btn_kb_space]) { /* jump */ }

// Read an axis
float lx = state.axes[lin_axis_gp_lx];
```

`lin_input_state` holds two flat arrays:

```c
typedef struct lin_input_state {
    char  buttons[lin_btn_count];   // non-zero = pressed
    float axes   [lin_axis_count];  // see ranges per axis below
} lin_input_state;
```

Index `state.buttons[]` with any `lin_button` enum value.
Index `state.axes[]` with any `lin_axis` enum value.

Due to possible expansion full list of button and axes is declared in header file and not included here.
Each axis is also given a comment on it's range.

### Mouse Sensitivity

Mouse movement is converted from raw OS units to the `lin_axis_ms_x/y` values by a sensitivity factor. The default factor is 1.0; adjust it per slot:

```c
lin_slot_set_mouse_sensitivity(slot, 0.5f);  // slower
lin_slot_set_mouse_sensitivity(slot, 2.0f);  // faster
```

### Axis Deadzone

Apply a per-axis deadzone to filter stick drift. The deadzone value is in `[0, 1]`:

- `0.0` — no deadzone, raw value passed through.
- `1.0` — binary; axis is either `0.0` or fully pressed (`1.0` / `±1.0`).
- Values in between — below the threshold the axis returns `0.0`; above it the value is rescaled towards full press.

```c
// Ignore small stick drift up to 15 %
lin_slot_set_axis_deadzone(slot, lin_axis_gp_lx, 0.15f);
lin_slot_set_axis_deadzone(slot, lin_axis_gp_ly, 0.15f);
```

---

## Typical Usage Pattern

```c
// Startup
lin_library*      lib    = lin_create_library();
lin_slot*         slot   = lin_create_slot(lib);
lin_devices_list* devs   = lin_library_get_devices_list(lib);

// Find and connect the first gamepad
for (size_t i = 0; i < lin_devices_list_get_size(devs); i++) {
    if (lin_devices_list_query_type(devs, i) == lin_device_gamepad) {
        if (lin_slot_connect(slot, devs, i)) break;
        // if failed, continue
    }
}
lin_free_devices_list(devs);

lin_slot_set_axis_deadzone(slot, lin_axis_gp_lx, 0.12f);
lin_slot_set_axis_deadzone(slot, lin_axis_gp_ly, 0.12f);

// Game loop
while (running) {
    lin_input_state state;
    lin_slot_query_input_state(slot, &state);

    if (!lin_slot_connected(slot)) { /* show "reconnect controller" UI */ }

    float move_x = state.axes[lin_axis_gp_lx];
    float move_y = state.axes[lin_axis_gp_ly];
    int   jump   = state.buttons[lin_btn_gp_south];
}

// Shutdown
lin_free_slot(slot);
lin_free_library(lib);
```
