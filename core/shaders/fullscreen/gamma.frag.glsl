#version 450
#extension GL_GOOGLE_include_directive : require


//////
//
// Includes
//

// Common gamma correction code
#include "gamma.glsl"



//////
//
// Uniforms
//

/// The source texture.
layout(set=2, binding=0) uniform sampler2D sourceTex;



//////
//
// Streams
//

////
// Inputs

/// The fragment texture coordinates.
layout(location=0) in vec2 uv;


////
// Outputs

/// The corrected color with sampled alpha.
layout(location=0) out vec4 color;



//////
//
// Functions
//

/// The shader entry point.
void main () {
	vec4 linear = texture(sourceTex, uv);
	color = vec4(applyGamma(linear.rgb), linear.a);
}
