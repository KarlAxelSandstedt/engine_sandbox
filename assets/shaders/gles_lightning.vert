attribute vec3 a_position;      	/* local:  vertex position	*/
attribute vec4 a_color;         	/* local: instance color	*/
attribute vec3 a_normal;        	/* local:  vertex normal	*/

uniform float aspect_ratio;
uniform mat4 view;
uniform mat4 perspective;

varying vec3 FragPos;
varying vec4 color;
varying vec3 normal;

void main()
{
	color = a_color;
	gl_Position = perspective * view * vec4(a_position, 1.0);
	normal = a_normal;
	FragPos = a_position;
}
