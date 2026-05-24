/*
----------------------------------------------------------------
Contents
This file provided commonly used prefab components for light user interface.

----------------------------------------------------------------
Code info:
- luipf prefix
- LIGHT_USER_INTERFACE_PREFABS_IMPL macro to build
- user_interface.h dependent

----------------------------------------------------------------
Usage:
- provided prefabs shall be instanced with lui_node_instance
- use on of provided lui_node arrays as root and a matching data structure as data
- enjoy

----------------------------------------------------------------
Possible refactor:
- horizontal/vertical scrollbox code may be merged and reduced
*/

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
    void*                       event_data;

    lui_box_data                state_style;
    char                        state_held;
} luipf_button_data;

extern const lui_node luipf_button[];

typedef struct luipf_scrollbox_data {
    float                       scroll_speed_mod;
    lui_node*                   scroll_child;
    lui_length                  handle_thickness;
    lui_node*                   handle_child;
    
    int                         state_offset;
    int                         state_handle_dragged;
    lui_offset_data             state_child_offset;
    lui_offset_data             state_handle_offset;
    lui_sizebox_data            state_handle_sizebox;
    lui_measure_size_query_data state_measure_size;
    lui_render_size_query_data  state_render_size;
} luipf_scrollbox_data;

extern const lui_node luipf_horizontal_scrollbox[];
extern const lui_node luipf_vertical_scrollbox[];

#endif // LIGHT_USER_INTERFACE_PREFABS_H

#ifdef LIGHT_USER_INTERFACE_PREFABS_IMPL

/*
    Button
*/

