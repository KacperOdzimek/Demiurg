# Light Graphics System

## Global Mental Model

Light Graphics System is a **Render Hardware Interface (RHI)**: a thin, API-agnostic abstraction over low-level graphics APIs. Its purpose is to let you write rendering code that does not depend on any specific graphics API, while still giving you explicit, low-level control over GPU resources.

### Objects dependency hierarchy

LGX objects form a strict **dependency hierarchy**. Every object is created from a parent, and that parent must remain alive for the entire lifetime of the child. Deleting a parent while children still reference it is undefined behaviour - with exceptions.

```
lgx_library
  └── lgx_hardware
        ├── lgx_cpu_signal
        ├── lgx_gpu_signal
        ├── lgx_window
        │     └── (render targets & layout — owned by window)
        ├── lgx_command_lists_allocator
        │     └── lgx_command_list (deleting parent definied - deletes all child lists)
        ├── lgx_staging_memory
        ├── lgx_buffer
        ├── lgx_sampler
        ├── lgx_texture
        ├── lgx_render_target_layout
        ├── lgx_render_target
        ├── lgx_descriptor_layout
        │     └── lgx_descriptor_allocator
        │           └── lgx_descriptor (deleting parent definied - deletes all child lists)
        ├── lgx_pipeline_descriptors_layout
        ├── lgx_shader
        └── lgx_pipeline
```

**Rule:** If object **A** was created using object **B**, then **B must not be freed before A**. For example, a `lgx_pipeline` holds references to a `lgx_render_target_layout` and a `lgx_pipeline_descriptors_layout` — both must outlive the pipeline.

### Ownership and Lifetime

- Every `lgx_create_*` function returns a heap-allocated opaque pointer. You own it and must free it with the corresponding `lgx_free_*` function.
- Objects created by an allocator (`lgx_command_list`, `lgx_descriptor`) are freed via their own free function, not by freeing the allocator. The allocator may be freed after all its children are freed.
- Window-owned render targets and their layout are **managed by the window**; do not free them manually.

### Synchronisation Model

LGX does **not** manage synchronisation for you. You are responsible for ensuring correct ordering of GPU work. The key principle is:

> **An object in use by the GPU must not be written to, freed, or re-recorded against until the GPU has finished using it.**

LGX provides two synchronisation primitives:

- `lgx_cpu_signal` (fence) — lets the CPU wait for the GPU to reach a point.
- `lgx_gpu_signal` (semaphore) — lets one GPU queue wait for another.

A typical frame loop uses a `lgx_cpu_signal` to know when a "frame in flight" slot is free before reusing its command list or staging memory.
The frames in flight boilerplate is abstracted inside ``synchronised_window.h`` - see it later.

Barrier/transition commands (`lgx_cmd_sync_buffers`, `lgx_cmd_sync_textures`) handle **resource state transitions** inside a command list. Always issue the correct sync command before changing how a resource is used (e.g. from transfer destination to shader read).

### Data Upload Pattern

LGX provides two ways to get data onto the GPU:

1. **Synchronous (slow):** `lgx_buffer_sync_upload` / `lgx_texture_sync_upload`. These are convenience helpers that block the CPU and create all intermediate objects internally. Useful for initialisation, not for per-frame uploads.
2. **Asynchronous (fast):** Manually allocate `lgx_staging_memory`, map it, write your data, then record `lgx_cmd_copy_staging_memory_to_buffer` or `lgx_cmd_copy_staging_memory_to_texture` into a command list and submit it. Issue sync commands around the copy to transition the resource into its final usage state.

### Rendering Flow

A typical frame follows this sequence:

