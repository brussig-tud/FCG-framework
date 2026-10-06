#version 450
layout(location=0) out vec4 color;
layout(std430, set=2, binding=0) readonly buffer FragmentStorage { vec4 value; } storageData;
layout(std140, set=3, binding=0) uniform Material { vec4 value; } material;
void main() { color = storageData.value * material.value; }
