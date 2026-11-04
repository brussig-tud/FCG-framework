
#ifndef FCG_PRIMITIVE_TRANSFORM_GLSL
#define FCG_PRIMITIVE_TRANSFORM_GLSL


//////
//
// Functions
//

/// Normalize a finite nonzero XYZW quaternion after rescaling its components.
vec4 normalizedOrientation (vec4 orientation) {
	orientation /= max(max(abs(orientation.x), abs(orientation.y)), max(abs(orientation.z), abs(orientation.w)));
	return normalize(orientation);
}

/// Rotate a local vector by a normalized quaternion in XYZW order.
vec3 rotate (vec4 q, vec3 p) {
	return p + 2.0 * cross(q.xyz, cross(q.xyz, p) + q.w * p);
}


#endif // FCG_PRIMITIVE_TRANSFORM_GLSL