```
lgx_window_update_input
lgx_window_acquire_next_render_target_index   → gpu_signal fires when image is ready
  [wait on cpu_signal for this frame slot]
  lgx_begin_command_list_recording
    lgx_cmd_sync_textures                     → transition to color_attachment
    lgx_gcmd_begin_render_target_write
      lgx_gcmd_bind_graphics_pipeline
      lgx_gcmd_bind_graphics_pipeline_vertex_buffer / index_buffer
      lgx_gcmd_bind_graphics_pipeline_descriptors
      lgx_gcmd_set_viewport / set_scissors
      lgx_gcmd_draw_vertices / draw_indexed
    lgx_gcmd_end_render_target_write
    lgx_cmd_sync_textures                     → transition to present
  lgx_finish_command_list_recording
lgx_submit_command_list                       → signals render_done gpu_signal, cpu_signal
lgx_window_enqueue_render_target_present      → waits on render_done gpu_signal
```

---

## Object & Function Reference

---

### `lgx_library`

The root object. Initialises the underlying graphics API and platform integration.

```c
lgx_library* lgx_create_library(const lgx_library_create_info*);
void         lgx_free_library(lgx_library*);
```

**Create info fields:**

| Field | Type | Description |
|---|---|---|
| `platform_code_enabled` | `int` | Non-zero to enable platform/window-system integration (required for windowed rendering). |

**Lifetime:** Must be the first object created and the last freed. All other objects depend on the hardware which depends on the library.

---

### `lgx_hardware`

Represents a physical GPU and its queues. There is typically one hardware object per application.

```c
lgx_hardware* lgx_create_hardware(lgx_library*, const lgx_hardware_create_info*);
void          lgx_free_hardware(lgx_hardware*);
```

**Create info fields:**

| Field | Type | Description |
|---|---|---|
| `require_presentation_queue` | `int` | Fail creation if no presentation queue is available. |
| `require_graphics_queues` | `int` | Fail creation if no graphics queues are available. |
| `desired_hardware_type` | `lgx_hardware_type` | Preference: `dont_mind`, `discrete`, or `integrated`. |
| `desired_graphics_queues` | `uint32_t` | How many graphics queues you would like. |
| `desired_transfer_compute_queues` | `uint32_t` | How many transfer+compute queues you would like. |
| `desired_transfer_queues` | `uint32_t` | How many transfer-only queues you would like. |
| `desired_compute_queues` | `uint32_t` | How many compute-only queues you would like. |

**Queue query:**

```c
uint32_t lgx_hardware_query_queues_count(lgx_hardware*, lgx_hardware_queue_type);
void     lgx_hardware_query_queues(lgx_hardware*, lgx_hardware_queue_type,
             uint32_t offset, uint32_t count, lgx_hardware_queue** out);
```

Use these after creation to retrieve actual `lgx_hardware_queue*` handles. The driver may grant fewer queues than desired.

**Hardware limits:**

```c
uint64_t lgx_hardware_query_limit(lgx_hardware*, lgx_hardware_limit);
```

Query implementation-specific limits (max texture dimensions, max descriptor counts, etc.) before creating resources that may exceed them. See `lgx_hardware_limit` enum for the full list.

**Idle wait:**

```c
void lgx_hardware_wait_idle(lgx_hardware*);
```

Blocks the CPU until all queues are idle. Use before freeing the hardware or during shutdown to ensure no GPU work is in flight.

---

### `lgx_cpu_signal` (Fence)

A CPU-side object that the GPU can signal when it finishes submitted work.

```c
lgx_cpu_signal* lgx_create_cpu_signal(lgx_hardware*, const lgx_cpu_signal_create_info*);
void            lgx_free_cpu_signal(lgx_cpu_signal*);
```

**Create info fields:**

| Field | Type | Description |
|---|---|---|
| `initialy_signaled` | `int` | Non-zero to create the signal in the already-signaled state. Useful when pre-signaling slots on the first frame so the wait does not deadlock. |

**Operations:**

```c
int  lgx_cpu_signal_signaled(lgx_cpu_signal*);  // non-zero if signaled
void lgx_cpu_signal_wait    (lgx_cpu_signal*);  // block CPU until signaled
void lgx_cpu_signal_reset   (lgx_cpu_signal*);  // un-signal for reuse
```

---

### `lgx_gpu_signal` (Semaphore)

A GPU-side synchronisation object. Used to order work between queues, or between rendering and presentation.

