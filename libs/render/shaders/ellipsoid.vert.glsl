#version 450


//////
//
// Uniforms
//

/// Homogeneous ellipsoid centers stored as raw 32-bit words.
layout(set=0, binding=0, std430) readonly buffer Positions {
	/// Position components interpreted as floating-point values by the attribute layout.
	uint positions[];
};

/// Ellipsoid semi-axis lengths stored as raw 32-bit words.
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

/// The proxy corner with X and Y equal to minus or plus one.
layout(location=0) in vec3 localPosition;


////
// Outputs

/// The ellipsoid's RGBA color.
layout(location=0) flat out vec4 color;

/// The ellipsoid center in eye coordinates.
layout(location=1) flat out vec3 eyeCenter;

/// The linear transformation from centered unit-sphere vectors to eye coordinates.
layout(location=2) flat out mat3 sphereToEye;

/// The linear transformation from eye-space vectors to centered unit-sphere coordinates.
layout(location=5) flat out mat3 eyeToSphere;

/// The unit-sphere direction toward the eye in XYZ and reciprocal eye distance in W.
layout(location=8) flat out vec4 sphereEye;

/// The first unit-sphere basis vector spanning the perspective silhouette plane.
layout(location=9) flat out vec3 basisA;

/// The second unit-sphere basis vector spanning the perspective silhouette plane.
layout(location=10) flat out vec3 basisB;

/// The perspective-interpolated coordinates whose unit circle bounds the silhouette.
layout(location=11) out vec2 circle;

/// The screen-linear normalized device coordinates within minus one to one.
layout(location=12) noperspective out vec2 screenNdc;

/// The intersection path, with zero for empty, one for a general ray, and two for the silhouette.
layout(location=13) flat out int path;



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

/// Reports whether every vector component is finite.
bool finiteVector (in vec3 value) {
	return !any(isnan(value)) && !any(isinf(value));
}

/// Reports whether every matrix component is finite.
bool finiteMatrix (in mat3 value) {
	return finiteVector(value[0]) && finiteVector(value[1]) && finiteVector(value[2]);
}

