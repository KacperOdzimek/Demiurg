# Light User Interface System

## Overview

Light user interface system is an immediate-mode UI layout and rendering library built around a declarative tree of **nodes**. Users describe their UI as a hierarchy of typed nodes, pass the root to `lui_measure` and then `lui_render`, and consume the resulting draw commands to paint their frame.

The library is a single-header file activated by the `LIGHT_USER_INTERFACE_IMPL` macro and uses an arena allocator model — no hidden heap allocations during a frame.

---

## Core Architecture — The Node Tree

### Concept

Every piece of UI in lui is represented as a `lui_node`. Nodes are composed into a tree. A node may have one or multiple children, based on type.

```
Root Node
 └─ Container (row/column)
     ├─ Child A (box + text)
     └─ Child B (box)
```

The tree is evaluated in two sequential passes each frame:

**Pass 1 — Measure (`lui_measure`)**  
Traverses the tree bottom-up, computing the desired dimensions of each node. Measurement functions for text are injected by the host application.

**Pass 2 — Render (`lui_render`)**  
Traverses the tree top-down with a known screen resolution, resolves flex sizing, and emits a flat list of `lui_draw_command` records into an arena. The resulting commands and input boxes are depth-sorted from deepest to topmost.

**Input (`lui_input`)**  
After rendering, call `lui_input` with the populated input-boxes arena to dispatch cursor events to registered handlers.

You can render UI yourself by walking the generated draw commands arena, or use the `user_interface_rendering.h` pipeline.

### The Frame Loop

```c
// 1. Measure — compute desired sizes
lui_measure(&root_node, &temp_arena, user_context);

// 2. Render — resolve layout and emit draw commands
lui_render(&root_node, &temp_arena,
           screen_w, screen_h,
           &commands_arena,
           &clipboxes_arena,
           &input_boxes_arena,
           1 /* clear_input_boxes */);

// 3. Draw — iterate commands_arena and render each lui_draw_command

// 4. Input — dispatch cursor/click events
lui_input(&input_boxes_arena, &clipboxes_arena, &input_state, delta_time);
```

### Arenas

lui uses `lui_arena` — a simple bump allocator — for all frame memory. Four arenas are required at render time:

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

### Queries

| Type | Children | Data | Description |
|---|---|---|---|
| `lui_node_measure_size_query` | single | `lui_measure_size_query_data*` | After measuring its subtree, writes the child's measured `width` and `height` (`lui_length`) into the data struct. Useful for reading layout dimensions at measure time. |
| `lui_node_render_size_query` | single | `lui_render_size_query_data*` | During the render pass, writes the resolved pixel `width` and `height` (`float`) into the data struct. Useful for reading the actual rendered size of a subtree. |

### Transform

| Type | Children | Data | Description |
|---|---|---|---|
| `lui_node_transform` | single | `lui_transform_data` | Applies a 2×3 affine matrix to child measure and/or render passes, controlled by the two flags in `lui_transform_data`. |

### Rendering Modifiers

| Type | Children | Data | Description |
|---|---|---|---|
| `lui_node_clipbox` | single | — | Constrains rendering of its subtree to its own resolved rectangle. Pair with `lui_node_sizebox` for precise clipping. |
| `lui_node_depth` | single | `lui_depth_data` | Shifts the depth value of all commands in the subtree. Decreasing depth moves elements visually *into* the screen. |
| `lui_node_offset` | single | `lui_offset_data` | Offsets child rendering by a fixed number of pixels (`offset_x`, `offset_y`) without affecting measurement. |

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

Handlers are called from the topmost (highest depth) box downward. To prevent click-through, consume the relevant state on `input_state` inside an upper handler so lower handlers do not see it.

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
| `lui_node_box` | single | `lui_box_data` | Renders a rectangle with a color tint, an optional image, and an optional shader index. |
| `lui_node_text` | single | `lui_text_data` | Renders a text string at its measured size (queried via `lui_injection_measure_text`). |

