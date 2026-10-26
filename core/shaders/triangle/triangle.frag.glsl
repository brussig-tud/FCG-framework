#version 450


//////
//
// Uniforms
//

/* none */



//////
//
// Streams
//

////
// Inputs

/// The interpolated vertex color.
layout(location=0) in vec3 vColor;


////
// Outputs

/// The final fragment color.
layout(location=0) out vec4 outColor;



//////
//
// Functions
//

/// The shader entry point.
void main () {
	outColor = vec4(vColor, 1);
}
