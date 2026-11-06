
#ifndef FCG_BLINN_PHONG_GLSL
#define FCG_BLINN_PHONG_GLSL


//////
//
// Constants
//

/// The coefficient applied to linear specular reflection colors.
const float specularStrength = 0.25;



//////
//
// Functions
//

/// Normalizes a finite direction without overflowing or underflowing, retaining zero for a zero vector.
vec3 lightingUnit (vec3 value) {
	float scale = max(max(abs(value.x), abs(value.y)), abs(value.z));
	return scale > 0.0 ? normalize(value / scale) : vec3(0.0);
}

/// Evaluates linear RGB Blinn-Phong reflection using eye-space geometry and a positive specular exponent.
vec3 blinnPhong (
	vec3 base, vec3 normal, vec3 eyePosition, vec3 lightDirection,
	vec3 ambient, vec3 diffuse, vec4 specular, bool orthographic
)
{
	// Prepare unit surface, light, viewing and half-vector directions
	vec3 n = lightingUnit(normal);
	vec3 l = lightingUnit(lightDirection);
	vec3 v = orthographic ? vec3(0.0, 0.0, 1.0) : lightingUnit(-eyePosition);
	vec3 h = lightingUnit(l + v);

	// Keep highlights separate from albedo and suppress undefined or unlit half-vectors
	float lambert = max(dot(n, l), 0.0);
	float highlight = 0.0;
	if (lambert > 0.0 && dot(v, v) > 0.0 && dot(h, h) > 0.0)
		highlight = pow(clamp(dot(n, h), 0.0, 1.0), specular.w);
	return base * (ambient + diffuse * lambert) + specularStrength * highlight * specular.rgb;
}


#endif // FCG_BLINN_PHONG_GLSL
