#version 450

layout (location=0) in vec4 position;
layout (location=1) in vec4 normal;

layout (location=0) out vec3 position_fs;
layout (location=1) out vec3 normal_fs;

struct Viewing {
	mat4 modelview;
	mat4 invModelview;
	mat4 projection;
	mat4 invProjection;
	mat4 modelviewProjection;
	mat4 invModelviewProjection;
	mat3 normal;
	mat3 invNormal;
};

layout (std140, set=1, binding=0) uniform ViewingUniform {
	Viewing mat;
};

void main() {
	vec4 position_eyeSpace = mat.modelview * position;
	position_fs = position_eyeSpace.xyz / position_eyeSpace.w;
	normal_fs = normalize(mat.normal * normal.xyz);
	gl_Position = mat.projection * position_eyeSpace;
}
