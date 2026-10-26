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

/* none */


////
// Outputs

/// The vertex color.
layout(location=0) out vec3 vColor;



//////
//
// Constants
//

/// The hardcoded triangle vertex positions.
const vec3 positions[3] = vec3[](
	vec3( 0, -.5, 0), vec3(.5, .5, 0), vec3(-.5,  .5, 0)
);

/// The hardcoded triangle vertex colors.
const vec3 colors[3] = vec3[](
	vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1)
);



//////
//
// Functions
//

/// The shader entry point.
void main () {
	gl_Position = vec4(positions[gl_VertexIndex], 1);
	vColor = colors[gl_VertexIndex];
}
