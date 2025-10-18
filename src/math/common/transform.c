/*
==========================================================================
    Copyright (C) 2025 Axel Sandstedt 

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
==========================================================================
*/

#include "kas_math.h"
#include "transform.h"

void sequential_rotation_matrix(mat3 dst, const vec3 axis_1, const f32 angle_1, const vec3 axis_2, const f32 angle_2)
{
	vec3 axis_snd;
	mat3 r_1, r_2;
	rotation_matrix(r_1, axis_1, angle_1);
	mat3_vec_mul(axis_snd, r_1, axis_2);
	rotation_matrix(r_2, axis_snd, angle_2);
	mat3_mult(dst, r_2, r_1);
}

void rotation_matrix(mat3 dst, const vec3 axis, const f32 angle)
{
	const f32 w = f32_cos(angle / 2.0f);
	vec3 pure_quat;
	vec3_scale(pure_quat, axis, f32_sin(angle / 2.0f));

	const f32 tr_part = 2.0f*w*w - 1.0f;
	const f32 q12 = 2.0f*pure_quat[0]*pure_quat[1];
	const f32 q13 = 2.0f*pure_quat[0]*pure_quat[2];
	const f32 q10 = 2.0f*pure_quat[0]*w;
	const f32 q23 = 2.0f*pure_quat[1]*pure_quat[2];
	const f32 q20 = 2.0f*pure_quat[1]*w;
	const f32 q30 = 2.0f*pure_quat[2]*w;
	mat3_set(dst, tr_part + 2.0f*pure_quat[0]*pure_quat[0], q12 + q30, q13 - q20,
		      q12 - q30, tr_part + 2.0f*pure_quat[1]*pure_quat[1], q23 + q10,
		      q13 + q20, q23 - q10, tr_part + 2.0f*pure_quat[2]*pure_quat[2]);
}

void vec3_rotate_center(vec3 src_rotated, mat3 rotation, const vec3 center, const vec3 src)
{
	vec3 tmp;
	vec3_sub(src_rotated, src, center);
	mat3_vec_mul(tmp, rotation, src_rotated);
	vec3_add(src_rotated, tmp, center);
}

void perspective_matrix(mat4 dst, const f32 aspect_ratio, const f32 fov_x, const f32 fz_near, const f32 fz_far)
{
	mat4_set(dst, 
		     1.0f / f32_tan(fov_x / 2.0f), 0.0f, 0.0f, 0.0f,
	             0.0f, aspect_ratio / f32_tan(fov_x / 2.0f), 0.0f, 0.0f,
		     0.0f, 0.0f, (fz_near + fz_far) / (fz_near - fz_far), -1.0f,
		     0.0f, 0.0f, (2.0f * fz_near * fz_far) / (fz_near - fz_far), 0.0f);
}

void view_matrix(mat4 dst, const vec3 position, const vec3 left, const vec3 up, const vec3 forward)
{
	/**
	 * (1) Translation to camera center 
	 * (2) Change to Camera basis
	 * (3) anything infront of camera must be reflected in x,z values againt (0,0), since
	 * Opengl expects camera looking down -Z axis, so mult left, and forward axes by (-1) .
	 */
	mat4 basis_change, translation;
	mat4_set(basis_change,
			-left[0], up[0], -forward[0], 0.0f,
			-left[1], up[1], -forward[1], 0.0f,
			-left[2], up[2], -forward[2], 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);
	mat4_set(translation,
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			-position[0], -position[1], -position[2], 1.0f);
	mat4_mult(dst, basis_change, translation);
}

void view_matrix_look_at(mat4 dst, const vec3 position, const vec3 target)
{
	vec3 tmp, relative, dir;
	vec3_sub(relative, target, position);
	vec3_normalize(dir, relative);
	vec3_set(tmp, 0.0f, 1.0f, 0.0f);
	const f32 pitch = MM_PI_F / 2.0f - f32_acos(vec3_dot(tmp, dir));

	relative[1] = 0.0f;
	vec3_normalize(dir, relative);
	vec3_set(tmp, 1.0f, 0.0f, 0.0f);

	f32 yaw;
	if (dir[2] < 0.0f) {
		yaw  = f32_acos(vec3_dot(tmp, dir));	
	} else {
		yaw  = -f32_acos(vec3_dot(tmp, dir));	
	}
	view_matrix_yaw_pitch(dst, position, yaw, pitch);
}

void view_matrix_yaw_pitch(mat4 dst, const vec3 position, const f32 yaw, const f32 pitch)
{
	vec3 left, up, forward, tmp;
	mat3 rot;
	quat q;
	const f32 cy = f32_cos(yaw / 2.0f);
	const f32 cp = f32_cos(pitch / 2.0f);
	const f32 sy = f32_sin(yaw / 2.0f);
	const f32 sp = f32_sin(pitch / 2.0f);
	quat_set(q, sy*sp, sy*cp, cy*sp, cy*cp);
	quat_to_mat3(rot, q);

	/* Assume no rotation is equivalent to looking down positive x-axis */
	vec3_set(tmp, 0.0f, 0.0f, -1.0f);
	mat3_vec_mul(left, rot, tmp);

	vec3_set(tmp, 0.0f, 1.0f, 0.0f);
	mat3_vec_mul(up, rot, tmp);

	vec3_set(tmp, 1.0f, 0.0f, 0.0f);
	mat3_vec_mul(forward, rot, tmp);

	mat4 basis_change, translation;
	mat4_set(basis_change,
			-left[0], up[0], -forward[0], 0.0f,
			-left[1], up[1], -forward[1], 0.0f,
			-left[2], up[2], -forward[2], 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);
	mat4_set(translation,
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			-position[0], -position[1], -position[2], 1.0f);
	mat4_mult(dst, basis_change, translation);
}
