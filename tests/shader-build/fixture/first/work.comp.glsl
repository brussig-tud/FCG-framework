#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout (local_size_x = 1) in;
layout (set = 0, binding = 0) buffer Output { uint value; } outputData;

void main () {
	outputData.value = VALUE;
}
