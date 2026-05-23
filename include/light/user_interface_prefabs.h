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
} luipf_button_data;

extern const lui_node luipf_button[];

typedef struct luipf_horizontal_scrollbox_data {
    lui_node*           scrolled_child;
    lui_transform_data  scrolled_transform;
} luipf_horizontal_scrollbox_data;

extern const lui_node luipf_horizontal_scrollbox[];

typedef struct luipf_vertical_scrollbox_data {
    float               scroll_speed;           // speed modificator, by default 2000 pixels per second at scroll_speed = 1.0f
    lui_node*           scrolled_child;
    lui_transform_data  scrolled_transform;
} luipf_vertical_scrollbox_data;

extern const lui_node luipf_vertical_scrollbox[];

#endif // LIGHT_USER_INTERFACE_PREFABS_H

#ifdef LIGHT_USER_INTERFACE_PREFABS_IMPL

// Button



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
    data->scrolled_transform.pixel_offset_y += -1.0f * scroll_dir * delta_time * data->scroll_speed * default_scroll_speed_vertical;
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
        .type = lui_node_transform | lui_node_flag_child_instanced | lui_node_flag_data_instanced,
        .child_instance_offset  = offsetof(luipf_vertical_scrollbox_data, scrolled_child),
        .data_instance_offset   = offsetof(luipf_vertical_scrollbox_data, scrolled_transform)
    }
};

#endif // LIGHT_USER_INTERFACE_PREFABS_IMPL
