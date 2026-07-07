#version 430
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) out      vec2  out_pos;
layout(location = 1) out      vec2  out_uv;
layout(location = 2) flat out int   out_clipbox_index;
layout(location = 3) flat out int   out_texture_index;
layout(location = 4) flat out vec4  out_color;

struct lla_mat2x3 {
    float m00, m01;
    float m10, m11;
    float tx,  ty;
};

struct lgx_uv_2d {
    float min_x, min_y;
    float max_x, max_y;
};

struct gpu_instance {
    int item;
    int glyph;
};

struct gpu_draw_item {
    lla_mat2x3  transform;
    lgx_uv_2d   atlas_position;
    int         texture_index;
    int         clipbox_index;
    uint        shader_index;
    float       r, g, b, a;
};

struct gpu_clipbox {
    lla_mat2x3 clip;
};

struct gpu_glyph {
    lgx_uv_2d   atlas_position;
    float       off_x,  off_y;
    float       size_x, size_y;
};

layout(push_constant) uniform PushConstants {
    uint    resolution_width;
    uint    resolution_height;
    uint    instances_buffer_index;
    uint    draw_items_buffer_index;
    uint    glyphs_buffer_index;
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

lla_mat2x3 mat2x3_scale(lla_mat2x3 m, float sx, float sy) {
    m.m00 *= sx;  m.m01 *= sy;
    m.m10 *= sx;  m.m11 *= sy;
    return m;
}

lla_mat2x3 mat2x3_offset(lla_mat2x3 m, float ox, float oy) {
    m.tx += ox; m.ty += oy;
    return m;
}

lla_mat2x3 mat2x3_mul(lla_mat2x3 p, lla_mat2x3 c) {
    lla_mat2x3 r;

    r.m00 = p.m00 * c.m00 + p.m01 * c.m10;
    r.m01 = p.m00 * c.m01 + p.m01 * c.m11;
    r.tx  = p.m00 * c.tx  + p.m01 * c.ty + p.tx;

    r.m10 = p.m10 * c.m00 + p.m11 * c.m10;
    r.m11 = p.m10 * c.m01 + p.m11 * c.m11;
    r.ty  = p.m10 * c.tx  + p.m11 * c.ty + p.ty;

    return r;
}

vec2 vert_pos[4] = {
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
};

vec2 vert_uv[4] = {
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
};

void main() {
    gpu_instance  inst = instances_buffers [nonuniformEXT(pc.instances_buffer_index)] .instances [gl_InstanceIndex];
    gpu_draw_item item = draw_items_buffers[nonuniformEXT(pc.draw_items_buffer_index)].draw_items[inst.item];

    lla_mat2x3 transform = item.transform;

    // nest uv to item atlas position
    vec2 uv = mix(
        vec2(item.atlas_position.min_x, item.atlas_position.min_y),
        vec2(item.atlas_position.max_x, item.atlas_position.max_y),
        vert_uv[gl_VertexIndex]
    );

    // text glyph transformations
    if (inst.glyph != -1) {
        // calculate entire text box pixel size
        vec2 text_box_size = vec2(
            pc.resolution_width  * (abs(transform.m00) + abs(transform.m10)),
            pc.resolution_height * (abs(transform.m01) + abs(transform.m11))
        );

        // get this glyph
        gpu_glyph glyph = glyphs_buffers[nonuniformEXT(pc.glyphs_buffer_index)].glyphs[inst.glyph];

        // deeper nest uv to glyph atlas position
        uv = mix(
            vec2(glyph.atlas_position.min_x, glyph.atlas_position.min_y),
            vec2(glyph.atlas_position.max_x, glyph.atlas_position.max_y),
            uv
        );

        // pixel to norm coords ratio
        float pixel_to_norm_x = 2.0 / text_box_size.x;
        float pixel_to_norm_y = 2.0 / text_box_size.y;

        // create local transform of glyph
        lla_mat2x3 local_transform = lla_mat2x3(1, 0, 0, 1, 0, 0);

        // scale : full screen to font pixel size
        local_transform = mat2x3_scale(
            local_transform,
            glyph.size_x * pixel_to_norm_x / 2,
            glyph.size_y * pixel_to_norm_y / 2
        );

        // offset : offset within screen
        local_transform = mat2x3_offset(
            local_transform,
            -1.0 + (2.0 * glyph.off_x + glyph.size_x) / text_box_size.x,
            -1.0 + (2.0 * glyph.off_y - glyph.size_y) / text_box_size.y
        );

        // combine both matrices
        transform = mat2x3_mul(transform, local_transform);
    }

    // calculate vertex final position
    vec2 pos = vert_pos[gl_VertexIndex];
    pos = vec2(
        transform.m00 * pos.x + transform.m01 * pos.y + transform.tx,
        transform.m10 * pos.x + transform.m11 * pos.y + transform.ty
    );

    gl_Position = vec4(pos.x, -pos.y, 0, 1.0);

    out_pos             = pos;
    out_uv              = uv;
    out_clipbox_index   = item.clipbox_index;
    out_texture_index   = item.texture_index;
    out_color           = vec4(item.r, item.g, item.b, item.a);
}