```c
lgx_gpu_signal* lgx_create_gpu_signal(lgx_hardware*);
void            lgx_free_gpu_signal(lgx_gpu_signal*);
```

Signals are passed to `lgx_submit_info::wait_gpu_signals` / `signal_gpu_signals` and to `lgx_window_acquire_next_render_target_index` / `lgx_window_enqueue_render_target_present`.

---

### `lgx_window`

Manages an OS window and the swapchain (a set of render targets backed by presentable images).

```c
lgx_window* lgx_create_window(lgx_hardware*, const lgx_window_create_info*);
void        lgx_free_window(lgx_window*);
```

**Create info fields:**

| Field | Type | Description |
|---|---|---|
| `title` | `const char*` | Window title string. |
| `width`, `height` | `uint32_t` | Initial window dimensions in pixels. |
| `desired_render_targets` | `uint32_t` | Desired number of frames in flight. The actual count may be lower — always query with `lgx_window_get_render_targets_count`. |
| `render_target_recreated_callback` | function pointer | Called whenever the swapchain is recreated (e.g. on resize). The `render_target_layout_changed` argument is non-zero if the format or layout also changed (requiring pipeline recreation). May be NULL. |

**Input & state:**

```c
void lgx_window_update_input(lgx_window*);          // call once per frame
int  lgx_window_query_shall_close(lgx_window*);     // non-zero = user closed window
void lgx_window_query_is_focused(lgx_window*, int* is);
void lgx_window_query_cursor_pos(lgx_window*, uint32_t* x, uint32_t* y);
void lgx_window_query_input(lgx_window*, int* left, int* right, float* scroll);
void lgx_window_get_size(lgx_window*, uint32_t* w, uint32_t* h);
```

**Swapchain / render target access:**

```c
uint32_t lgx_window_get_render_targets_count(lgx_window*);

// Acquire the next available swapchain image index.
// Signals can_render_signal (gpu signal) when the image is ready to render into.
uint32_t lgx_window_acquire_next_render_target_index(lgx_window*, lgx_gpu_signal* can_render_signal);

// Queue a present of the given render target index.
// Waits on all provided gpu signals before presenting.
void lgx_window_enqueue_render_target_present(lgx_window*, uint32_t index,
         uint32_t wait_count, lgx_gpu_signal** wait_signals);

lgx_render_target*        lgx_window_get_render_target(lgx_window*, uint32_t index);
lgx_render_target_layout* lgx_window_get_render_target_layout(lgx_window*);
```

**Important:** The render targets and layout returned by the window are **owned by the window**. Do not call `lgx_free_render_target` or `lgx_free_render_target_layout` on them. When the swapchain is recreated the pointers become invalid; retrieve fresh ones inside `render_target_recreated_callback`.

---

### `lgx_command_lists_allocator` & `lgx_command_list`

A command list allocator owns a pool of GPU command buffers. Command lists are recorded on the CPU and then submitted to a hardware queue.
Note command lists allocators, like all objects are externaly synchronised. 
That means you cannot alloc command lists at diffrent threads without synchronisation.
Even more - **you cannot record command lists from the same allocator at same time on diffrent threads**
You can create command lists allocator per thread.

```c
lgx_command_lists_allocator* lgx_create_command_lists_allocator(
    lgx_hardware*, const lgx_command_lists_allocator_create_info*);
void lgx_free_command_lists_allocator(lgx_command_lists_allocator*);

lgx_command_list* lgx_command_lists_allocator_alloc_command_list(lgx_command_lists_allocator*);
void              lgx_command_lists_allocator_free_command_list(lgx_command_list*);
```

**Create info fields:**

| Field | Type | Description |
|---|---|---|
| `target_queue_type` | `lgx_hardware_queue_type` | The queue type this allocator's lists will be submitted to. |
| `often_recorded` | `int` | Non-zero if lists will be re-recorded frequently (hint for internal pool strategy). |

**Recording:**

```c
void lgx_begin_command_list_recording(lgx_command_list*);
// ... record commands ...
void lgx_finish_command_list_recording(lgx_command_list*);
```

A command list must be in the finished state before submission. Do not record into a list that the GPU is currently executing.

