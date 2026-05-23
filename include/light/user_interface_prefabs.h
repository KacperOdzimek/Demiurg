#ifndef LIGHT_USER_INTERFACE_PREFABS_H
#define LIGHT_USER_INTERFACE_PREFABS_H

#include "light/user_interface.h"

typedef struct luipf_button_data {
    lui_node*               button_child;
    lui_box_data            default_style;
    lui_box_data            hovered_style;
    lui_box_data            pressed_style;
    lui_input_handler_func  on_clicked;
    lui_input_handler_func  on_released;
    lui_input_handler_func  on_held;
    void*                   data;
    int                     state_held;
    lui_box_data            state_style;
} luipf_button_data;

extern const lui_node luipf_button[];

typedef struct luipf_horizontal_scrollbox_data {
    float           scroll_speed_mod;
    lui_node*       scrolled_child;
    lui_offset_data state_offset;
} luipf_horizontal_scrollbox_data;

extern const lui_node luipf_horizontal_scrollbox[];

typedef struct luipf_vertical_scrollbox_data {
    float           scroll_speed_mod;
    lui_node*       scrolled_child;
    lui_offset_data state_offset;
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
    int left_pressed = 0; lui_injection_query_cursor_state(input_state, &left_pressed, NULL, NULL);

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

static void vertical_scrollbox_input_func(
    void*                       input_box_data,
    lui_injection_input_state*  input_state,
    int                         cursor_inside,
    float                       delta_time
) {
    if (!cursor_inside) return;

    float scroll_dir; lui_injection_query_cursor_state(input_state, NULL, NULL, &scroll_dir);
    luipf_vertical_scrollbox_data* data = (luipf_vertical_scrollbox_data*)input_box_data;
    data->state_offset.offset_y += -1.0f * scroll_dir * delta_time * data->scroll_speed_mod * default_scroll_speed_vertical;
}

const lui_node luipf_vertical_scrollbox[] = {
    {
        .type  = lui_node_clipbox,
        .child = &luipf_vertical_scrollbox[1],
        .data  = NULL
    },
    {
        .type  = lui_node_input_handle,
        .child = &luipf_vertical_scrollbox[2],
        .data  = vertical_scrollbox_input_func,
    },
    {
        .type  = lui_node_input_box | lui_node_flag_data_instanced,
        .child = &luipf_vertical_scrollbox[3],
        .data_instance_offset = 0 // the instance itself
    },
    {
        .type = lui_node_offset | lui_node_flag_child_instanced | lui_node_flag_data_instanced,
        .child_instance_offset  = offsetof(luipf_vertical_scrollbox_data, scrolled_child),
        .data_instance_offset   = offsetof(luipf_vertical_scrollbox_data, state_offset)
    }
};

#endif // LIGHT_USER_INTERFACE_PREFABS_IMPL
