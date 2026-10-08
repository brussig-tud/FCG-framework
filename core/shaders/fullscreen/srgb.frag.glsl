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
void main()
{
	// Fetch color and alpha
	vec4 linear = texture(sourceTex, uv);
	vec3 rgb = clamp(linear.rgb, 0.0, 1.0);

	// Convert
	color = vec4(
		/* to sRGB color: */ mix(1.055*pow(rgb, vec3(1/2.4))-.055, 12.92*rgb, lessThanEqual(rgb, vec3(.0031308))),
		/*  linear alpha: */ linear.a
	);
}
