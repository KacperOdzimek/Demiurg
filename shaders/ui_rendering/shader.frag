#version 450

layout(location = 0) in vec2     in_uv;
layout(location = 1) flat in int in_instance;

layout(location = 0) out vec4 outColor;

struct lui_transform {
    float m00, m01, tx;
    float m10, m11, ty;
};

struct atlas_position_uv {
    float uv_min_x, uv_min_y;
    float uv_max_x, uv_max_y;
};

struct gpu_instance {
    lui_transform       transform;
    atlas_position_uv   atlas_position;
    int                 texture_index;
    int                 clip_index;
    float               r, g, b, a;
    float               depth;
};

layout(set = 0, binding = 0) readonly buffer Instances {
    gpu_instance instances[];
};

layout(set = 0, binding = 1) readonly buffer Clips {
    lui_transform clips[];
};

layout(set = 0, binding = 2) uniform sampler Sampler;
layout(set = 0, binding = 3) uniform texture2D Textures[1024];

bool point_in_clip(lui_transform t, vec2 p) {
    // transform point into clip space (inverse affine)
    float det = t.m00 * t.m11 - t.m01 * t.m10;

    if (abs(det) < 0.00001)
        return true; // avoid breaking everything

    float inv00 =  t.m11 / det;
    float inv01 = -t.m01 / det;
    float inv10 = -t.m10 / det;
    float inv11 =  t.m00 / det;

    vec2 local;
    local.x = inv00 * (p.x - t.tx) + inv01 * (p.y - t.ty);
    local.y = inv10 * (p.x - t.tx) + inv11 * (p.y - t.ty);

    // assuming clip rect is 0..1
    return (local.x >= 0.0 && local.y >= 0.0 &&
            local.x <= 1.0 && local.y <= 1.0);
}

vec3 srgb_to_linear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

void main() {
    gpu_instance inst = instances[in_instance];

    // Clipping
    if (inst.clip_index >= 0) {
        if (!point_in_clip(clips[inst.clip_index], gl_FragCoord.xy))
            discard;
    }

    // Tint
    vec4 tint = vec4(
        srgb_to_linear(vec3(inst.r, inst.g, inst.b)),
        inst.a
    );

    // Texture selection
    bool has_texture = inst.texture_index != 0;
    bool is_font     = inst.texture_index < 0;

    int tex_index = has_texture
        ? (is_font ? -(inst.texture_index + 1) : inst.texture_index - 1)
        : 0;

    vec2 uv = mix(
        vec2(inst.atlas_position.uv_min_x, inst.atlas_position.uv_min_y),
        vec2(inst.atlas_position.uv_max_x, inst.atlas_position.uv_max_y),
        in_uv
    );

    vec4 texture_color = has_texture
        ? texture(sampler2D(Textures[tex_index], Sampler), uv)
        : vec4(1.0);

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
    outColor = vec4(texture_color.rgb * tint.rgb,
                    texture_color.a * tint.a);
}
