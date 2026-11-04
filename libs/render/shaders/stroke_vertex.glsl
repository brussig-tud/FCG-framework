
#ifndef FCG_STROKE_VERTEX_GLSL
#define FCG_STROKE_VERTEX_GLSL


//////
//
// Structs
//

/// Expanded stroke data consumed by the line fragment interface.
struct StrokeVertex
{
	/// Generated rectangle corner in clip coordinates.
	vec4 position;

	/// Pixel coordinates along and perpendicular to the centerline.
	vec2 strokeCoord;

	/// Projected length and endpoint pixel radii in XYZ.
	vec4 geometry;

	/// Endpoint normalized depths in XY and reciprocal clip W in ZW.
	vec4 depths;

	/// Linear RGBA color at the surviving first endpoint.
	vec4 colorA;

	/// Linear RGBA color at the surviving second endpoint.
	vec4 colorB;

	/// Round-cap flags at the surviving endpoints.
	uvec2 caps;
};



//////
//
// Functions
//

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

/// Expand clip-space endpoints into one stroke rectangle corner using physical pixel widths.
StrokeVertex strokeVertex (
	vec4 a, vec4 b, vec2 radius, vec4 originalColorA, vec4 originalColorB,
	uint roundCap, vec2 viewport, vec2 corner
)
{
	// Initialize a fully clipped result for geometry that produces no stroke
	StrokeVertex result = StrokeVertex(vec4(0.0, 0.0, -1.0, 1.0), vec2(0.0), vec4(0.0),
		vec4(0.0), vec4(0.0), vec4(0.0), uvec2(0));
	if (max(radius.x, radius.y) == 0.0)
		return result;

	// Clip the centerline before dividing by W
	float first = 0.0;
	float last = 1.0;
	if (!clipPlane(a.z, b.z, first, last) || !clipPlane(a.w - a.z, b.w - b.z, first, last))
		return result;
	vec4 clippedA = mix(a, b, first);
	vec4 clippedB = mix(a, b, last);
	if (clippedA.w <= 0.0 || clippedB.w <= 0.0)
		return result;
	radius = vec2(mix(radius.x, radius.y, first), mix(radius.x, radius.y, last));
	result.colorA = mix(originalColorA, originalColorB, first);
	result.colorB = mix(originalColorA, originalColorB, last);
	result.caps = uvec2(first == 0.0 ? roundCap : 0,
		last == 1.0 ? roundCap : 0);

	// Express the surviving endpoints in a viewport-relative pixel coordinate system
	vec2 pixelScale = viewport * 0.5;
	vec2 start = clippedA.xy / clippedA.w * pixelScale;
	vec2 stop = clippedB.xy / clippedB.w * pixelScale;
	vec2 delta = stop - start;
	float segmentLength = length(delta);
	vec2 tangent = vec2(1.0, 0.0);
	result.depths = vec4(clippedA.z / clippedA.w, clippedB.z / clippedB.w,
		1.0 / clippedA.w, 1.0 / clippedB.w);

	// Projected round strokes collapse to a disk while butt strokes disappear
	if (segmentLength < 0.0001)
	{
		if (result.caps.x == 0 && result.caps.y == 0)
			return result;
		if (result.depths.y < result.depths.x) {
			start = stop;
			result.colorA = result.colorB;
			result.depths.x = result.depths.y;
			result.depths.z = result.depths.w;
		}
		result.colorB = result.colorA;
		result.depths.y = result.depths.x;
		result.depths.w = result.depths.z;
		radius = vec2(max(radius.x, radius.y));
		segmentLength = 0.0;
		result.caps = uvec2(1);
	}
	else
		tangent = delta / segmentLength;
	result.geometry = vec4(segmentLength, radius, 0.0);

	// Enclose the finite stroke and the complete filter support with one rectangle
	vec2 normal = vec2(-tangent.y, tangent.x);
	float transverse = max(radius.x, radius.y) + 1.0;
	float left = (result.caps.x != 0 ? -radius.x : 0.0) - 1.0;
	float right = segmentLength + (result.caps.y != 0 ? radius.y : 0.0) + 1.0;
	result.strokeCoord = vec2(mix(left, right, corner.x), mix(-transverse, transverse, corner.y));
	vec2 ndc = (start + tangent * result.strokeCoord.x + normal * result.strokeCoord.y) / pixelScale;
	bool atStart = corner.x == 0.0;
	float clipW = 1.0 / (atStart ? result.depths.z : result.depths.w);
	float depth = atStart ? result.depths.x : result.depths.y;
	result.position = vec4(ndc * clipW, depth * clipW, clipW);
	return result;
}


#endif // FCG_STROKE_VERTEX_GLSL
