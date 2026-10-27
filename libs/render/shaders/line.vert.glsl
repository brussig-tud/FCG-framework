#version 450


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

/// Clip the parameter interval to a homogeneous half-space whose endpoint distances are a and b.
bool clipPlane (float a, float b, inout float first, inout float last)
{
	if (a < 0.0 && b < 0.0)
		return false;
	if (a < 0.0)
		first = max(first, a / (a - b));
	else if (b < 0.0)
		last = min(last, a / (a - b));
	return first <= last;
}

/// The shader entry point.
void main ()
{
	// Initialize a fully clipped primitive for geometry that produces no stroke
	gl_Position = vec4(0.0, 0.0, -1.0, 1.0);
	strokeCoord = vec2(0.0);
	geometry = vec4(0.0);
	depths = vec4(0.0);
	colorA = vec4(0.0);
	colorB = vec4(0.0);
	caps = uvec2(0);

	// Fetch the explicit endpoint pair and its node attributes
	uint address = attributes.connectivity.x + 2 * (uint(gl_InstanceIndex) + attributes.connectivity.y);
	uint nodeA = indices[address];
	uint nodeB = indices[address + 1];
	vec4 positionA = nodeAttribute(0, nodeA, 4);
	vec4 positionB = nodeAttribute(0, nodeB, 4);
	vec4 a = viewing.matrix * vec4(positionA.xyz / positionA.w, 1.0);
	vec4 b = viewing.matrix * vec4(positionB.xyz / positionB.w, 1.0);
	vec2 radius = vec2(nodeAttribute(1, nodeA, 1).x, nodeAttribute(1, nodeB, 1).x);
	vec4 originalColorA = nodeAttribute(2, nodeA, 4);
	vec4 originalColorB = nodeAttribute(2, nodeB, 4);
	if (max(radius.x, radius.y) == 0.0)
		return;

	// Clip the centerline before dividing by W
	float first = 0.0;
	float last = 1.0;
	if (!clipPlane(a.z, b.z, first, last) || !clipPlane(a.w - a.z, b.w - b.z, first, last))
		return;
	vec4 clippedA = mix(a, b, first);
	vec4 clippedB = mix(a, b, last);
	if (clippedA.w <= 0.0 || clippedB.w <= 0.0)
		return;
	radius = vec2(mix(radius.x, radius.y, first), mix(radius.x, radius.y, last));
	colorA = mix(originalColorA, originalColorB, first);
	colorB = mix(originalColorA, originalColorB, last);
	caps = uvec2(first == 0.0 ? attributes.connectivity.z : 0,
		last == 1.0 ? attributes.connectivity.z : 0);

	// Express the surviving endpoints in a viewport-relative pixel coordinate system
	vec2 pixelScale = viewing.viewport.xy * 0.5;
	vec2 start = clippedA.xy / clippedA.w * pixelScale;
	vec2 stop = clippedB.xy / clippedB.w * pixelScale;
	vec2 delta = stop - start;
	float segmentLength = length(delta);
	vec2 tangent = vec2(1.0, 0.0);
	depths = vec4(clippedA.z / clippedA.w, clippedB.z / clippedB.w,
		1.0 / clippedA.w, 1.0 / clippedB.w);

	// Projected round strokes collapse to a disk while butt strokes disappear
	if (segmentLength < 0.0001)
	{
		if (caps.x == 0 && caps.y == 0)
			return;
		if (depths.y < depths.x) {
			start = stop;
			colorA = colorB;
			depths.x = depths.y;
			depths.z = depths.w;
		}
		colorB = colorA;
		depths.y = depths.x;
		depths.w = depths.z;
		radius = vec2(max(radius.x, radius.y));
		segmentLength = 0.0;
		caps = uvec2(1);
	}
	else
		tangent = delta / segmentLength;
	geometry = vec4(segmentLength, radius, 0.0);

	// Enclose the finite stroke and the complete filter support with one rectangle
	vec2 normal = vec2(-tangent.y, tangent.x);
	float transverse = max(radius.x, radius.y) + 1.0;
	float left = (caps.x != 0 ? -radius.x : 0.0) - 1.0;
	float right = segmentLength + (caps.y != 0 ? radius.y : 0.0) + 1.0;
	vec2 corner = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));
	strokeCoord = vec2(mix(left, right, corner.x), mix(-transverse, transverse, corner.y));
	vec2 ndc = (start + tangent * strokeCoord.x + normal * strokeCoord.y) / pixelScale;
	bool atStart = corner.x == 0.0;
	float clipW = 1.0 / (atStart ? depths.z : depths.w);
	float depth = atStart ? depths.x : depths.y;
	gl_Position = vec4(ndc * clipW, depth * clipW, clipW);
}
