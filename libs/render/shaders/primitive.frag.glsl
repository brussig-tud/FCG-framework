#version 450

layout(location=0) in vec4 color;
layout(location=1) in vec2 uv;
layout(location=2) in vec3 eyeNormal;
layout(location=0) out vec4 outputColor;

layout(set=3, binding=0, std140) uniform Lighting {
	vec4 direction; // normalized direction toward the light; W enables lighting
	vec4 ambient; // W enables two-sided quad normals
	vec4 diffuse;
} lighting;

void main() {
	vec4 base = color;
	if (lighting.direction.w != 0.0) {
		vec3 n = normalize(eyeNormal);
		if (lighting.ambient.w != 0.0 && !gl_FrontFacing) n = -n;
		base.rgb *= lighting.ambient.rgb + lighting.diffuse.rgb * max(dot(n, lighting.direction.xyz), 0.0);
	}
	outputColor = base;
}
