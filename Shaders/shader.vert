#version 450 //Use GLSL 4.5

layout(location = 0) in vec3 pos;
layout(location = 1) in vec4 color;

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
} pushModel;

layout(location = 0) out vec4 fragCol;
void main()
{
  fragCol = color;
  gl_Position = uboViewProjection.projection * uboViewProjection.view * pushModel.model * vec4(pos, 1.0);
}
