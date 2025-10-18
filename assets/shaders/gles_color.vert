attribute vec3 position;
attribute vec4 color;

uniform float aspect_ratio;
uniform mat4 view;
uniform mat4 perspective;

varying vec4 out_color;

void main()
{
	out_color = color;
 	gl_Position = perspective * view * vec4(position, 1.0);
}
