#version 450

layout(location=0) in vec3 localPosition;
layout(location=1) in vec3 localNormal;
layout(location=2) in vec2 localUV;

layout(set=0, binding=0, std430) readonly buffer Positions { uint positions[]; };
layout(set=0, binding=1, std430) readonly buffer Extents { uint extents[]; };
layout(set=0, binding=2, std430) readonly buffer Orientations { uint orientations[]; };
layout(set=0, binding=3, std430) readonly buffer Colors { uint colors[]; };

layout(set=1, binding=0, std140) uniform Viewing {
	mat4 modelview;
	mat4 invModelview;
	mat4 projection;
	mat4 invProjection;
	mat4 modelviewProjection;
	mat4 invModelviewProjection;
	mat3 normal;
	mat3 invNormal;
} viewing;

layout(set=1, binding=1, std140) uniform Attributes {
	uvec4 layouts[4]; // word offset, word stride, buffer enabled, reserved
	uvec4 components[4]; // trait component offsets in words, in XYZW order
	vec4 constants[4];
	uvec4 drawInfo; // first instance, reserved
} attributes;

layout(location=0) out vec4 color;
layout(location=1) out vec2 uv;
layout(location=2) out vec3 eyeNormal;

float fetch(uint slot, uint address) {
	if (slot == 0) return uintBitsToFloat(positions[address]);
	if (slot == 1) return uintBitsToFloat(extents[address]);
	if (slot == 2) return uintBitsToFloat(orientations[address]);
	return uintBitsToFloat(colors[address]);
}

vec4 fetchAttribute(uint slot, uint instance, uint count) {
	vec4 value = attributes.constants[slot];
	if (attributes.layouts[slot].z != 0) {
		uint start = attributes.layouts[slot].x + instance * attributes.layouts[slot].y;
		for (uint i = 0; i < count; ++i)
			value[i] = fetch(slot, start + attributes.components[slot][i]);
	}
	return value;
}

vec3 rotate(vec4 q, vec3 p) {
	return p + 2.0 * cross(q.xyz, cross(q.xyz, p) + q.w * p);
}

void main() {
	uint instance = uint(gl_InstanceIndex) + attributes.drawInfo.x;
	vec4 position = fetchAttribute(0, instance, 4);
	vec3 extent = fetchAttribute(1, instance, 3).xyz;
	vec4 orientation = fetchAttribute(2, instance, 4);
	// Scaling before normalization keeps finite tiny and large quaternions representable.
	orientation /= max(max(abs(orientation.x), abs(orientation.y)), max(abs(orientation.z), abs(orientation.w)));
	orientation = normalize(orientation);
	vec3 point = position.xyz / position.w + rotate(orientation, localPosition * extent);
	gl_Position = viewing.modelviewProjection * vec4(point, 1.0);
	color = fetchAttribute(3, instance, 4);
	uv = localUV;
	eyeNormal = viewing.normal * rotate(orientation, localNormal);
}