**Submission:**

```c
void lgx_submit_command_list(lgx_hardware_queue*, const lgx_submit_info*);
```

`lgx_submit_info` fields:

| Field | Description |
|---|---|
| `command_lists` / `command_lists_count` | The lists to execute in order. |
| `wait_gpu_signals` / `wait_gpu_signals_count` | GPU signals that must be signaled before execution begins. |
| `signal_gpu_signals` / `signal_gpu_signals_count` | GPU signals to signal when execution completes. |
| `cpu_signal` | CPU signal (fence) to signal when execution completes. May be NULL. |

---

### `lgx_staging_memory`

CPU-accessible memory used as a staging area for uploading data to GPU-only buffers and textures.

```c
lgx_staging_memory* lgx_create_staging_memory(lgx_hardware*, const lgx_staging_memory_create_info*);
void                lgx_free_staging_memory(lgx_staging_memory*);
```

**Create info fields:**

| Field | Type | Description |
|---|---|---|
| `size_bytes` | `uint64_t` | Total size of the staging allocation. |

**Mapping:**

```c
void* lgx_staging_memory_map(lgx_staging_memory*, uint64_t region_offset, uint64_t region_size);
void  lgx_staging_memory_unmap(lgx_staging_memory*);
```

Map returns a CPU-writable pointer to the requested region. Unmap before submitting copy commands that read from this staging memory.

---

### `lgx_buffer`

A GPU buffer for vertices, indices, uniforms, or storage data.

```c
lgx_buffer* lgx_create_buffer(lgx_hardware*, const lgx_buffer_create_info*);
void        lgx_free_buffer(lgx_buffer*);
uint64_t    lgx_buffer_get_size_bytes(lgx_buffer*);
```

**Create info fields:**

| Field | Type | Description |
|---|---|---|
| `size_bytes` | `uint64_t` | Buffer size. |
| `usage` | `lgx_buffer_usage` | `vertex`, `index`, `uniform`, or `storage`. |
| `memory_strategy` | `lgx_memory_allocation_strategy` | `paged` (sub-allocate from a shared heap) or `dedicated` (own allocation). |
| `memory_access` | `lgx_memory_access` | `gpu_only` for device-local memory; `allow_staging_memory_and_buffer_copy_commands_for_read/write/read_and_write` to allow staging uploads/downloads. |

**Sync upload (slow path):**

```c
int lgx_buffer_sync_upload(lgx_buffer*, uint64_t offset, const void* data, uint64_t size);
```

Blocks the CPU until the write is complete. Returns non-zero on success. Use only during initialisation.

---

### `lgx_sampler`

Describes how a texture is sampled (filtering, wrapping, LOD).

```c
lgx_sampler* lgx_create_sampler(lgx_hardware*, const lgx_sampler_create_info*);
void         lgx_free_sampler(lgx_sampler*);
```

**Create info fields:**

| Field | Description |
|---|---|
| `mag_filter` / `min_filter` / `mipmap_filter` | `nearest` or `linear` for magnification, minification, and mipmap selection. |
| `x/y/z_coord_wrapping` | `repeat`, `repeat_mirrored`, `repeat_clamp_coordinates`, or `repeat_clamp_texture`. |
| `unnormalized_coordinates` | Non-zero to use pixel coordinates instead of [0,1]. |
| `min_lod` / `max_lod` / `mip_lod_bias` | LOD range and bias. |

---

### `lgx_texture`

A GPU texture of any type (1D, 2D, 3D, cubemap).

```c
lgx_texture* lgx_create_texture(lgx_hardware*, const lgx_texture_create_info*);
void         lgx_free_texture(lgx_texture*);
lgx_texture_dimensions lgx_texture_get_dimensions(lgx_texture*);
```

**Create info fields:**

