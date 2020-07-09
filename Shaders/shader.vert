#version 450 //Use GLSL 4.5

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 color;

layout(location = 0) out vec3 fragCol;
void main()
{
  fragCol = color;
  gl_Position = vec4(pos, 1.0);
}
