#version 450

layout(location = 0) in  vec2       in_pos;
layout(location = 1) flat in int    in_instance;

layout(location = 0) out vec4       out_color;

struct gpu_instance {
    float r, g, b, a;
    float center_x, center_y;
    float radius;
};

layout(set = 0, binding = 0) readonly buffer Instances {
    gpu_instance instances[];
};

void main() {
    gpu_instance inst = instances[in_instance];

    // radius constrain
    if (inst.radius > 0.0 && distance(in_pos, vec2(inst.center_x, inst.center_y)) > inst.radius) discard;

    // color
    out_color = vec4(inst.r, inst.g, inst.b, inst.a);
}
