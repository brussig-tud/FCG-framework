#version 450


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

// The fragment uv coordinates.
layout(location=0) in vec2 uv;


////
// Outputs

// The shaded fragment color.
layout(location=0) out vec4 color;



//////
//
// Functions
//

// The shader entry point.
void main() {
	color = texture(sourceTex, uv); // <- pass through texture values untouched
}