| Field | Type | Description |
|---|---|---|
| `type` | `lgx_texture_type` | `1d`, `2d`, `3d`, or `cubemap`. |
| `usage` | `lgx_texture_usage` | `sampled`, `color_attachment`, `depth_stencil_attachment`, or `storage`. |
| `format` | `lgx_texture_format` | Pixel format (see format table below). |
| `dimensions` | `lgx_texture_dimensions` | Width (x), height (y), depth (z). |
| `array_length` | `uint32_t` | Number of array layers. |
| `mipmap_layers` | `uint32_t` | Number of mip levels. |
| `memory_strategy` | `lgx_memory_allocation_strategy` | `paged` or `dedicated`. |
| `memory_access` | `lgx_memory_access` | As per buffer (usually `gpu_only` for textures). |

**Supported texture formats:**

| Format | Description |
|---|---|
| `r8_unorm` / `rg8_unorm` / `rgba8_unorm` | 8-bit unsigned normalised |
| `rgba8_srgb` / `bgra8_unorm` / `bgra8_srgb` | sRGB and BGRA variants |
| `r16_float` / `rg16_float` / `rgba16_float` | 16-bit float |
| `r32_float` / `rg32_float` / `rgba32_float` | 32-bit float |
| `depth16_unorm` / `depth24_unorm_stencil8` / `depth32_float` | Depth/stencil |

**Sync upload (slow path):**

```c
int lgx_texture_sync_upload(lgx_texture*, lgx_texture_dimensions offset,
        const void* data, lgx_texture_dimensions write_dimensions);
```

Blocks the CPU. Returns non-zero on success. Use only during initialisation.

---

### `lgx_render_target_layout`

Describes the attachment formats and load/store behaviour of a render pass, without being bound to specific texture instances. Pipelines are compiled against a layout, not a specific render target.

```c
lgx_render_target_layout* lgx_create_render_target_layout(lgx_hardware*, const lgx_render_target_layout_create_info*);
void lgx_free_render_target_layout(lgx_render_target_layout*);
```

**Create info fields:**

| Field | Description |
|---|---|
| `color_attachments` / `color_attachments_count` | Array of color attachment descriptors. |
| `depth_stencil_attachment` | Pointer to a depth/stencil descriptor, or NULL. |

Each `lgx_render_target_layout_attachment` has:

| Field | Description |
|---|---|
| `format` | Texture format of the attachment. |
| `sample_count` | MSAA sample count (1 = no MSAA). |
| `load_op` | `load` (keep previous), `clear` (clear at pass start), `dont_care` (undefined, fastest). |
| `store_op` | `store` (preserve after pass), `dont_care` (discard, faster). |

**Note:** Window render target layouts are returned by `lgx_window_get_render_target_layout()` and must not be freed manually.

---

### `lgx_render_target`

Binds concrete textures to the slots described by a `lgx_render_target_layout`. This is the object you render into.

```c
lgx_render_target* lgx_create_render_target(lgx_hardware*, const lgx_render_target_create_info*);
void               lgx_free_render_target(lgx_render_target*);
```

**Create info fields:**

| Field | Description |
|---|---|
| `render_target_layout` | The layout this render target conforms to. Must stay alive. |
| `color_attachments` / `color_attachments_count` | Array of texture bindings for each color slot. |
| `depth_stencil_attachment` | Texture binding for depth/stencil, or NULL. |

Each `lgx_render_target_attachment` contains a single `lgx_texture*`.

**Note:** Window render targets are managed by the window and must not be freed manually.

---

### `lgx_descriptor_layout`

Declares the shape of a descriptor set: which bindings exist, what type they are, how many, and which shader stages can access them.

```c
lgx_descriptor_layout* lgx_create_descriptor_layout(lgx_hardware*, const lgx_descriptor_layout_create_info*);
void lgx_free_descriptor_layout(lgx_descriptor_layout*);
```

Each `lgx_descriptor_binding` in the array has:

| Field | Description |
|---|---|
| `binding` | Slot index (matches shader binding number). |
| `type` | `uniform_buffer`, `storage_buffer`, `sampled_texture`, or `sampler`. |
| `count` | Array count (1 for non-array). |
| `stages` | Bitmask of `lgx_shader_stages_bitmask` values. |

---

### `lgx_descriptor_allocator` & `lgx_descriptor`

