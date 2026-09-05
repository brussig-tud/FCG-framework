#version 450

layout (location=0) in vec4 position;
layout (location=1) in vec4 normal;

layout (location=0) out vec3 normal_fs;

layout (std140, set=1, binding=0) uniform Viewing {
	mat4 modelview;
	mat4 invModelview;
	mat4 projection;
	mat4 invProjection;
	mat4 modelviewProjection;
	mat4 invModelviewProjection;
	mat3 normalMat;
	mat3 invNormalMat;
};

void main() {
	gl_Position = modelviewProjection * position;
	normal_fs = normalize(normalMat * normal.xyz);
}
