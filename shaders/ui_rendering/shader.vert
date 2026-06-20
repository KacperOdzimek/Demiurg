#version 430

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec2       out_pos;
layout(location = 1) out vec2       out_uv;
layout(location = 2) flat out int   out_instance;

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
    int                 shader;
};

layout(std430, set = 0, binding = 0) readonly buffer Instances {
    gpu_instance instances[];
};

void main() {
    gpu_instance inst = instances[gl_InstanceIndex];

    vec2 pos;
    pos.x = inst.transform.m00 * in_pos.x + inst.transform.m01 * in_pos.y + inst.transform.tx;
    pos.y = inst.transform.m10 * in_pos.x + inst.transform.m11 * in_pos.y + inst.transform.ty;

    gl_Position = vec4(pos.x, -pos.y, 0, 1.0);

    out_pos      = pos;
    out_uv       = in_uv;
    out_instance = gl_InstanceIndex;
}
