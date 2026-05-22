# Light User Interface System

## Overview

Light user inteface system is an immediate-mode UI layout and rendering library built around a declarative tree of **nodes**. User describe their UI as a hierarchy of typed nodes, pass the root to `lui_measure` and then `lui_render`, and consume the resulting draw commands to paint their frame.

The library is a single-header file activated by the `LIGHT_FONT_IMPL` macro and uses an arena allocator model — no hidden heap allocations during a frame.

---

## Core Architecture — The Node Tree

### Concept

Every piece of UI in lui is represented as a `lui_node`. Nodes are composed into a tree. A node may have one or multiple child, based on type. 

```
Root Node
 └─ Container (row/column)
     ├─ Child A (box + text)
     └─ Child B (image)
```

The tree is evaluated in two sequential passes each frame:

**Pass 1 — Measure (`lui_measure`)**  
Traverses the tree bottom-up, computing the desired dimensions of each node. Measurement functions for images and text are injected by the host application.

**Pass 2 — Render (`lui_render`)**  
Traverses the tree top-down with a known screen resolution, resolves flex sizing, and emits a flat list of `lui_draw_command` records into an arena. The resulting commands and input boxes are depth-sorted from deepest to topmost.

**Input (`lui_input`)**  
After rendering, call `lui_input` with the populated input-boxes arena to dispatch cursor events to registered handlers.

You can render UI yourself by walking generated draw commands arena, or use user_interface_rendering.h pipeline.

### The Frame Loop

```c
// 1. Measure — compute desired sizes
lui_measure(&root_node, &temp_arena, user_context);

// 2. Render — resolve layout and emit draw commands
lui_render(&root_node, &temp_arena,
           screen_w, screen_h,
           &commands_arena,
           &clipboxes_arena,
           &input_boxes_arena);

// 3. Draw — iterate commands_arena and render each lui_draw_command, or use user_interface_rendering.h

// 4. Input — dispatch cursor/click events
lui_input(&input_boxes_arena, &clipboxes_arena, &input_state, delta_time);
```

### Arenas

lui uses `lui_arena` — a simple bump allocator — for all frame memory. Three arenas are required at render time:

| Arena | Purpose |
|---|---|
| `temp_arena` | Scratch space during measure and render passes |
| `commands_arena` | Output `lui_draw_command` list |
| `clipboxes_arena` | Scissor rectangles referenced by draw commands |
| `input_boxes_arena` | Hit-test regions for input dispatch (optional) |

Arenas can be reused across frames. Resize them with `lui_arena_resize` and free with `lui_arena_free`.

### Lengths

Layout is controlled with `lui_length`, a 1D constraint:

```c
typedef struct lui_length {
    int   min;   // minimum size the element may occupy
    int   max;   // maximum size the element may occupy
    float flex;  // relative grow rate among siblings
} lui_length;
```

`lui_inf_length` (128 000 px) represents an unconstrained axis. Flex values work like CSS `flex-grow`: siblings with higher flex values claim proportionally more of available free space.

---

## Node Types

Every node is an instance of `lui_node`:

```c
typedef struct lui_node {
    lui_node_type             type;
    union { child / child_array / child_instance_offset };
    union { data  / data_instance_offset };
} lui_node;
```

Nodes are either **single-childed** (hold one `const lui_node*`) or **multi-childed** (hold a `const lui_node_array*`). The table below lists all built-in types.

### Architectural

| Type | Children | Data | Description |
|---|---|---|---|
| `lui_node_instance` | single | instance pointer (`void*`) | Sets the active *instance pointer* carried into the subtree. Enables data and child instancing (see §Instancing). |

### Transform

| Type | Children | Data | Description |
|---|---|---|---|
| `lui_node_transform` | single | `lui_transform_data` | Applies a 2×3 affine matrix to child measure and/or render passes, controlled by the two flags in `lui_transform_data`. |

### Rendering Modifiers

| Type | Children | Data | Description |
|---|---|---|---|
| `lui_node_clipbox` | single | — | Constrains rendering of its subtree to its own resolved rectangle. Pair with `lui_node_sizebox` for precise clipping. |
| `lui_node_depth` | single | `lui_depth_data` | Shifts the depth value of all commands in the subtree. Decreasing depth moves elements visually *into* the screen. |

### Input