Allocates concrete descriptor sets that bind actual resources (buffers, textures, samplers) to the slots defined by a layout.

```c
lgx_descriptor_allocator* lgx_create_descriptor_allocator(lgx_hardware*, const lgx_descriptor_allocator_create_info*);
void lgx_free_descriptor_allocator(lgx_descriptor_allocator*);

lgx_descriptor* lgx_descriptor_allocator_alloc_descriptor(lgx_descriptor_allocator*);
void            lgx_descriptor_allocator_free_descriptor(lgx_descriptor*);
```

**Create info fields:**

| Field | Description |
|---|---|
| `descriptor_layout` | The layout all descriptors from this allocator will conform to. |
| `max_descriptors_allocated` | Maximum live descriptors at once. |

**Writing resources into a descriptor:**

```c
void lgx_descriptors_write(lgx_hardware*, uint32_t count, lgx_descriptor_write_info*);
```

Each `lgx_descriptor_write_info` entry:

| Field | Description |
|---|---|
| `descriptor` | The descriptor to update. |
| `binding_type` | Must match the type declared in the layout. |
| `binding_index` | Which binding slot to write. |
| `array_element_index` / `array_elements_count` | Range within an array binding. |
| `infos.for_buffers` | Pointer to `lgx_descriptor_buffer_write_info` array (buffer + offset + length). |
| `infos.for_samplers` | Pointer to `lgx_descriptor_sampler_write_info` array. |
| `infos.for_sampled_textures` | Pointer to `lgx_descriptor_sampled_texture_write_info` array. |

**Important:** Do not update a descriptor while it is bound and the GPU is executing work that reads it. Use per-frame descriptor copies or wait on the appropriate `lgx_cpu_signal` first. **Also you cannot update descriptors after binding them in a command list - doing so invalidates the list**

---

### `lgx_pipeline_descriptors_layout`

Groups one or more `lgx_descriptor_layout` objects into a single pipeline-level descriptor space. Pipelines are created against this layout and descriptors are bound using it.

```c
lgx_pipeline_descriptors_layout* lgx_create_pipeline_descriptors_layout(
    lgx_hardware*, const lgx_pipeline_descriptors_layout_create_info*);
void lgx_free_pipeline_descriptors_layout(lgx_pipeline_descriptors_layout*);
```

**Create info fields:**

| Field | Description |
|---|---|
| `layouts` / `layouts_count` | Ordered array of `lgx_descriptor_layout*` pointers. Each layout occupies one descriptor set slot. |

---

### `lgx_shader`

A compiled shader module. The source format depends on the active backend (e.g. SPIR-V bytecode for Vulkan).

```c
lgx_shader* lgx_create_shader(lgx_hardware*, const lgx_shader_create_info*);
void        lgx_free_shader(lgx_shader*);
```

**Create info fields:**

| Field | Description |
|---|---|
| `source_code` | Pointer to the shader bytecode. |
| `source_size` | Size in bytes. |

Shaders may be freed after the pipeline that uses them has been created.

---

### `lgx_pipeline`

A fully compiled graphics pipeline: shaders, vertex layout, rasteriser state, blend state, depth/stencil state, and render target compatibility.

```c
lgx_pipeline* lgx_create_pipeline(lgx_hardware*, const lgx_pipeline_create_info*);
void          lgx_free_pipeline(lgx_pipeline*);
```

**Create info fields:**

| Field | Type | Description |
|---|---|---|
| `render_target_layout` | `lgx_render_target_layout*` | The pipeline is only compatible with render targets using this layout. |
| `descriptor_layout` | `lgx_pipeline_descriptors_layout*` | Descriptor set layout used by this pipeline. |
| `vertex_layout` | `lgx_pipeline_vertex_layout` | Vertex buffer bindings and attribute descriptions. |
| `shader_stages` | `lgx_pipeline_shader_stages` | `vertex`, `geometry` (optional), `pixel` shader modules. |
| `input_assembly` | `lgx_pipeline_input_assembly` | Primitive topology (`triangle_list`, etc.). |
| `rasterizer` | `lgx_pipeline_rasterizer_state` | Cull mode, fill mode, depth clamp, scissor enable. |
| `blend` | `lgx_pipeline_blend_state` | Per-target blend enable, blend op, src/dst factors. |
| `depth_stencil` | `lgx_pipeline_depth_stencil_state` | Depth test/write enable, stencil test enable. |

