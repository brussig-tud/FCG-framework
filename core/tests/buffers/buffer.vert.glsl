#version 450
layout(location=0) in vec2 position;
layout(location=1) in vec2 instanceOffset;
layout(std430, set=0, binding=0) readonly buffer VertexStorage { vec4 value; } storageData;
layout(std140, set=1, binding=0) uniform Transform { vec4 value; } transformData;
void main() {
    gl_Position = vec4(position + instanceOffset + storageData.value.xy + transformData.value.xy, 0.5, 1.0);
}
