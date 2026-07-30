#version 430
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) out vec2      out_pos;
layout(location = 1) out vec2      out_uv;
layout(location = 2) flat out int  out_clipbox_index;
layout(location = 3) flat out int  out_texture_index;
layout(location = 4) flat out vec4 out_color;

struct uv_2d {
    float min_x, min_y;
    float max_x, max_y;
};

struct gpu_instance {
    int     item;
    int     glyph;
};

struct gpu_draw_item {
    mat3    transform;
    uv_2d   atlas_position;
    int     texture_index;
    int     clipbox_index;
    uint    shader_index;
    float   r, g, b, a;
};

struct gpu_clipbox {
    mat3 transform;
};

struct gpu_glyph {
    uv_2d atlas_position;
    float off_x, off_y;
    float size_x, size_y;
};

layout(push_constant) uniform PushConstants {
    uint resolution_width;
    uint resolution_height;
    uint instances_buffer_index;
    uint draw_items_buffer_index;
    uint glyphs_buffer_index;
} pc;

layout(set = 0, binding = 1) readonly buffer InstancesBuffer {
    gpu_instance instances[];
} instances_buffers[];

layout(set = 0, binding = 1) readonly buffer DrawItemsBuffer {
    gpu_draw_item draw_items[];
} draw_items_buffers[];

layout(set = 0, binding = 1) readonly buffer GlyphsBuffer {
    gpu_glyph glyphs[];
} glyphs_buffers[];

const vec2 vert_pos[4] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
);

const vec2 vert_uv[4] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
);

void main() {
    gpu_instance  inst = instances_buffers[nonuniformEXT(pc.instances_buffer_index)].instances[gl_InstanceIndex];
    gpu_draw_item item = draw_items_buffers[nonuniformEXT(pc.draw_items_buffer_index)].draw_items[inst.item];

    mat3 transform = item.transform;

    vec2 uv = mix(
        vec2(item.atlas_position.min_x, item.atlas_position.min_y),
        vec2(item.atlas_position.max_x, item.atlas_position.max_y),
        vert_uv[gl_VertexIndex]
    );

    if (inst.glyph != -1) {
        vec2 text_box_size = vec2(
            pc.resolution_width  * length(transform[0].xy),
            pc.resolution_height * length(transform[1].xy)
        );

        gpu_glyph glyph = glyphs_buffers[nonuniformEXT(pc.glyphs_buffer_index)].glyphs[inst.glyph];

        uv = mix(
            vec2(glyph.atlas_position.min_x, glyph.atlas_position.min_y),
            vec2(glyph.atlas_position.max_x, glyph.atlas_position.max_y),
            uv
        );

        float pixel_to_norm_x = 2.0 / text_box_size.x;
        float pixel_to_norm_y = 2.0 / text_box_size.y;

        float sx = glyph.size_x * pixel_to_norm_x / 2.0;
        float sy = glyph.size_y * pixel_to_norm_y / 2.0;

        float tx = -1.0 + (2.0 * glyph.off_x + glyph.size_x) / text_box_size.x;
        float ty = -1.0 + (2.0 * glyph.off_y - glyph.size_y) / text_box_size.y;

        mat3 local_transform = mat3(
            sx, 0.0, tx,
            0.0, sy, ty,
            0.0, 0.0, 1.0
        );

        transform = transform * local_transform;
    }

    vec2 pos = (transform * vec3(vert_pos[gl_VertexIndex], 1.0)).xy;
    gl_Position = vec4(pos.x, -pos.y, 0.0, 1.0);

    out_pos             = pos;
    out_uv              = uv;
    out_clipbox_index   = item.clipbox_index;
    out_texture_index   = item.texture_index;
    out_color           = vec4(item.r, item.g, item.b, item.a);
}