**Vertex layout details (`lgx_pipeline_vertex_layout`):**

Each binding (`lgx_vertex_input_binding_info`) defines a vertex buffer slot:

| Field | Description |
|---|---|
| `binding` | Binding index. |
| `stride` | Byte stride between elements. |
| `input_rate` | `per_vertex` or `per_instance`. |

Each attribute (`lgx_vertex_input_attribute_info`) maps a buffer binding to a shader input:

| Field | Description |
|---|---|
| `binding` | Which buffer binding this attribute reads from. |
| `location` | Shader input location. |
| `type` | Data type (`float32`, `vec2f32`, …, `vec4i32`, etc.). |
| `offset` | Byte offset within the stride. |

---

## Command Reference

All commands are recorded into a `lgx_command_list` between `lgx_begin_command_list_recording` and `lgx_finish_command_list_recording`.

---

### Synchronisation Commands

#### `lgx_cmd_sync_buffers`

```c
void lgx_cmd_sync_buffers(
    lgx_command_list*     target,
    lgx_buffer_sync_point previous_use,
    lgx_buffer_sync_point next_use,
    uint32_t              buffers_count,
    lgx_buffer**          buffers
);
```

Issues a pipeline barrier for the listed buffers, transitioning them from `previous_use` to `next_use`.

**Sync points:**

| Sync point | Meaning |
|---|---|
| `this_command` | Current position in the command list. |
| `transfer_source` / `transfer_destination` | Used as copy source/destination. |
| `compute_read` / `compute_write` | Read or written by a compute shader. |
| `vertex_shader_read` / `fragment_shader_read` | Read by the respective shader stage. |
| `vertex_buffer` / `index_buffer` | Consumed by fixed-function vertex/index fetch. |

#### `lgx_cmd_sync_textures`

```c
void lgx_cmd_sync_textures(
    lgx_command_list*      target,
    lgx_texture_sync_point previous_use,
    lgx_texture_sync_point next_use,
    uint32_t               textures_count,
    lgx_texture**          textures
);
```

Issues a pipeline barrier and layout transition for the listed textures.

**Sync points:**

| Sync point | Meaning |
|---|---|
| `this_command` | Current position in the command list. |
| `transfer_source` / `transfer_destination` | Copy source/destination layout. |
| `compute_read` / `compute_write` | Compute shader access. |
| `vertex_shader_read` / `fragment_shader_read` | Shader sampling. |
| `color_attachment` | Being written as a color attachment. |
| `depth_attachment` | Being written as a depth/stencil attachment. |

---

### Transfer Commands

#### `lgx_cmd_copy_staging_memory_to_buffer`

```c
void lgx_cmd_copy_staging_memory_to_buffer(
    lgx_command_list*   target,
    lgx_staging_memory* staging,
    lgx_buffer*         dst_buffer,
    uint32_t            staging_offset,
    uint32_t            buffer_offset,
    uint32_t            size
);
```

Copies `size` bytes from `staging` (at `staging_offset`) into `dst_buffer` (at `buffer_offset`). The buffer must have been transitioned to `transfer_destination` before this command, and should be transitioned to its final usage after.

#### `lgx_cmd_copy_staging_memory_to_texture`

```c
void lgx_cmd_copy_staging_memory_to_texture(
    lgx_command_list*      target,
    lgx_staging_memory*    staging,
    lgx_texture*           dst_texture,
    uint32_t               staging_offset,
    lgx_texture_dimensions texture_offset,
    lgx_texture_dimensions texture_size
);
```

Copies a region of staging memory into a sub-region of a texture. The texture must be in `transfer_destination` layout before this command.

---

### Graphics Commands

