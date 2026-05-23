#ifndef LIGHT_USER_INTERFACE_PREFABS_H
#define LIGHT_USER_INTERFACE_PREFABS_H

#include "light/user_interface.h"

typedef struct luipf_button_data {
    lui_node*                   button_child;
    lui_box_data                default_style;
    lui_box_data                hovered_style;
    lui_box_data                pressed_style;
    lui_input_handler_func      on_clicked;
    lui_input_handler_func      on_released;
    lui_input_handler_func      on_held;
    void*                       data;
    int                         state_held;
    lui_box_data                state_style;
} luipf_button_data;

extern const lui_node luipf_button[];

typedef struct luipf_horizontal_scrollbox_data {

} luipf_horizontal_scrollbox_data;

extern const lui_node luipf_horizontal_scrollbox[];

typedef struct luipf_vertical_scrollbox_data {
    float                       scroll_speed_mod;
    lui_node*                   scrolled_child;
    lui_length                  handle_width;
    lui_node*                   handle_child;

    int                         state_offset;
    lui_offset_data             state_child_offset;
    lui_offset_data             state_handle_offset;
    lui_sizebox_data            state_handle_sizebox;
    lui_measure_size_query_data state_measure_size;
    lui_render_size_query_data  state_render_size;
} luipf_vertical_scrollbox_data;

extern const lui_node luipf_vertical_scrollbox[];

#endif // LIGHT_USER_INTERFACE_PREFABS_H

#ifdef LIGHT_USER_INTERFACE_PREFABS_IMPL

// Button

static void button_input_func(
    void*                       input_box_data,
    lui_injection_input_state*  input_state,
    int                         cursor_inside,
    float                       delta_time
) {
    luipf_button_data* data = (luipf_button_data*)input_box_data;
    int left_pressed; lui_injection_query_cursor_state(input_state, &left_pressed, NULL, NULL);

    // just clicked
    if (cursor_inside && left_pressed && !data->state_held) {
        data->state_held  = 1;
        data->state_style = data->pressed_style;
        if (data->on_clicked) data->on_clicked(data->data, input_state, cursor_inside, delta_time);
    }
    // held
    else if (cursor_inside && left_pressed && data->state_held) {
        data->state_style = data->pressed_style;
        if (data->on_held) data->on_held(data->data, input_state, cursor_inside, delta_time);
    }
    // released
    else if (data->state_held && !left_pressed) {
        data->state_held = 0;

        if (cursor_inside)  data->state_style = data->hovered_style;
        else                data->state_style = data->default_style;

        if (data->on_released) data->on_released(data->data, input_state, cursor_inside, delta_time);
    }
    // just hovered
    else if (cursor_inside) {
        data->state_style = data->hovered_style;
    }
    // default
    else {
        data->state_style = data->default_style;
    }
}

const lui_node luipf_button[] = {
    {
        .type  = lui_node_box | lui_node_flag_data_instanced,
        .child = &luipf_button[1],
        .data_instance_offset = offsetof(luipf_button_data, state_style)
    },
    {
        .type  = lui_node_input_handle,
        .child = &luipf_button[2],
        .data  = button_input_func
    },
    {
        .type  = lui_node_input_box | lui_node_flag_child_instanced | lui_node_flag_data_instanced,
        .child_instance_offset = offsetof(luipf_button_data, button_child),
        .data_instance_offset  = 0 // instance itself
    }
};

// Horizontal Scrollbox

// Vertical Scrollbox

static const float default_scroll_speed_vertical = 2000;

static void vertical_scrollbox_apply_scroll(luipf_vertical_scrollbox_data* data) {
    // sizes
    int content_height  = data->state_measure_size.height.min;
    int viewport_height = data->state_render_size.height;

    int offset_to_align = -content_height / 2;  // start offseting from align - hardcoded top
    int total_offset = offset_to_align + data->state_offset;

    // no scrolling needed
    if (content_height <= viewport_height) {
        total_offset = 0;
        data->state_offset = 0;
    } 
    // clamp
    else {
        int max_offset = (content_height - viewport_height) / 2;
        if (total_offset >  max_offset) {
            total_offset = max_offset;
            data->state_offset = max_offset - offset_to_align;
        }
        if (total_offset < -max_offset) {
            total_offset = -max_offset;
            data->state_offset = -max_offset - offset_to_align;
        }
    }

    data->state_child_offset.offset_y = total_offset;
}

static void vertical_scrollbox_scroll_func(
    void*                       input_box_data,
    lui_injection_input_state*  input_state,
    int                         cursor_inside,
    float                       delta_time
) {
    luipf_vertical_scrollbox_data* data = (luipf_vertical_scrollbox_data*)input_box_data;

    int pixels_change = 0;
    if (cursor_inside) {
        float scroll_dir; lui_injection_query_cursor_state(input_state, NULL, NULL, &scroll_dir);
        pixels_change = scroll_dir * delta_time * data->scroll_speed_mod * default_scroll_speed_vertical;
    }
    data->state_offset -= pixels_change;

    vertical_scrollbox_apply_scroll(data);
}

