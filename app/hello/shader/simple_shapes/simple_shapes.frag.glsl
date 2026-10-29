#version 450



//////
//
// Streams
//

////
// Inputs

/// The eye-space position on the shape.
layout(location=0) in vec3 position;

/// The eye-space normal at the position on the shape.
layout(location=1) in vec3 normal;


////
// Outputs

/// The final fragment color.
layout(location=0) out vec4 color;



//////
//
// Constants
//

/// The surface material albedo.
const vec3 baseColor = vec3(1/32., .25, 2/3.);

/// The Blinn-Phong shininess of the surface material.
const float shininess = 48;

/// The specular reflection color of the surface material.
const vec3 specularColor = vec3(1);

/// The light direction.
const vec3 lightDir = normalize(vec3(1/3., 1/3., 1));



//////
//
// Functions
//

/// The shader entry point.
void main()
{
	vec3 N = normalize(normal);
	vec3 L = lightDir;
	vec3 V = normalize(-position);
	vec3 H = normalize(L + V);

	float diffuse = max(dot(N, L), 0);
	float specular = (diffuse > 0) ? pow(max(dot(N, H), 0), shininess) : 0;

	vec3 ambient = .1 * baseColor;
	vec3 diffuseTerm = .8 * diffuse * baseColor;
	vec3 specularTerm = .25 * specular * specularColor;

	color = vec4(ambient + diffuseTerm + specularTerm, 1);
}
