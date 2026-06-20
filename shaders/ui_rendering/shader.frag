#version 430

layout(location = 0) in      vec2  in_pos;
layout(location = 1) in      vec2  in_uv;
layout(location = 2) flat in int   in_clipbox_index;
layout(location = 3) flat in int   in_texture_index;
layout(location = 4) flat in vec4  in_color;

layout(location = 0) out vec4 outColor;

struct lla_mat2x3 {
    float m00, m01;
    float m10, m11;
    float tx,  ty;
};

layout(std430, set = 0, binding = 4) readonly buffer Clips {
    lla_mat2x3 clips[];
};

layout(set = 0, binding = 5) uniform sampler Sampler;
layout(set = 0, binding = 6) uniform texture2D Textures[1024];

bool point_in_clip(lla_mat2x3 t, vec2 p) {
    float det = t.m00 * t.m11 - t.m01 * t.m10;
    if (abs(det) < 0.00001) return true;

    // inverse matrix
    float inv00 =  t.m11 / det;
    float inv01 = -t.m01 / det;
    float inv10 = -t.m10 / det;
    float inv11 =  t.m00 / det;

    vec2 d = p - vec2(t.tx, t.ty);

    vec2 local = vec2(
        inv00 * d.x + inv01 * d.y,
        inv10 * d.x + inv11 * d.y
    );

    return (
        local.x >= -1.0 &&
        local.x <=  1.0 &&
        local.y >= -1.0 &&
        local.y <=  1.0
    );
}

vec3 srgb_to_linear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

void main() {
    if (in_clipbox_index >= 0 && (!point_in_clip(clips[in_clipbox_index], in_pos))) discard; // Clipping
    vec4 tint = vec4(srgb_to_linear(in_color.rgb), in_color.a);                              // Tint

    // Texture selection
    bool has_texture = in_texture_index != 0;
    bool is_font     = in_texture_index < 0;

    int  tex_index     = has_texture ? (is_font ? -(in_texture_index + 1) : in_texture_index - 1) : 0;
    vec4 texture_color = has_texture ? texture(sampler2D(Textures[tex_index], Sampler), in_uv) : vec4(1.0);

    // Font alpha handling
    if (is_font) {
        float dist = texture_color.r;

        // Controls
        float edge = 0.5;           // where the edge is in the SDF
        float width = fwidth(dist); // adaptive smoothing

        float alpha = smoothstep(edge - width, edge + width, dist);
        texture_color = vec4(1.0, 1.0, 1.0, alpha);
    }

    // Final color
    outColor = vec4(texture_color.rgb * tint.rgb, texture_color.a * tint.a);
}