static void vertical_scrollbox_handle_func(
    void*                       input_box_data,
    lui_injection_input_state*  input_state,
    int                         cursor_inside,
    float                       delta_time
) {
    luipf_vertical_scrollbox_data* data = (luipf_vertical_scrollbox_data*)input_box_data;

    // set width to user desires
    data->state_handle_sizebox.flag |= lui_sizebox_overwrite_all_width;
    data->state_handle_sizebox.width = data->handle_width;

    // sizes
    float content_height  = data->state_measure_size.height.min;
    float viewport_height = data->state_render_size.height;

    // set height to visible part of widget
    data->state_handle_sizebox.flag |= lui_sizebox_overwrite_all_height;

    float visible_fraction = viewport_height / content_height;
    if (visible_fraction > 1.0f) visible_fraction = 1.0f; // clamp

    int height = viewport_height * visible_fraction;
    data->state_handle_sizebox.height.min = height;
    data->state_handle_sizebox.height.max = height;

    // position handle
    if (visible_fraction >= 1.0f) {
        data->state_handle_offset.offset_y = 0;
    }
    else {
        // find current lerp alpha of content between ends
        float begin = (content_height - viewport_height) / 2;
        float end   = -begin;
        float alpha = (data->state_child_offset.offset_y - begin) / (end -  begin);

        // apply alpha to handle movement
        begin = -(viewport_height / 2) + (height / 2);
        end   = -begin;
        data->state_handle_offset.offset_y = begin + (end - begin) * alpha;
    }
}

static lui_node vertical_scrollbox_row[];

const lui_node_array vertical_scrollbox_row_array = {
    .count = 2,
    .nodes = vertical_scrollbox_row
};

const lui_row_data vertical_scrollbox_row_data = {
    .horizontal_align = 0,
    .vertical_align   = 0.5,
    .spacing          = (lui_length){0, 0, 0}
};

const lui_box_data vertical_scrollbox_handle_data = {
    .tint = LUI_HEX("#00FF00")
};

// Linear Main Body
const lui_node luipf_vertical_scrollbox[] = {
    {   // measure parent space - the clipbox dimensions - the viewport size
        .type  = lui_node_render_size_query | lui_node_flag_data_instanced,
        .child = &luipf_vertical_scrollbox[1],
        .data_instance_offset = offsetof(luipf_vertical_scrollbox_data, state_render_size)
    },
    {   // clip contents not viewed
        .type  = lui_node_clipbox | lui_node_flag_ignore_min,
        .child = &luipf_vertical_scrollbox[2],
        .data  = NULL
    },
    {   // set scrollbox callback
        .type  = lui_node_input_handle,
        .child = &luipf_vertical_scrollbox[3],
        .data  = vertical_scrollbox_scroll_func,
    },
    {   // create scroll box input field
        .type  = lui_node_input_box | lui_node_flag_data_instanced,
        .child = &luipf_vertical_scrollbox[4],
        .data_instance_offset = 0 // the instance itself
    },
    {   // measure child - the content size
        .type  = lui_node_measure_size_query | lui_node_flag_data_instanced,
        .child = &luipf_vertical_scrollbox[5],
        .data_instance_offset = offsetof(luipf_vertical_scrollbox_data, state_measure_size)
    },
    {   // row - content, handle
        .type        = lui_node_row,
        .child_array = &vertical_scrollbox_row_array,
        .data        = &vertical_scrollbox_row_data
    },
};

// Handle
static lui_node vertical_scrollbox_handle[] = {
    {
        .type  = lui_node_input_handle,
        .child = &vertical_scrollbox_handle[1],
        .data  = vertical_scrollbox_handle_func
    },
    {
        .type = lui_node_input_box | lui_node_flag_data_instanced,
        .child = &vertical_scrollbox_handle[2],
        .data  = 0, // instance itself
    },
    {   // span handle
        .type  = lui_node_sizebox | lui_node_flag_child_instanced | lui_node_flag_data_instanced,
        .child_instance_offset = offsetof(luipf_vertical_scrollbox_data, handle_child),
        .data_instance_offset  = offsetof(luipf_vertical_scrollbox_data, state_handle_sizebox)
    }
};

// Row Contents
static lui_node vertical_scrollbox_row[] = {
    {
        .type = lui_node_offset | lui_node_flag_child_instanced | lui_node_flag_data_instanced,
        .child_instance_offset  = offsetof(luipf_vertical_scrollbox_data, scrolled_child),
        .data_instance_offset   = offsetof(luipf_vertical_scrollbox_data, state_child_offset)
    },
    {
        .type  = lui_node_offset | lui_node_flag_data_instanced,
        .child = vertical_scrollbox_handle,
        .data_instance_offset   = offsetof(luipf_vertical_scrollbox_data, state_handle_offset)
    },
};

#endif // LIGHT_USER_INTERFACE_PREFABS_IMPL
