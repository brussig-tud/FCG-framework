#version 450
#extension GL_GOOGLE_include_directive : require


//////
//
// Includes
//

// Shared clipping and pixel stroke expansion
#include "stroke_vertex.glsl"



//////
//
// Uniforms
//

/// Homogeneous node positions stored as raw 32-bit words.
layout(set=0, binding=0, std430) readonly buffer Positions {
	/// Position components in the caller's attribute layout.
	uint positions[];
};

/// Node half-widths in physical framebuffer pixels.
layout(set=0, binding=1, std430) readonly buffer Radii {
	/// Radius components stored as raw 32-bit words.
	uint radii[];
};

/// Node colors in linear RGBA space.
layout(set=0, binding=2, std430) readonly buffer Colors {
	/// Color components stored as raw 32-bit words.
	uint colors[];
};

/// Explicit pairs of node indices.
layout(set=0, binding=3, std430) readonly buffer Connectivity {
	/// Consecutive 32-bit node indices.
	uint indices[];
};

/// Projection and pixel dimensions; matches the C++ line ViewingUniforms layout.
layout(set=1, binding=0, std140) uniform Viewing {
	/// The object-to-clip transformation with zero-to-one normalized depth.
	mat4 matrix;

	/// The physical viewport dimensions in XY, with ZW unused.
	vec4 viewport;
} viewing;

/// Node addressing and defaults; matches the C++ line AttributeUniforms layout.
layout(set=1, binding=1, std140) uniform Attributes {
	/// Per attribute: word offset, word stride, buffer-enabled flag and unused component.
	uvec4 layouts[3];

	/// Component offsets in words for position, radius and color.
	uvec4 components[3];

	/// Constant or fallback components for position, radius and color.
	vec4 constants[3];

	/// Index-buffer word offset, first segment, round-cap flag and unused component.
	uvec4 connectivity;
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
// Functions
//

/// Read one raw attribute component without changing its bit representation.
float fetch (uint slot, uint address) {
	if (slot == 0)
		return uintBitsToFloat(positions[address]);
	if (slot == 1)
		return uintBitsToFloat(radii[address]);
	return uintBitsToFloat(colors[address]);
}

/// Read the specified number of node components, retaining constant defaults for missing sources.
vec4 nodeAttribute (uint slot, uint node, uint count)
{
	vec4 value = attributes.constants[slot];
	if (attributes.layouts[slot].z != 0) {
		uint start = attributes.layouts[slot].x + node * attributes.layouts[slot].y;
		for (uint i = 0; i < count; ++i)
			value[i] = fetch(slot, start + attributes.components[slot][i]);
	}
	return value;
}

/// The shader entry point.
void main ()
{
	// Fetch the explicit endpoint pair and its node attributes
	uint address = attributes.connectivity.x + 2 * (uint(gl_InstanceIndex) + attributes.connectivity.y);
	uint nodeA = indices[address];
	uint nodeB = indices[address + 1];
	vec4 positionA = nodeAttribute(0, nodeA, 4);
	vec4 positionB = nodeAttribute(0, nodeB, 4);
	vec4 a = viewing.matrix * vec4(positionA.xyz / positionA.w, 1.0);
	vec4 b = viewing.matrix * vec4(positionB.xyz / positionB.w, 1.0);
	vec2 radius = vec2(nodeAttribute(1, nodeA, 1).x, nodeAttribute(1, nodeB, 1).x);
	vec2 corner = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));

	// Expand the stroke and forward its fragment interface
	StrokeVertex vertex = strokeVertex(a, b, radius, nodeAttribute(2, nodeA, 4), nodeAttribute(2, nodeB, 4),
		attributes.connectivity.z, viewing.viewport.xy, corner);
	gl_Position = vertex.position;
	strokeCoord = vertex.strokeCoord;
	geometry = vertex.geometry;
	depths = vertex.depths;
	colorA = vertex.colorA;
	colorB = vertex.colorB;
	caps = vertex.caps;
}
