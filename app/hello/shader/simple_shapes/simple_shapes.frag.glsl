#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

layout (location = 0) out vec4 color;

const float shininess = 48;
const vec3 lightDir = normalize(vec3(1/3., 1/3., 1));
const vec3 baseColor = vec3(.15, .55, .9);
const vec3 specularColor = vec3(1);

void main() {
	vec3 N = normalize(normal);
	vec3 L = lightDir;
	vec3 V = normalize(-position);
	vec3 H = normalize(L + V);

	float diffuse = max(dot(N, L), 0);
	float specular = (diffuse > 0) ? pow(max(dot(N, H), 0), shininess) : 0;

	vec3 ambient = .2 * baseColor;
	vec3 diffuseTerm = .8 * diffuse * baseColor;
	vec3 specularTerm = 1/3. * specular * specularColor;

	color = vec4(ambient + diffuseTerm + specularTerm, 1);
}