All `lgx_gcmd_*` commands must be recorded on a command list targeting a graphics queue, and most must appear between `lgx_gcmd_begin_render_target_write` / `lgx_gcmd_end_render_target_write`.

#### `lgx_gcmd_begin_render_target_write`

```c
void lgx_gcmd_begin_render_target_write(lgx_command_list*, lgx_gcmd_begin_render_target_write_info*);
```

Begins a render pass. Fields:

| Field | Description |
|---|---|
| `render_target` | The render target to draw into. |
| `clear_colors` / `clear_colors_count` | Clear values for each color attachment when `load_op` is `clear`. One `lgx_color` per attachment. |

#### `lgx_gcmd_end_render_target_write`

```c
void lgx_gcmd_end_render_target_write(lgx_command_list*);
```

Ends the current render pass.

#### `lgx_gcmd_bind_graphics_pipeline`

```c
void lgx_gcmd_bind_graphics_pipeline(lgx_command_list*, lgx_pipeline*);
```

Binds a compiled pipeline. Must be called before any draw commands.

#### `lgx_gcmd_bind_graphics_pipeline_vertex_buffer`

```c
void lgx_gcmd_bind_graphics_pipeline_vertex_buffer(
    lgx_command_list*, lgx_buffer*, uint32_t offset, uint32_t binding);
```

Binds a vertex buffer to a binding slot. `offset` is a byte offset into the buffer.

#### `lgx_gcmd_bind_graphics_pipeline_index_buffer`

```c
void lgx_gcmd_bind_graphics_pipeline_index_buffer(
    lgx_command_list*, lgx_buffer*, uint32_t offset, int uint32_not_uint16);
```

Binds an index buffer. Pass non-zero for `uint32_not_uint16` to use 32-bit indices.

#### `lgx_gcmd_bind_graphics_pipeline_descriptors`

```c
void lgx_gcmd_bind_graphics_pipeline_descriptors(
    lgx_command_list*,
    lgx_pipeline_descriptors_layout* layout,
    uint32_t first_descriptor_index,
    uint32_t descriptors_count,
    lgx_descriptor** descriptors
);
```

Binds an array of descriptors. `first_descriptor_index` is the starting set index in the pipeline layout. Descriptors must not be written to while bound and in use by the GPU.

#### `lgx_gcmd_set_viewport`

```c
void lgx_gcmd_set_viewport(lgx_command_list*, float x, float y, float width, float height);
```

Sets the rendering viewport. Must be called before drawing if the pipeline uses dynamic viewport state.

#### `lgx_gcmd_set_scissors`

```c
void lgx_gcmd_set_scissors(lgx_command_list*, float x, float y, float width, float height);
```

Sets the scissor rectangle. Only has effect if `scissor_enable` is non-zero in the pipeline's rasteriser state.

#### `lgx_gcmd_draw_vertices`

```c
void lgx_gcmd_draw_vertices(
    lgx_command_list*,
    uint32_t vertices_count,
    uint32_t vertices_buffer_offset_index,
    uint32_t instances_count,
    uint32_t instances_id_values_offset
);
```

Non-indexed draw. Draws `vertices_count` vertices starting at `vertices_buffer_offset_index`, repeated for `instances_count` instances.

#### `lgx_gcmd_draw_indexed`

```c
void lgx_gcmd_draw_indexed(
    lgx_command_list*,
    uint32_t indices_count,
    uint32_t indicies_buffer_offset_index,
    int32_t  indicies_values_offset,
    uint32_t instances_count,
    uint32_t instances_id_values_offset
);
```

Indexed draw. `indicies_buffer_offset_index` is the first index element in the index buffer. `indicies_values_offset` is added to each index value before fetching the vertex.

## Compile-Time Flags

| Macro | Effect |
|---|---|
| `LIGHT_GRAPHICS_IMPL` | Must be defined in exactly one translation unit to include the implementation. |
| `LIGHT_GRAPHICS_VULKAN` | Selects the Vulkan backend. |
| `LIGHT_GRAPHICS_VALIDATE` | Enables validation layers and error logging. Recommended during development, disable for release. |