### Extra Flags

Flags are OR-ed into the `type` field of any node:

| Flag | Effect |
|---|---|
| `lui_node_flag_ignore_min_width` | Forces the node's width minimum to 0 and enables width flex. Equivalent to wrapping in a sizebox that overwrites `width.min`. Useful when paired with `lui_node_clipbox` to allow shrinking below content minimum. |
| `lui_node_flag_ignore_min_height` | Same as above for the height axis. |
| `lui_node_flag_ignore_max_width` | Forces the node's width maximum to `lui_inf_length` and enables width flex. Allows expanding beyond content maximum in width. |
| `lui_node_flag_ignore_max_height` | Same as above for the height axis. |
| `lui_node_flag_data_instanced` | Read node data from the active instance pointer at `data_instance_offset` instead of `node->data`. See §Instancing. |
| `lui_node_flag_child_instanced` | Read node child/child_array from the active instance pointer at `child_instance_offset` instead of `node->child`. See §Instancing. |

Example:

```c
lui_node my_box = {
    .type  = lui_node_box | lui_node_flag_ignore_max_width | lui_node_flag_ignore_max_height,
    .child = NULL,
    .data  = &box_style
};
```

## Node Sizes

Most nodes, by default, inherit their children's size. Only some push their own requirements:

```
lui_node_text    - requires space to fit all text
lui_node_sizebox - allows user to push own requirements
```

If a user were to render the following widget:

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

A box tightly wrapping the rendered text would appear.

If the user were to add `lui_node_flag_ignore_max_width | lui_node_flag_ignore_max_height` flags to the box type, the box would render across the entire screen/given space.

---

## Instancing

Instancing allows a single static node tree to be rendered multiple times with different data — useful for lists, reusable components, and dynamic content — without duplicating the tree structure at compile time.

### How It Works

1. Define an instance struct — an arbitrary structure with the fields you want to vary per instance.
2. Define a UI tree. On nodes that should vary per-instance, add `lui_node_flag_data_instanced` and/or `lui_node_flag_child_instanced` to their `type`.
3. If instancing data, instead of `node->data`, set `node->data_instance_offset` to the byte offset of the relevant field inside your instance struct (`offsetof(MyInstance, my_field)`).
4. If instancing child, instead of `node->child`, set `node->child_instance_offset` to the byte offset of the child pointer/child array in the instance struct.
5. Wire it up with `lui_node_instance` — set its child to the element root and its data to a pointer to your instance struct. All instanced fields in the subtree will then be read from that struct.

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

// Using the reusable widget

ListItemInstance my_instance = {
    .label = {
        .text = "An apple",
        // ...
    },
    .bg = {
        .tint = LUI_HEX("#FF00FF"),
        // ...
    },
};

static const lui_node item_root = {
    .type  = lui_node_instance,
    .child = &item_box,     // root of the reusable component
    .data  = &my_instance   // component will pull data from this struct instance
};
```

This behavior works for all nodes regardless of data type. Instances can even be chained — you can instance the `data` of a `lui_node_instance` itself.

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

Rotation direction defaults to clockwise. Define `LUI_IMPL_INVERT_ROTATION` before including the header to flip it.

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

lui delegates platform-specific concerns to the host application via injected functions:

| Function | When | Purpose |
|---|---|---|
| `lui_injection_measure_text` | Measure pass | Returns measured width/height of a `lui_node_text`. |
| `lui_injection_query_cursor_position` | Input pass | Provides pixel and normalised cursor coordinates. |
| `lui_injection_query_cursor_state` | Input pass | Provides button states and scroll delta. Pass `check_consumed = 1` to skip already-consumed state. |
| `lui_injection_consume_cursor_state` | Input pass | Marks left button, right button, and/or scroll as consumed so deeper handlers ignore them. |
| `lui_injection_query_previous_cursor_state` | Input pass | Provides button states and scroll delta from the previous frame. |

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
