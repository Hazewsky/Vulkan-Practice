#version 450

layout(location = 0) in vec4 FragCol;
layout(location = 1) in vec3 FragPos;
layout(location = 2) in vec3 Normal;
layout(location = 3) in vec2 UVs;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D textureSampler;

/*layout(push_constant) uniform LightingModel
{
	vec3 ambientLightColor;
	vec3 diffuseLightColor;
	vec3 specularLightColor;
	vec3 lightPos;
	vec3 viewPos;
	vec3 reflectionShininess;
} lightingModel;*/

void main()
{
	//TODO: put into fragment shader push constant
	vec3 lightColor = vec3(1.0, 1.0, 1.0);
	vec3 lightPos = vec3(0.0, 0.0, -3.5);
	vec3 viewPos = vec3(0.0, 0.0, 1.0);
	// ambient lighting
	float ambientStrength = 0.15;
	vec3 ambient = ambientStrength * lightColor;
	// diffuse lighting
	vec3 norm = normalize(Normal);
	vec3 lightDir = normalize(lightPos - vec3(FragPos));
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * lightColor;

	// specular lighting
	float specularStrength = 0.5;
	vec3 viewDir = normalize(viewPos - vec3(FragPos));
	vec3 reflectDir = reflect(-lightDir, Normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
	vec3 specular = specularStrength * spec * lightColor;

	vec4 resultingColor = vec4(ambient + diffuse + specular, 1.0) * FragCol;
	outColor = texture(textureSampler, UVs) * resultingColor;
}
