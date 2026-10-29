#version 450


//////
//
// Uniforms
//

/// Viewing matrices, matches the C++ ViewingUniforms layout.
layout(set=1, binding=0, std140) uniform ViewingMatrices {
	/// The object-to-eye transformation.
	mat4 modelview;

	/// The eye-to-object transformation.
	mat4 invModelview;

	/// The eye-to-clip transformation with zero-to-one normalized depth.
	mat4 projection;

	/// The clip-to-eye transformation.
	mat4 invProjection;

	/// The combined object-to-clip transformation.
	mat4 modelviewProjection;

	/// The combined clip-to-object transformation.
	mat4 invModelviewProjection;

	/// The inverse-transpose object-to-eye transformation for normals.
	mat3 normal;

	/// The inverse of the object-to-eye normal transformation.
	mat3 invNormal;
} mat;



//////
//
// Streams
//

////
// Inputs

/// The shape vertex position.
layout(location=0) in vec4 position;

/// The shape vertex unit normal.
layout(location=1) in vec4 normal;


////
// Outputs

/// The eye-space position on the shape.
layout(location=0) out vec3 position_fs;

/// The eye-space normal at the position on the shape.
layout(location=1) out vec3 normal_fs;



//////
//
// Functions
//

/// The shader entry point.
void main() {
	vec4 position_eyeSpace = mat.modelview * position;
	position_fs = position_eyeSpace.xyz / position_eyeSpace.w;
	normal_fs = normalize(mat.normal * normal.xyz);
	gl_Position = mat.projection * position_eyeSpace;
}
