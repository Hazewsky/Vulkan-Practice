#version 450 //Use GLSL 4.5

layout(location = 0) in vec3 pos;
layout(location = 1) in vec4 color;
layout(location = 2) in vec3 normal;
layout(binding = 0) uniform UboViewProjection
{
	mat4 projection;
	mat4 view;
} uboViewProjection;

// NOT IN USE, SAPMLE OF PARSING UBO DATA AS DYNAMIC UNIFORM 
layout(binding = 1) uniform UboModel
{
	mat4 model;
} uboModel;

layout(push_constant) uniform PushModel
{
	mat4 model;
	//TODO::calculate normal matrix on the CPU instead on the GPU
} pushModel;

layout(location = 0) out vec4 FragCol;
layout(location = 1) out vec3 FragPos;
layout(location = 2) out vec3 Normal;

void main()
{
	gl_Position = uboViewProjection.projection * uboViewProjection.view * pushModel.model * vec4(pos, 1.0);
	FragCol = color;
	//get fragment pos in world space
	FragPos = vec3(pushModel.model * vec4(pos, 1.0));
	//Yeah, I know inversing matrices per vertex is a contly operation. Yeah, someday I'll turn it into push constant
	Normal = mat3(transpose(inverse(pushModel.model))) * normal;
}
