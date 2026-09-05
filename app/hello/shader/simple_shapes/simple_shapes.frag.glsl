#version 450

layout (location = 0) in vec3 normal;
layout (location = 0) out vec4 color;

void main() {
	float lighting = max(
		dot(normalize(normal), normalize(vec3(0.4, 0.5, 1.0))),
		0.0
	);
	color = vec4(vec3(0.15, 0.55, 0.9) * (0.2 + 0.8 * lighting), 1.0);
}
