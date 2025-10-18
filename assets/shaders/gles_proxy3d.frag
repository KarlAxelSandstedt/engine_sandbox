precision mediump float;

varying vec3 FragPos;
varying vec4 color;
varying vec3 normal;

uniform vec3 light_position;

void main()
{
	vec3 light_dir = normalize(FragPos - light_position);
	float diff = max(-dot(light_dir, normal), 0.0);
	gl_FragColor = vec4(0.8 * diff * color.xyz + 0.2 * color.xyz, color.w);
}
