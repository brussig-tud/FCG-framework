
#ifndef FCG_FULLSCREEN_GAMMA_GLSL
#define FCG_FULLSCREEN_GAMMA_GLSL


//////
//
// Uniforms
//

/// Gamma and final RGB bounds; matches the C++ GammaUniforms layout.
layout(std140, set=3, binding=0) uniform GammaUniforms {
	/// Per-channel gamma values
	vec3 gamma;

	/// Common channel clamp range
	vec2 clampRange;
} gammaUniforms;



//////
//
// Functions
//

/// Apply per-channel reciprocal-gamma correction and clamp even channels with neutral gamma.
vec3 applyGamma (in vec3 rgb) {
	if (any(notEqual(gammaUniforms.gamma, vec3(1))))
		rgb = mix(pow(rgb, 1/gammaUniforms.gamma), rgb, equal(gammaUniforms.gamma, vec3(1)));
	return clamp(rgb, vec3(gammaUniforms.clampRange.x), vec3(gammaUniforms.clampRange.y));
}


#endif // FCG_FULLSCREEN_GAMMA_GLSL
