
#ifndef FCG_ELLIPSOID_FRAGMENT_GLSL
#define FCG_ELLIPSOID_FRAGMENT_GLSL


//////
//
// Functions
//

/// Reports whether every vector component is finite.
bool finiteVector (in vec3 value) {
	return !any(isnan(value)) && !any(isinf(value));
}

/// Accepts an ellipsoid surface point after analytic culling and zero-to-one clip-depth checks.
bool acceptSurface (
	in vec3 sphere, in bool front, in vec3 eyeCenter, in mat3 sphereToEye,
	in mat4 projection, in uint cullMode, out float depth
)
{
	// Reject the culled side independently of the proxy's winding and depth
	depth = 0.0;
	if ((front && cullMode == 1u) || (!front && cullMode == 2u))
		return false;

	// Project the unit-sphere surface point and retain its normalized depth
	vec3 eye = eyeCenter + sphereToEye * sphere;
	vec4 clip = projection * vec4(eye, 1.0);
	if (any(isnan(clip)) || any(isinf(clip)) || clip.w <= 0.0 || clip.z < 0.0 || clip.z > clip.w)
		return false;
	depth = clip.z / clip.w;
	return true;
}

/// Finds the nearest accepted unit-sphere surface using Gumhold's perspective silhouette parameterization.
bool silhouetteHit (
	in vec2 circle, in vec4 sphereEye, in vec3 basisA, in vec3 basisB,
	in vec3 eyeCenter, in mat3 sphereToEye, in mat4 projection, in uint cullMode,
	out vec3 sphere, out float depth
)
{
	// Reject samples outside the silhouette's unit circle
	sphere = vec3(1, 0, 0);
	depth = 0.0;
	float squared = dot(circle, circle);
	if (squared > 1.0)
		return false;

	// Rearrange the paper's lambda expression to avoid cancellation at distant eyes
	float alpha = sqrt(max(0.0, 1.0 - squared));
	float u = sphereEye.w;
	vec3 lateral = sqrt(max(0.0, 1.0 - u * u)) * (circle.x * basisA + circle.y * basisB);

	// Test the near surface before the far surface
	sphere = ((u + alpha) * sphereEye.xyz + lateral) / (1.0 + u * alpha);
	if (acceptSurface(sphere, true, eyeCenter, sphereToEye, projection, cullMode, depth))
		return true;
	sphere = ((u - alpha) * sphereEye.xyz + lateral) / (1.0 - u * alpha);
	return acceptSurface(sphere, false, eyeCenter, sphereToEye, projection, cullMode, depth);
}

/// Finds the nearest accepted unit-sphere surface along a ray unprojected from normalized device coordinates.
bool generalHit (
	in vec2 screenNdc, in vec3 eyeCenter, in mat3 sphereToEye, in mat3 eyeToSphere,
	in mat4 projection, in mat4 invProjection, in uint cullMode,
	out vec3 sphere, out float depth
)
{
	// Unproject two finite-depth positions to define the viewing ray
	sphere = vec3(1, 0, 0);
	depth = 0.0;
	vec4 nearClip = invProjection * vec4(screenNdc, 0.0, 1.0);
	vec4 middleClip = invProjection * vec4(screenNdc, 0.5, 1.0);
	if (nearClip.w == 0.0 || middleClip.w == 0.0)
		return false;

	// Transform the ray to unit-sphere coordinates and normalize its direction safely
	vec3 nearEye = nearClip.xyz / nearClip.w;
	vec3 middleEye = middleClip.xyz / middleClip.w;
	vec3 origin = eyeToSphere * (nearEye - eyeCenter);
	vec3 direction = eyeToSphere * (middleEye - nearEye);
	float directionScale = max(max(abs(direction.x), abs(direction.y)), abs(direction.z));
	if (!finiteVector(origin) || !finiteVector(direction) || directionScale == 0.0)
		return false;
	direction = normalize(direction / directionScale);

	// Solve geometrically and retain the closest point for stable hit coordinates
	vec3 side = cross(origin, direction);
	float squared = dot(side, side);
	if (squared > 1.0 || isnan(squared) || isinf(squared))
		return false;
	vec3 closest = cross(direction, side);
	float halfChord = sqrt(max(0.0, 1.0 - squared));
	float along = -dot(origin, direction);

	// Test forward intersections in near-to-far order
	sphere = closest - halfChord * direction;
	if (along - halfChord >= 0.0
		&& acceptSurface(sphere, true, eyeCenter, sphereToEye, projection, cullMode, depth))
		return true;
	sphere = closest + halfChord * direction;
	return along + halfChord >= 0.0
		&& acceptSurface(sphere, false, eyeCenter, sphereToEye, projection, cullMode, depth);
}

/// Maps a unit-sphere point to longitude U and polar angle V, using U equal to one half at the poles.
vec2 textureCoordinates (in vec3 sphere) {
	vec3 unit = normalize(sphere);
	float longitude = dot(unit.xz, unit.xz) > 1e-12 ? atan(unit.z, unit.x) : 0.0;
	return vec2(0.5 + longitude / 6.28318530718, acos(clamp(unit.y, -1.0, 1.0)) / 3.14159265359);
}


#endif // FCG_ELLIPSOID_FRAGMENT_GLSL
