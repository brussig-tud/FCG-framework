#version 450
#extension GL_GOOGLE_include_directive : require


//////
//
// Includes
//

// Shared clipping and pixel stroke expansion
#include "stroke_vertex.glsl"

// Shared quaternion normalization and rotation
#include "primitive_transform.glsl"



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

/// Per-box stroke half-widths in physical framebuffer pixels.
layout(set=0, binding=4, std430) readonly buffer Radii {
	/// Radius components stored as raw 32-bit words.
	uint radii[];
};

/// Projection and pixel dimensions; matches the C++ WireViewingUniforms layout.
layout(set=1, binding=0, std140) uniform Viewing {
	/// The object-to-clip transformation with zero-to-one normalized depth.
	mat4 matrix;

	/// The physical viewport dimensions in XY, with ZW unused.
	vec4 viewport;
} viewing;

/// Box addressing and defaults; matches the C++ wire-box AttributeUniforms layout.
layout(set=1, binding=1, std140) uniform Attributes {
	/// Per attribute: word offset, word stride, buffer-enabled flag and unused component.
	uvec4 layouts[5];

	/// Component offsets in words for position, extent, orientation, color and radius.
	uvec4 components[5];

	/// Constant or fallback components for position, extent, orientation, color and radius.
	vec4 constants[5];

	/// First box in X and round-cap flag in Y, with ZW unused.
	uvec4 drawInfo;
} attributes;



//////
//
// Streams
//

////
// Inputs

/* none */


////
// Outputs

/// Pixel coordinates along and perpendicular to the projected centerline.
layout(location=0) noperspective out vec2 strokeCoord;

/// Projected length and endpoint pixel radii in XYZ, with W unused.
layout(location=1) flat out vec4 geometry;

/// Endpoint normalized depths in XY and reciprocal clip W values in ZW.
layout(location=2) flat out vec4 depths;

/// Linear RGBA color at the surviving first endpoint.
layout(location=3) flat out vec4 colorA;

/// Linear RGBA color at the surviving second endpoint.
layout(location=4) flat out vec4 colorB;

/// Round-cap flags for the surviving first and second endpoints.
layout(location=5) flat out uvec2 caps;



//////
//
// Constants
//

/// Cube endpoint pairs ordered along X, Y and Z, using XYZ sign bits for corner indices.
const uvec2 edges[12] = uvec2[12](
	uvec2(0,1), uvec2(2,3), uvec2(4,5), uvec2(6,7),
	uvec2(0,2), uvec2(1,3), uvec2(4,6), uvec2(5,7),
	uvec2(0,4), uvec2(1,5), uvec2(2,6), uvec2(3,7)
);



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
	if (slot == 3)
		return uintBitsToFloat(colors[address]);
	return uintBitsToFloat(radii[address]);
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

/// Decode a unit-cube corner spanning minus one to one along each local axis.
vec3 cubeCorner (uint index) {
	return 2.0 * vec3(float(index & 1), float((index >> 1) & 1), float((index >> 2) & 1)) - 1.0;
}

/// The shader entry point.
void main ()
{
	// Read one box's placement and appearance
	uint instance = uint(gl_InstanceIndex) + attributes.drawInfo.x;
	vec4 position = fetchAttribute(0, instance, 4);
	vec3 extent = fetchAttribute(1, instance, 3).xyz;
	vec4 orientation = normalizedOrientation(fetchAttribute(2, instance, 4));
	vec4 color = fetchAttribute(3, instance, 4);
	float radius = fetchAttribute(4, instance, 1).x;

	// Transform both endpoints of the selected local edge
	uvec2 pair = edges[uint(gl_VertexIndex) / 4];
	vec3 center = position.xyz / position.w;
	vec4 a = viewing.matrix * vec4(center + rotate(orientation, cubeCorner(pair.x) * extent), 1.0);
	vec4 b = viewing.matrix * vec4(center + rotate(orientation, cubeCorner(pair.y) * extent), 1.0);
	vec2 corner = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));

	// Expand the stroke and forward the shared fragment interface
	StrokeVertex vertex = strokeVertex(a, b, vec2(radius), color, color,
		attributes.drawInfo.y, viewing.viewport.xy, corner);
	gl_Position = vertex.position;
	strokeCoord = vertex.strokeCoord;
	geometry = vertex.geometry;
	depths = vertex.depths;
	colorA = vertex.colorA;
	colorB = vertex.colorB;
	caps = vertex.caps;
}
