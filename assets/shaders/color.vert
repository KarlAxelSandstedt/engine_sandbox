#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;

uniform float aspect_ratio;
uniform mat4 view;
uniform mat4 perspective;

out vec4 out_color;

void main()
{
	out_color = color;
 	gl_Position = perspective * view * vec4(position, 1.0f);
}
