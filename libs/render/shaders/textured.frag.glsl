#version 450


//////
//
// Uniforms
//

/// The primitive's RGBA color texture.
layout(set=2, binding=0) uniform sampler2D colorTexture;

/// Directional lighting parameters; matches the C++ LightUniforms layout.
layout(set=3, binding=0, std140) uniform Lighting {
	/// The normalized eye-space direction toward the light in XYZ, with nonzero W enabling lighting.
	vec4 direction;

	/// The ambient RGB intensity, with nonzero W enabling two-sided quad normals.
	vec4 ambient;

	/// The diffuse RGB intensity, with W reserved.
	vec4 diffuse;
} lighting;



//////
//
// Streams
//

////
// Inputs

/// The interpolated primitive RGBA color.
layout(location=0) in vec4 color;

/// The interpolated texture coordinates within the primitive face.
layout(location=1) in vec2 uv;

/// The interpolated outward normal in eye coordinates.
layout(location=2) in vec3 eyeNormal;


////
// Outputs

/// The shaded RGBA fragment color.
layout(location=0) out vec4 outputColor;



//////
//
// Functions
//

/// The shader entry point.
void main ()
{
	// Read the primitive color and modulate it by the texture
	vec4 base = color * texture(colorTexture, uv);

	// Apply optional directional lighting with two-sided normals for quads
	if (lighting.direction.w != 0.0) {
		vec3 n = normalize(eyeNormal);
		if (lighting.ambient.w != 0.0 && !gl_FrontFacing)
			n = -n;
		base.rgb *= lighting.ambient.rgb + lighting.diffuse.rgb * max(dot(n, lighting.direction.xyz), 0.0);
	}

	// Write the shaded color
	outputColor = base;
}
