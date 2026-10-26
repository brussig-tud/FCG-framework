#version 450
#extension GL_GOOGLE_include_directive : require


//////
//
// Includes
//

// Shared analytic ellipsoid intersections and spherical texture coordinates
#include "ellipsoid_fragment.glsl"



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

/// Surface projection and culling parameters; matches the C++ RaycastUniforms layout.
layout(set=3, binding=1, std140) uniform Raycast {
	/// The eye-to-clip transformation with zero-to-one normalized depth.
	mat4 projection;

	/// The clip-to-eye transformation used to construct general viewing rays.
	mat4 invProjection;

	/// The surface cull mode in X, with zero for none, one for front, two for back, and YZW reserved.
	uvec4 flags;
} raycast;



//////
//
// Streams
//

////
// Inputs

/// The ellipsoid's RGBA color.
layout(location=0) flat in vec4 color;

/// The ellipsoid center in eye coordinates.
layout(location=1) flat in vec3 eyeCenter;

/// The linear transformation from centered unit-sphere vectors to eye coordinates.
layout(location=2) flat in mat3 sphereToEye;

/// The linear transformation from eye-space vectors to centered unit-sphere coordinates.
layout(location=5) flat in mat3 eyeToSphere;

/// The unit-sphere direction toward the eye in XYZ and reciprocal eye distance in W.
layout(location=8) flat in vec4 sphereEye;

/// The first unit-sphere basis vector spanning the perspective silhouette plane.
layout(location=9) flat in vec3 basisA;

/// The second unit-sphere basis vector spanning the perspective silhouette plane.
layout(location=10) flat in vec3 basisB;

/// The perspective-interpolated coordinates whose unit circle bounds the silhouette.
layout(location=11) sample in vec2 circle;

/// The screen-linear normalized device coordinates within minus one to one.
layout(location=12) sample noperspective in vec2 screenNdc;

/// The intersection path, with zero for empty, one for a general ray, and two for the silhouette.
layout(location=13) flat in int path;


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
	// Find the nearest accepted ellipsoid surface for this sample
	vec3 sphere = vec3(1, 0, 0);
	float depth = 0.0;
	bool hit = false;
	if (path == 2)
		hit = silhouetteHit(circle, sphereEye, basisA, basisB,
			eyeCenter, sphereToEye, raycast.projection, raycast.flags.x, sphere, depth);
	else if (path == 1)
		hit = generalHit(screenNdc, eyeCenter, sphereToEye, eyeToSphere,
			raycast.projection, raycast.invProjection, raycast.flags.x, sphere, depth);
	vec4 base = color;

	// Evaluate gradients before discard and unwrap longitude derivatives across the seam
	vec2 uv = textureCoordinates(sphere);
	vec2 dx = dFdx(uv);
	vec2 dy = dFdy(uv);
	dx.x -= round(dx.x);
	dy.x -= round(dy.x);
	base *= textureGrad(colorTexture, uv, dx, dy);

	// Discard missed samples and apply optional lighting with the outward eye-space normal
	if (!hit)
		discard;
	if (lighting.direction.w != 0.0) {
		vec3 n = transpose(eyeToSphere) * sphere;
		float scale = max(max(abs(n.x), abs(n.y)), abs(n.z));
		n = normalize(n / scale);
		base.rgb *= lighting.ambient.rgb + lighting.diffuse.rgb * max(dot(n, lighting.direction.xyz), 0.0);
	}

	// Write the analytic surface depth and shaded color
	gl_FragDepth = depth;
	outputColor = base;
}
