#version 450


//////
//
// Uniforms
//

/// Coverage options; matches the C++ AppearanceUniforms layout.
layout(set=3, binding=0, std140) uniform Appearance {
	/// Smooth-filter and blending flags in XY, with ZW unused.
	uvec4 flags;
} appearance;



//////
//
// Streams
//

////
// Inputs

/// Pixel coordinates along and perpendicular to the projected centerline.
layout(location=0) noperspective in vec2 strokeCoord;

/// Projected length and endpoint pixel radii in XYZ, with W unused.
layout(location=1) flat in vec4 geometry;

/// Endpoint normalized depths in XY and reciprocal clip W values in ZW.
layout(location=2) flat in vec4 depths;

/// Linear RGBA color at the surviving first endpoint.
layout(location=3) flat in vec4 colorA;

/// Linear RGBA color at the surviving second endpoint.
layout(location=4) flat in vec4 colorB;

/// Round-cap flags for the surviving first and second endpoints.
layout(location=5) flat in uvec2 caps;


////
// Outputs

/// Straight-alpha linear RGBA stroke color with filtered coverage.
layout(location=0) out vec4 outputColor;



//////
//
// Functions
//

/// Euclidean pixel distance to a finite edge, including a collapsed edge.
float edgeDistance (vec2 point, vec2 a, vec2 b) {
	vec2 edge = b - a;
	float t = clamp(dot(point - a, edge) / max(dot(edge, edge), 1e-20), 0.0, 1.0);
	return length(point - a - t * edge);
}

/// Signed pixel distance to the tapered body and its finite cap boundaries.
float strokeDistance (vec2 point)
{
	// Symmetry lets the upper side represent both tapered side boundaries
	float span = geometry.x;
	vec2 radius = geometry.yz;
	if (span == 0.0)
		return length(point) - radius.x;
	vec2 p = vec2(point.x, abs(point.y));
	float distance = edgeDistance(p, vec2(0.0, radius.x), vec2(span, radius.y));

	// Round boundaries use only the outward semicircle, including its nearest endpoint
	float startDistance;
	if (caps.x != 0)
		startDistance = p.x <= 0.0 ? abs(length(p) - radius.x) : length(p - vec2(0.0, radius.x));
	else
		startDistance = edgeDistance(p, vec2(0.0), vec2(0.0, radius.x));
	float stopDistance;
	if (caps.y != 0)
		stopDistance = p.x >= span ? abs(length(p - vec2(span, 0.0)) - radius.y)
			: length(p - vec2(span, radius.y));
	else
		stopDistance = edgeDistance(p, vec2(span, 0.0), vec2(span, radius.y));
	distance = min(distance, min(startDistance, stopDistance));

	// Determine the sign from the finite body or the appropriate outward cap
	bool inside;
	if (p.x < 0.0)
		inside = caps.x != 0 && length(p) <= radius.x;
	else if (p.x > span)
		inside = caps.y != 0 && length(p - vec2(span, 0.0)) <= radius.y;
	else
		inside = p.y <= mix(radius.x, radius.y, p.x / span);
	return inside ? -distance : distance;
}

/// The shader entry point.
void main ()
{
	// Evaluate derivatives before any fragment is discarded
	float distance = strokeDistance(strokeCoord);
	float footprint = max(fwidth(distance), 1.0);
	float coverage = 1.0 - smoothstep(-0.5 * footprint, 0.5 * footprint, distance);
	if (appearance.flags.x != 0) {
		if (coverage <= 0.0)
			discard;
	}
	else {
		if (distance > 0.0)
			discard;
		coverage = 1.0;
	}

	// Interpolate color along the 3D centerline and keep endpoint colors across caps
	float t = geometry.x == 0.0 ? 0.0 : clamp(strokeCoord.x / geometry.x, 0.0, 1.0);
	float weightA = (1.0 - t) * depths.z;
	float weightB = t * depths.w;
	vec4 color = (colorA * weightA + colorB * weightB) / (weightA + weightB);
	if (appearance.flags.y != 0 && color.a <= 0.0)
		discard;

	// The finite centerline supplies depth independently of the expanded rasterization rectangle
	gl_FragDepth = mix(depths.x, depths.y, t);
	outputColor = vec4(color.rgb, color.a * coverage);
}
