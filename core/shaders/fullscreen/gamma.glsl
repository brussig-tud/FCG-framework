
#ifndef FCG_FULLSCREEN_GAMMA_GLSL
#define FCG_FULLSCREEN_GAMMA_GLSL


//////
//
// Uniforms
//

/// Gamma and final RGB bounds; matches the C++ GammaUniforms layout.
layout(std140, set=3, binding=0) uniform GammaUniforms
{
	float gamma;
	vec2 clampRange;
} gammaUniforms;



//////
//
// Functions
//

/// Apply reciprocal-gamma correction followed by clamping, including when gamma is one.
vec3 applyGamma (vec3 rgb) {
	if (gammaUniforms.gamma != 1.0)
		rgb = pow(rgb, vec3(1.0 / gammaUniforms.gamma));
	return clamp(rgb, vec3(gammaUniforms.clampRange.x), vec3(gammaUniforms.clampRange.y));
}


#endif // FCG_FULLSCREEN_GAMMA_GLSL