static void button_input_func(
    void*                       input_box_data,
    lui_injection_input_state*  input_state,
    int                         cursor_inside,
    float                       delta_time
) {
    luipf_button_data* data = (luipf_button_data*)input_box_data;
    int c_left_pressed; lui_injection_query_cursor_state(input_state, &c_left_pressed, NULL, NULL);
    int p_left_pressed; lui_injection_query_previous_cursor_state(input_state, &p_left_pressed, NULL, NULL);

    char just_pressed  = c_left_pressed && !p_left_pressed;
    char just_released = !c_left_pressed && p_left_pressed;

    if (just_pressed && cursor_inside) {            // press started
        data->state_held = 1;
        data->state_style = data->pressed_style;
        if (data->on_clicked) data->on_clicked(data->event_data, input_state, cursor_inside, delta_time);
    }
    else if (c_left_pressed && data->state_held) {    // held
        data->state_style = data->pressed_style;
        if (data->on_held) data->on_held(data->event_data, input_state, cursor_inside, delta_time);
    }
    else if (just_released && data->state_held) {   // released
        data->state_held = 0;
        if (cursor_inside)      data->state_style = data->hovered_style;
        else                    data->state_style = data->default_style;
        if (data->on_released)  data->on_released(data->event_data, input_state, cursor_inside, delta_time);
    }
    else if (cursor_inside) data->state_style = data->hovered_style;    // hover
    else                    data->state_style = data->default_style;    // idle
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

/*
    Horizontal Scrollbox
*/

static const float default_scroll_speed_horizontal = 3500;

static inline void horizontal_scrollbox_apply_content_scroll(luipf_scrollbox_data* data) {
    // sizes
    int content_width   = data->state_measure_size.width.min;
    int viewport_width  = data->state_render_size.width;

    int offset_to_align = content_width / 2;  // start offseting from align - hardcoded left
    int total_offset = offset_to_align + data->state_offset;

    // no scrolling needed
    if (content_width <= viewport_width) {
        total_offset = 0;
        data->state_offset = 0;
    } 
    // clamp
    else {
        int max_offset = (content_width - viewport_width) / 2;
        if (total_offset >  max_offset) {
            total_offset = max_offset;
            data->state_offset = max_offset - offset_to_align;
        }
        if (total_offset < -max_offset) {
            total_offset = -max_offset;
            data->state_offset = -max_offset - offset_to_align;
        }
    }

    data->state_child_offset.offset_x = total_offset;
}

static inline void horizontal_scrollbox_apply_handle_scroll(luipf_scrollbox_data* data) {
    // set height to user desires
    data->state_handle_sizebox.flag |= lui_sizebox_overwrite_all_height;
    data->state_handle_sizebox.height = data->handle_thickness;

    // sizes
    float content_width   = data->state_measure_size.width.min;
    float viewport_width  = data->state_render_size.width;

    // set width to visible part of widget
    data->state_handle_sizebox.flag |= lui_sizebox_overwrite_all_width;

    float visible_fraction = viewport_width / content_width;
    if (visible_fraction > 1.0f) visible_fraction = 1.0f; // clamp

    int width = viewport_width * visible_fraction;
    if (width > data->state_measure_size.width.max) width = data->state_measure_size.width.max;

    data->state_handle_sizebox.width.min = width;
    data->state_handle_sizebox.width.max = width;

    // position handle
    if (visible_fraction >= 1.0f) {
        data->state_handle_offset.offset_x = 0;
    }
    else {
        // find current lerp alpha of content between ends
        float begin = (content_width - viewport_width) / 2;
        float end   = -begin;
        float alpha = (data->state_child_offset.offset_x - begin) / (end - begin);

        // apply alpha to handle movement
        begin = -(viewport_width / 2) + (width / 2);
        end   = -begin;
        data->state_handle_offset.offset_x = begin + (end - begin) * alpha;
    }
}

static void horizontal_scrollbox_scroll_func(
    void*                       input_box_data,
    lui_injection_input_state*  input_state,
    int                         cursor_inside,
    float                       delta_time
) {
    luipf_scrollbox_data* data = (luipf_scrollbox_data*)input_box_data;

    int pixels_change = 0;
    if (cursor_inside) {
        float scroll_dir; lui_injection_query_cursor_state(input_state, NULL, NULL, &scroll_dir);
        pixels_change = scroll_dir * delta_time * data->scroll_speed_mod * default_scroll_speed_horizontal;
    }
    data->state_offset -= pixels_change;

    horizontal_scrollbox_apply_content_scroll(data);
}

static void horizontal_scrollbox_handle_func(
    void*                       input_box_data,
    lui_injection_input_state*  input_state,
    int                         cursor_inside,
    float                       delta_time
) {
    luipf_scrollbox_data* data = (luipf_scrollbox_data*)input_box_data;

    // position handle
    horizontal_scrollbox_apply_handle_scroll(data);

    // sizes
    float content_width   = data->state_measure_size.width.min;
    float viewport_width  = data->state_render_size.width;

    // scroll by draging handle
    int left_pressed; lui_injection_query_cursor_state(input_state, &left_pressed, NULL, NULL);
    if (left_pressed) {
        int cursor_x; lui_injection_query_cursor_position(input_state, &cursor_x, NULL, NULL, NULL);

        if (data->state_handle_dragged != -1) {                         // was dragged
            int pixels_change = cursor_x - data->state_handle_dragged;  // calculate pixel movement within handle
            pixels_change *= (content_width / viewport_width);          // calculate pixel movement within content

            data->state_offset -= pixels_change;
            horizontal_scrollbox_apply_content_scroll(data);

            data->state_handle_dragged = cursor_x;
        }
        else if (cursor_inside) {
            int c_left_pressed; lui_injection_query_cursor_state(input_state, &c_left_pressed, NULL, NULL);
            int p_left_pressed; lui_injection_query_previous_cursor_state(input_state, &p_left_pressed, NULL, NULL);
            if (!(c_left_pressed && !p_left_pressed)) return; // avoid accidental drag, require new click inside handle
            data->state_handle_dragged = cursor_x;
        }
    }
    else data->state_handle_dragged = -1;
}

static lui_node horizontal_scrollbox_column[];

const lui_node_array horizontal_scrollbox_column_array = {
    .count = 2,
    .nodes = horizontal_scrollbox_column
};

const lui_column_data horizontal_scrollbox_column_data = {
    .horizontal_align = 0.5,
    .vertical_align   = 0,
    .spacing          = (lui_length){0, 0, 0}
};

// Linear Main Body
const lui_node luipf_horizontal_scrollbox[] = {
    {   // measure parent space - the clipbox dimensions - the viewport size
        .type  = lui_node_render_size_query | lui_node_flag_data_instanced,
        .child = &luipf_horizontal_scrollbox[1],
        .data_instance_offset = offsetof(luipf_scrollbox_data, state_render_size)
    },
    {   // clip contents not viewed
        .type  = lui_node_clipbox | lui_node_flag_ignore_min_width,
        .child = &luipf_horizontal_scrollbox[2],
        .data  = NULL
    },
    {   // set scrollbox callback
        .type  = lui_node_input_handle,
        .child = &luipf_horizontal_scrollbox[3],
        .data  = horizontal_scrollbox_scroll_func,
    },
    {   // create scroll box input field
        .type  = lui_node_input_box | lui_node_flag_data_instanced,
        .child = &luipf_horizontal_scrollbox[4],
        .data_instance_offset = 0 // the instance itself
    },
    {   // column - content, handle
        .type        = lui_node_column,
        .child_array = &horizontal_scrollbox_column_array,
        .data        = &horizontal_scrollbox_column_data
    },
};

// Handle
static lui_node horizontal_scrollbox_handle[] = {
    {
        .type  = lui_node_input_handle,
        .child = &horizontal_scrollbox_handle[1],
        .data  = horizontal_scrollbox_handle_func
    },
    {
        .type = lui_node_input_box | lui_node_flag_data_instanced,
        .child = &horizontal_scrollbox_handle[2],
        .data  = 0, // instance itself
    },
    {   // span handle
        .type  = lui_node_sizebox | lui_node_flag_child_instanced | lui_node_flag_data_instanced,
        .child_instance_offset = offsetof(luipf_scrollbox_data, handle_child),
        .data_instance_offset  = offsetof(luipf_scrollbox_data, state_handle_sizebox)
    }
};

// Column Contents
static lui_node horizontal_scrollbox_column[] = {
    {   // measure child - the content size
        .type  = lui_node_measure_size_query | lui_node_flag_data_instanced,
        .child = &horizontal_scrollbox_column[2],
        .data_instance_offset = offsetof(luipf_scrollbox_data, state_measure_size)
    },
    {   // render offseted handle
        .type  = lui_node_offset | lui_node_flag_data_instanced,
        .child = horizontal_scrollbox_handle,
        .data_instance_offset   = offsetof(luipf_scrollbox_data, state_handle_offset)
    },
    {   // render offseted content
        .type = lui_node_offset | lui_node_flag_child_instanced | lui_node_flag_data_instanced,
        .child_instance_offset  = offsetof(luipf_scrollbox_data, scroll_child),
        .data_instance_offset   = offsetof(luipf_scrollbox_data, state_child_offset)
    },
};

/*
    Vertical Scrollbox
*/

static const float default_scroll_speed_vertical = 2000;

static inline void vertical_scrollbox_apply_content_scroll(luipf_scrollbox_data* data) {
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

static inline void vertical_scrollbox_apply_handle_scroll(luipf_scrollbox_data* data) {
    // set width to user desires
    data->state_handle_sizebox.flag |= lui_sizebox_overwrite_all_width;
    data->state_handle_sizebox.width = data->handle_thickness;

    // sizes
    float content_height  = data->state_measure_size.height.min;
    float viewport_height = data->state_render_size.height;

    // set height to visible part of widget
    data->state_handle_sizebox.flag |= lui_sizebox_overwrite_all_height;

    float visible_fraction = viewport_height / content_height;
    if (visible_fraction > 1.0f) visible_fraction = 1.0f; // clamp

    int height = viewport_height * visible_fraction;
    if (height > data->state_measure_size.height.max) height = data->state_measure_size.height.max;
    
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

static void vertical_scrollbox_scroll_func(
    void*                       input_box_data,
    lui_injection_input_state*  input_state,
    int                         cursor_inside,
    float                       delta_time
) {
    luipf_scrollbox_data* data = (luipf_scrollbox_data*)input_box_data;

    int pixels_change = 0;
    if (cursor_inside) {
        float scroll_dir; lui_injection_query_cursor_state(input_state, NULL, NULL, &scroll_dir);
        pixels_change = scroll_dir * delta_time * data->scroll_speed_mod * default_scroll_speed_vertical;
    }
    data->state_offset -= pixels_change;

    vertical_scrollbox_apply_content_scroll(data);
}

static void vertical_scrollbox_handle_func(
    void*                       input_box_data,
    lui_injection_input_state*  input_state,
    int                         cursor_inside,
    float                       delta_time
) {
    luipf_scrollbox_data* data = (luipf_scrollbox_data*)input_box_data;

    // position handle
    vertical_scrollbox_apply_handle_scroll(data);

    // sizes
    float content_height  = data->state_measure_size.height.min;
    float viewport_height = data->state_render_size.height;

    // scroll by draging handle
    int left_pressed; lui_injection_query_cursor_state(input_state, &left_pressed, NULL, NULL);
    if (left_pressed) {
        int cursor_y; lui_injection_query_cursor_position(input_state, NULL, &cursor_y, NULL, NULL);

        if (data->state_handle_dragged != -1) {                         // was dragged
            int pixels_change = data->state_handle_dragged - cursor_y;  // calculate pixel movement within handle
            pixels_change *= (content_height / viewport_height);        // calculate pixel movement within content

            data->state_offset -= pixels_change;
            vertical_scrollbox_apply_content_scroll(data);

            data->state_handle_dragged = cursor_y;
        }
        else if (cursor_inside) {
            int c_left_pressed; lui_injection_query_cursor_state(input_state, &c_left_pressed, NULL, NULL);
            int p_left_pressed; lui_injection_query_previous_cursor_state(input_state, &p_left_pressed, NULL, NULL);
            if (!(c_left_pressed && !p_left_pressed)) return; // avoid accidental drag, require new click inside handle
            data->state_handle_dragged = cursor_y;
        }
    }
    else data->state_handle_dragged = -1;
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

// Linear Main Body
const lui_node luipf_vertical_scrollbox[] = {
    {   // measure parent space - the clipbox dimensions - the viewport size
        .type  = lui_node_render_size_query | lui_node_flag_data_instanced,
        .child = &luipf_vertical_scrollbox[1],
        .data_instance_offset = offsetof(luipf_scrollbox_data, state_render_size)
    },
    {   // clip contents not viewed
        .type  = lui_node_clipbox | lui_node_flag_ignore_min_height,
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
        .child_instance_offset = offsetof(luipf_scrollbox_data, handle_child),
        .data_instance_offset  = offsetof(luipf_scrollbox_data, state_handle_sizebox)
    }
};

// Row Contents
static lui_node vertical_scrollbox_row[] = {
    {   // measure child - the content size
        .type  = lui_node_measure_size_query | lui_node_flag_data_instanced,
        .child = &vertical_scrollbox_row[2],
        .data_instance_offset = offsetof(luipf_scrollbox_data, state_measure_size)
    },
    {   // rebder offsetet handle
        .type  = lui_node_offset | lui_node_flag_data_instanced,
        .child = vertical_scrollbox_handle,
        .data_instance_offset   = offsetof(luipf_scrollbox_data, state_handle_offset)
    },
    {   // render offseted content
        .type = lui_node_offset | lui_node_flag_child_instanced | lui_node_flag_data_instanced,
        .child_instance_offset  = offsetof(luipf_scrollbox_data, scroll_child),
        .data_instance_offset   = offsetof(luipf_scrollbox_data, state_child_offset)
    },
};

#endif // LIGHT_USER_INTERFACE_PREFABS_IMPL
