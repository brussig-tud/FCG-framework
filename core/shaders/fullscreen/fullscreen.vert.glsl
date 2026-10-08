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

// The vertex uv coordinates.
layout(location = 0) out vec2 uv;



//////
//
// Functions
//

// The shader entry point.
void main() {
	// On-the-fly vertex generation via Morton index
	uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(/* x: */2*uv.x - 1, /* y: */1 - 2*uv.y,   0, 1);
}
