#version 430

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;

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
    float       r, g, b, a;
    int         shader;
};

struct gpu_clipbox {
    lla_mat2x3 clip;
};

struct gpu_glyph {
    lgx_uv_2d   atlas_position;
    float       off_x,  off_y;
    float       size_x, size_y;
};

layout(set = 0, binding = 0) uniform RenderParameters {
    int         resolution_x;
    int         resolution_y;
} parameters_ubo;

layout(std430, set = 0, binding = 1) readonly buffer Instances {
    gpu_instance instances[];
};

layout(std430, set = 0, binding = 2) readonly buffer DrawItems {
    gpu_draw_item items[];
};

layout(std430, set = 0, binding = 3) readonly buffer Glyphs {
    gpu_glyph glyphs[];
};

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

void main() {
    gpu_instance  inst = instances[gl_InstanceIndex];
    gpu_draw_item item = items[inst.item];

    lla_mat2x3 transform = item.transform;

    // nest uv to item atlas position
    vec2 uv = mix(
        vec2(item.atlas_position.min_x, item.atlas_position.min_y),
        vec2(item.atlas_position.max_x, item.atlas_position.max_y),
        in_uv
    );

    // text glyph transformations
    if (inst.glyph != -1) {
        // calculate entire text box pixel size
        vec2 text_box_size = vec2(
            parameters_ubo.resolution_x * (abs(transform.m00) + abs(transform.m10)),
            parameters_ubo.resolution_y * (abs(transform.m01) + abs(transform.m11))
        );

        // get this glyph
        gpu_glyph glyph = glyphs[inst.glyph];

        // deeper nest uv to glyph atlas position
        uv = mix(
            vec2(glyph.atlas_position.min_x, glyph.atlas_position.min_y),
            vec2(glyph.atlas_position.max_x, glyph.atlas_position.max_y),
            uv
        );

        // pixel to norm coords ratio
        float pixel_to_norm_x = 2.0f / text_box_size.x;
        float pixel_to_norm_y = 2.0f / text_box_size.y;

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
    vec2 pos = vec2(
        transform.m00 * in_pos.x + transform.m01 * in_pos.y + transform.tx,
        transform.m10 * in_pos.x + transform.m11 * in_pos.y + transform.ty
    );

    gl_Position = vec4(pos.x, -pos.y, 0, 1.0);

    out_pos             = pos;
    out_uv              = uv;
    out_clipbox_index   = item.clipbox_index;
    out_texture_index   = item.texture_index;
    out_color           = vec4(item.r, item.g, item.b, item.a);
}