| Type | Children | Data | Description |
|---|---|---|---|
| `lui_node_input_handle` | single | `lui_input_handler_func` | Registers a callback for input events occurring inside descendant `lui_node_input_box` nodes. |
| `lui_node_input_box` | single | arbitrary user pointer/instance offset | Defines a hit-test region. When the cursor intersects it, the nearest ancestor `lui_node_input_handle` callback is invoked with this node's data pointer. |

The input handler signature:

```c
typedef void (*lui_input_handler_func)(
    void*                      input_box_data,
    lui_injection_input_state* input_state,
    int                        cursor_inside,
    float                      delta_time
);
```

Handlers are called from the topmost (highest depth) box downward. To prevent click-through, clear the relevant flag on `input_state` inside an upper handler so lower handlers do not see it.

### Basic Layout

| Type | Children | Data | Description |
|---|---|---|---|
| `lui_node_padding` | single | `lui_padding_data` | Insets children by independent left/right/top/bottom lengths. |
| `lui_node_sizebox` | single | `lui_sizebox_data` | Overrides specific length fields (min, max, flex) of its child on either or both axes, selected by a bitmask flag. |

`lui_sizebox_overwrite_flag` lets you surgically overwrite only the fields you need:

```c
// Overwrite only the maximum width and all height fields:
lui_sizebox_overwrite_width_max | lui_sizebox_overwrite_all_height
```

### Containers

| Type | Children | Data | Description |
|---|---|---|---|
| `lui_node_row` | array | `lui_row_data` | Lays children out horizontally. Supports `horizontal_align`, `vertical_align`, and inter-child `spacing`. |
| `lui_node_column` | array | `lui_column_data` | Lays children out vertically. Supports `vertical_align`, `horizontal_align`, and inter-child `spacing`. |

Alignment values follow a 0 → 1 range: `0` = start (left/top), `0.5` = center, `1` = end (right/bottom). Values outside `[0, 1]` are valid and produce over-aligned layouts.

### Primitives

| Type | Children | Data | Description |
|---|---|---|---|
| `lui_node_box` | single | `lui_box_data` | Renders a solid-color rectangle with an optional shader index.
| `lui_node_image` | single | `lui_image_data` | Renders a tinted texture. |
| `lui_node_sized_image` | single | `lui_image_data` | Renders a texture at its intrinsic size (queried via the injected `lui_injection_measure_sized_image`), growing only if child content demands it. |
| `lui_node_text` | single | `lui_text_data` | Renders a text string at its measured size (queried via `lui_injection_measure_text`). |

### Extra Flags

Flags are OR-ed into the `type` field of any node:

| Flag | Effect |
|---|---|
| `lui_node_flag_fill` | Forces the node to fill all available space on both axes, equivalent to setting `max` to `lui_inf_length` via a sizebox. |
| `lui_node_flag_data_instanced` | Read node data from the active instance pointer at `data_instance_offset` instead of `node->data`. |
| `lui_node_flag_child_instanced` | Read node child/child_array from the active instance pointer at `child_instance_offset` instead of `node->child`. |

Example:

```c
lui_node my_box = {
    .type  = lui_node_box | lui_node_flag_fill,
    .child = NULL,
    .data  = &box_style
};
```

## Node Sizes

Most nodes, by default inherit their children size. Only some pushes their own requirements:
```
lui_node_text        - requires space to fit all text
lui_node_sized_image - requires texture intrinsic size
lui_node_sizebox     - allows user to push own requirements
```
If user were to render following widget:
```c
lui_node txt = {
    .type  = lui_node_text,
    .child = NULL,
    .data  = ...
};

lui_node box = {
    .type  = lui_node_box,
    .child = &txt,
    .data  = ...
};
```
A box tightly wrapping rendering text would appear.  

If user were to add ``lui_node_flag_fill`` flag to box type:  ``lui_node_box | lui_node_flag_fill``, then box would render across entire screen/given space.

---

## Instancing

Instancing allows a single static node tree to be rendered multiple times with different data — useful for lists, reusable components, and dynamic content — without duplicating the tree structure at compile time.

### How It Works

