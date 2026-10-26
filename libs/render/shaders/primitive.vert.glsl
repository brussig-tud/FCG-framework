#version 450


//////
//
// Uniforms
//

/// Homogeneous primitive positions stored as raw 32-bit words.
layout(set=0, binding=0, std430) readonly buffer Positions {
	/// Position components interpreted as floating-point values by the attribute layout.
	uint positions[];
};

/// Primitive half-extents stored as raw 32-bit words.
layout(set=0, binding=1, std430) readonly buffer Extents {
	/// Extent components interpreted as floating-point values by the attribute layout.
	uint extents[];
};

/// Primitive orientation quaternions stored as raw 32-bit words.
layout(set=0, binding=2, std430) readonly buffer Orientations {
	/// Quaternion components interpreted as floating-point values in XYZW order.
	uint orientations[];
};

/// Primitive RGBA colors stored as raw 32-bit words.
layout(set=0, binding=3, std430) readonly buffer Colors {
	/// Color components interpreted as floating-point values in RGBA order.
	uint colors[];
};

/// Viewing transformations; matches the C++ ViewingUniforms layout.
layout(set=1, binding=0, std140) uniform Viewing {
	/// The object-to-eye transformation.
	mat4 modelview;

	/// The eye-to-object transformation.
	mat4 invModelview;

	/// The eye-to-clip transformation with zero-to-one normalized depth.
	mat4 projection;

	/// The clip-to-eye transformation.
	mat4 invProjection;

	/// The combined object-to-clip transformation.
	mat4 modelviewProjection;

	/// The combined clip-to-object transformation.
	mat4 invModelviewProjection;

	/// The inverse-transpose object-to-eye transformation for normals.
	mat3 normal;

	/// The inverse of the object-to-eye normal transformation.
	mat3 invNormal;
} viewing;

/// Attribute addressing and constant values; matches the C++ AttributeUniforms layout.
layout(set=1, binding=1, std140) uniform Attributes {
	/// Per-slot word offset, word stride, buffer-enabled flag, and reserved component.
	uvec4 layouts[4];

	/// Per-slot component offsets in words, with slots ordered as position, extent, orientation, and color.
	uvec4 components[4];

	/// Per-slot constant values used when the corresponding buffer is disabled.
	vec4 constants[4];

	/// The first attribute instance in X, with the remaining components reserved.
	uvec4 drawInfo;
} attributes;



//////
//
// Streams
//

////
// Inputs

/// The vertex position on a unit primitive spanning minus one to one along each used axis.
layout(location=0) in vec3 localPosition;

/// The outward normal in the unit primitive's local coordinates.
layout(location=1) in vec3 localNormal;

/// The vertex texture coordinates within the primitive face.
layout(location=2) in vec2 localUV;


////
// Outputs

/// The primitive's RGBA color.
layout(location=0) out vec4 color;

/// The texture coordinates within the primitive face.
layout(location=1) out vec2 uv;

/// The outward normal in eye coordinates.
layout(location=2) out vec3 eyeNormal;



//////
//
// Functions
//

/// Reads a floating-point component at a word address in the selected attribute buffer.
float fetch (in uint slot, in uint address)
{
	// Select the position, extent, orientation, or color buffer
	if (slot == 0)
		return uintBitsToFloat(positions[address]);
	if (slot == 1)
		return uintBitsToFloat(extents[address]);
	if (slot == 2)
		return uintBitsToFloat(orientations[address]);
	return uintBitsToFloat(colors[address]);
}

/// Reads count components for an attribute instance, retaining constant defaults for other components.
vec4 fetchAttribute (in uint slot, in uint instance, in uint count)
{
	// Start with the constant value and replace enabled buffer components
	vec4 value = attributes.constants[slot];
	if (attributes.layouts[slot].z != 0) {
		uint start = attributes.layouts[slot].x + instance * attributes.layouts[slot].y;
		for (uint i = 0; i < count; ++i)
			value[i] = fetch(slot, start + attributes.components[slot][i]);
	}
	return value;
}

/// Rotates a local vector by a normalized quaternion in XYZW order.
vec3 rotate (in vec4 q, in vec3 p) {
	return p + 2.0 * cross(q.xyz, cross(q.xyz, p) + q.w * p);
}

/// The shader entry point.
void main ()
{
	// Read the instance attributes
	uint instance = uint(gl_InstanceIndex) + attributes.drawInfo.x;
	vec4 position = fetchAttribute(0, instance, 4);
	vec3 extent = fetchAttribute(1, instance, 3).xyz;
	vec4 orientation = fetchAttribute(2, instance, 4);

	// Scale before normalization to keep finite tiny and large quaternions representable
	orientation /= max(max(abs(orientation.x), abs(orientation.y)), max(abs(orientation.z), abs(orientation.w)));
	orientation = normalize(orientation);

	// Transform the vertex and forward its color, texture coordinates, and normal
	vec3 point = position.xyz / position.w + rotate(orientation, localPosition * extent);
	gl_Position = viewing.modelviewProjection * vec4(point, 1.0);
	color = fetchAttribute(3, instance, 4);
	uv = localUV;
	eyeNormal = viewing.normal * rotate(orientation, localNormal);
}
