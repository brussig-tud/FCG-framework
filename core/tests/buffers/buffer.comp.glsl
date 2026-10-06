#version 450
layout(local_size_x=1, local_size_y=1, local_size_z=1) in;
layout(std430, set=0, binding=0) readonly buffer Input { uint values[]; } source;
layout(std430, set=1, binding=0) buffer Output { uint values[]; } destination;
layout(std140, set=2, binding=0) uniform Parameters { uvec4 params; };
void main() {
    uint i = gl_GlobalInvocationID.x;
    destination.values[i] += source.values[i] * params.x + params.y;
}