1. Define a instance struct - an arbitrary structure with thing you want to set per instance
2. Define an UI tree. On nodes that should vary per-instance, add `lui_node_flag_data_instanced` and/or `lui_node_flag_child_instanced` to their `type`.
3. If instancing data, instead of `node->data`, set `node->data_instance_offset` to the byte offset of the relevant field inside your instance struct (`offsetof(MyInstance, my_field)`)
4. If instancing child, instead of `node->child` set``node->child_instance_offset`` to the byte offset of child pointer/child array in instance struct.
5. Instance your element with ``lui_node_instance`` - set child to element root and data to an instance of instance struct - now all instanced fields, instead of directly, will be read from instance struct

### Example

```c
// Reusable node tree (defined once, statically)
typedef struct ListItemInstance {
    lui_text_data  label;
    lui_box_data   bg;
} ListItemInstance;

static const lui_node item_text = {
    .type                 = lui_node_text | lui_node_flag_data_instanced,
    .child                = NULL,
    .data_instance_offset = offsetof(ListItemInstance, label)   // pull data from instance
};

static const lui_node item_box = {
    .type                 = lui_node_box | lui_node_flag_data_instanced,
    .child                = &item_text,
    .data_instance_offset = offsetof(ListItemInstance, bg)      // pull data from instance
};

// Using reusable widget

ListItemInstance my_instance = {
    .label = {
        .text = "An apple",
        (...)
    },
    .bg = {
        .color = LUI_HEX("#FF00FF"),
        (...)
    },
};

static const lui_node item_root = {
    .type  = lui_node_instance,
    .child = &item_box,     // since item_box is root of the reusable component
    .data  = &my_instance   // reusable component will now pull data from this struct instance
};
```

This behavior work for all nodes, despite ``data`` type. You can even chain instances and instance the ``lui_node_instance->data``.

### Child Instancing

`lui_node_flag_child_instanced` works identically but for the *child pointer* field. This allows the subtree itself to be swapped out per instance, enabling fully dynamic component composition from a static node skeleton.

---

## Transforms

lui uses a 2×3 affine matrix (`lui_transform`) for all positional operations:

```
| m00  m01  tx |
| m10  m11  ty |
```

Transforms can be built at runtime or compile time:

```c
// Runtime
lui_transform t = lui_trans(dx, dy, sx, sy, deg_cw);
t = lui_off(t, 10.0f, 0.0f);   // translate
t = lui_sca(t, 2.0f, 1.0f);    // scale
t = lui_rot(t, 45.0f);          // rotate CW

// Compile time (uses Taylor-series approximations for sin/cos)
static const lui_transform t = LUI_TRANS(dx, dy, sx, sy, deg_cw);
```

Rotation direction defaults to clockwise. Define `lui_IMPL_INVERT_ROTATION` before including the header to flip it.

---

## Colors

Colors are 32-bit RGBA (`lui_color`). Construct them from hex strings at runtime or compile time:

```c
// Runtime
lui_color red  = lui_hex("#FF0000");
lui_color semi = lui_hex("#FF000080");   // with alpha

// Compile time
static const lui_color red  = LUI_HEX("#FF0000");
static const lui_color semi = LUI_HEX("#FF000080");
```

The `#` prefix is required. Six hex digits set alpha to `0xFF`; eight hex digits read alpha explicitly.

---

## Injections

lui delegates two platform-specific concerns to the host application via injected functions:

| Function | When | Purpose |
|---|---|---|
| `lui_injection_measure_sized_image` | Measure pass | Returns intrinsic width/height of a `lui_node_sized_image`. |
| `lui_injection_measure_text` | Measure pass | Returns measured width/height of a `lui_node_text`. |
| `lui_injection_query_cursor_position` | Input pass | Provides normalised cursor coordinates in `[-1, 1]`. |
| `lui_injection_query_cursor_state` | Input pass | Provides button states and scroll delta. |

Define `LIGHT_USER_INTERFACE_IMPL` before including the header in exactly one translation unit to expose the measurement injection declarations.

---

## Return Codes

All main API functions return `lui_return_flag`:

| Value | Meaning |
|---|---|
| `lui_return_ok` | Success |
| `lui_return_temp_arena_too_small` | Increase `temp_arena` capacity |
| `lui_return_command_arena_too_small` | Increase `commands_arena` capacity |
| `lui_return_clip_boxes_arena_too_small` | Increase `clipboxes_arena` capacity |
| `lui_return_input_boxes_arena_too_small` | Increase `input_boxes_arena` capacity |