/// The shader entry point.
void main ()
{
	// Initialize every output, including instances discarded before rasterization
	gl_Position = vec4(2.0, 2.0, 0.5, 1.0);
	color = vec4(1.0);
	eyeCenter = vec3(0.0);
	sphereToEye = mat3(1.0);
	eyeToSphere = mat3(1.0);
	sphereEye = vec4(0.0);
	basisA = vec3(1.0, 0.0, 0.0);
	basisB = vec3(0.0, 1.0, 0.0);
	circle = localPosition.xy;
	screenNdc = vec2(0.0);
	path = 0;

	// Read the instance attributes and reject collapsed semi-axes
	uint instance = uint(gl_InstanceIndex) + attributes.drawInfo.x;
	vec4 position = fetchAttribute(0, instance, 4);
	vec3 extent = fetchAttribute(1, instance, 3).xyz;
	if (any(lessThanEqual(extent, vec3(0.0))))
		return;
	vec4 orientation = fetchAttribute(2, instance, 4);

	// Normalize the quaternion and construct the sphere-to-eye transformations
	orientation /= max(max(abs(orientation.x), abs(orientation.y)), max(abs(orientation.z), abs(orientation.w)));
	orientation = normalize(orientation);
	mat3 rotation = mat3(rotate(orientation, vec3(1, 0, 0)), rotate(orientation, vec3(0, 1, 0)),
		rotate(orientation, vec3(0, 0, 1)));
	mat3 scale = mat3(extent.x, 0, 0, 0, extent.y, 0, 0, 0, extent.z);
	mat3 inverseScale = mat3(1.0 / extent.x, 0, 0, 0, 1.0 / extent.y, 0, 0, 0, 1.0 / extent.z);
	eyeCenter = (viewing.modelview * vec4(position.xyz / position.w, 1.0)).xyz;
	sphereToEye = mat3(viewing.modelview) * rotation * scale;
	eyeToSphere = inverseScale * transpose(rotation) * mat3(viewing.invModelview);
	if (!finiteVector(eyeCenter) || !finiteMatrix(sphereToEye) || !finiteMatrix(eyeToSphere))
		return;
	color = fetchAttribute(3, instance, 4);

	// Build Gumhold's silhouette circle in unit-sphere coordinates for an external perspective eye
	vec3 eye = -(eyeToSphere * eyeCenter);
	float eyeScale = max(max(abs(eye.x), abs(eye.y)), abs(eye.z));
	bool perspective = viewing.projection[3][3] == 0.0 && viewing.projection[0][3] == 0.0
		&& viewing.projection[1][3] == 0.0;
	if (perspective && finiteVector(eye) && eyeScale > 0.5)
	{
		// Find a stable reciprocal eye distance in unit-sphere coordinates
		vec3 scaledEye = eye / eyeScale;
		float inverseDistance = (1.0 / eyeScale) / length(scaledEye);
		if (inverseDistance < 0.999)
		{
			// Construct the silhouette plane and its circumscribed square
			sphereEye = vec4(normalize(scaledEye), inverseDistance);
			vec3 helper = abs(sphereEye.x) < 0.8 ? vec3(1, 0, 0) : vec3(0, 1, 0);
			basisA = normalize(cross(sphereEye.xyz, helper));
			basisB = cross(sphereEye.xyz, basisA);
			float radius = sqrt(max(0.0, 1.0 - inverseDistance * inverseDistance));

			// Require every square corner to remain safely in front of the eye plane
			bool safe = true;
			vec4 vertex = vec4(0.0);
			for (int corner = 0; corner < 4; ++corner)
			{
				vec2 signs = vec2((corner & 1) != 0 ? 1.0 : -1.0, (corner & 2) != 0 ? 1.0 : -1.0);
				vec3 sphere = inverseDistance * sphereEye.xyz + radius * (signs.x * basisA + signs.y * basisB);
				vec4 clip = viewing.projection * vec4(eyeCenter + sphereToEye * sphere, 1.0);
				safe = safe && !any(isnan(clip)) && !any(isinf(clip)) && clip.w > 1e-6;
				if (all(equal(signs, localPosition.xy)))
					vertex = clip;
			}

			// Keep proxy depth inside the clip range and let the fragment stage supply surface depth
			if (safe) {
				path = 2;
				gl_Position = vec4(vertex.xy, 0.5 * vertex.w, vertex.w);
				screenNdc = vertex.xy / vertex.w;
				return;
			}
		}
	}

	// Project a conservative enclosing box; crossing the eye plane requires a full viewport proxy
	vec2 lower = vec2(1e30);
	vec2 upper = vec2(-1e30);
	bool allInFront = true;
	bool allBehind = true;
	for (int corner = 0; corner < 8; ++corner)
	{
		vec3 signs = vec3((corner & 1) != 0 ? 1.0 : -1.0, (corner & 2) != 0 ? 1.0 : -1.0,
			(corner & 4) != 0 ? 1.0 : -1.0);
		vec4 clip = viewing.projection * vec4(eyeCenter + sphereToEye * signs, 1.0);
		allInFront = allInFront && clip.w > 1e-6 && !any(isnan(clip)) && !any(isinf(clip));
		allBehind = allBehind && clip.w <= 0.0;
		if (clip.w > 1e-6) {
			lower = min(lower, clip.xy / clip.w);
			upper = max(upper, clip.xy / clip.w);
		}
	}

	// Cull hidden boxes and expand eye-plane crossings to the full viewport
	if (allBehind)
		return;
	if (!allInFront) {
		lower = vec2(-1.0);
		upper = vec2(1.0);
	}
	lower = max(lower, vec2(-1.0));
	upper = min(upper, vec2(1.0));
	if (any(greaterThanEqual(lower, upper)))
		return;

	// Emit the conservative rectangle for general per-sample raycasting
	path = 1;
	screenNdc = mix(lower, upper, 0.5 * localPosition.xy + 0.5);
	gl_Position = vec4(screenNdc, 0.5, 1.0);
}
