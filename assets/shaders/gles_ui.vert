/* Note: input sizes are in pixels */

uniform vec2	resolution;

attribute vec4	position_uv;		/* local:  corner pixel position[2], uv[2]  	*/
attribute vec4	node_rect;		/* shared: center[2], size[2] 			*/
attribute vec4	visible_rect;		/* shared: center[2], size[2] 			*/
attribute vec4	background_color;	/* shared					*/
attribute vec4	border_color;		/* shared					*/
attribute vec4	sprite_color;		/* shared					*/
attribute vec4	gradient_color; 	/* local:  gradient corner color 		*/
attribute vec3	extra;			/* shared: border_size[1]
					   local: corner_radius[1]
					   shared: edge_softness[1] 			*/
varying vec2 	uv;
varying vec2 	n_rect_center;
varying vec2 	v_rect_center;
varying vec2 	n_rect_halfsize;
varying vec2 	v_rect_halfsize;
varying vec4 	bg_color;
varying vec4 	br_color;
varying vec4 	sp_color;
varying vec4 	gr_color;
varying	float 	border_size;
varying	float 	corner_radius;
varying	float 	edge_softness;

void main()
{
	uv = position_uv.zw;
/*
	n_rect_center = 2.0*(node_rect.xy) / resolution - 1.0;
	v_rect_center = 2.0*(visible_rect.xy) / resolution - 1.0;
	n_rect_halfsize = 2.0*node_rect.zw / resolution;
	v_rect_halfsize = 2.0*visible_rect.zw / resolution;
*/
	n_rect_center = node_rect.xy;
	v_rect_center = visible_rect.xy;
	n_rect_halfsize = node_rect.zw;
	v_rect_halfsize = visible_rect.zw;
	bg_color = background_color;
	br_color = border_color;
	sp_color = sprite_color;
	gr_color = gradient_color;
	border_size = extra.x;
	corner_radius = extra.y;
	edge_softness = extra.z;

	gl_Position = vec4(2.0*(position_uv.xy / resolution) - 1.0, 0.0, 1.0);
}
