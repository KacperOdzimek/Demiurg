#version 430

layout(location = 0) in  vec2       in_pos;

layout(location = 0) out vec2       out_pos;
layout(location = 1) flat out int   out_instance;

void main() {
    gl_Position  = vec4(in_pos, 0.0, 1.0);
    out_pos      = in_pos;
    out_instance = gl_VertexIndex / 3;
}
