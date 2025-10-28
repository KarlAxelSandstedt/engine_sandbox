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

#include <float.h>
#include "float32.h"
#include "collision.h"
#include "rigid_body.h"
#include "physics_pipeline.h"

/********************************** Contact Manifold helpers **********************************/

void contact_manifold_debug_print(FILE *file, const struct contact_manifold *cm)
{
	fprintf(stderr, "Contact Manifold:\n{\n");

	fprintf(stderr, "\t.i1 = %u\n", cm->i1);
	fprintf(stderr, "\t.i2 = %u\n", cm->i2);
	fprintf(stderr, "\t.v_count = %u\n", cm->v_count);
	for (u32 i = 0; i < cm->v_count; ++i)
	{
		fprintf(stderr, "\t.v[%u] = { %f, %f, %f }\n", i, cm->v[i][0], cm->v[i][1], cm->v[i][2]);
	}
	fprintf(stderr, "\t.n = { %f, %f, %f }\n", cm->n[0], cm->n[1], cm->n[2]);
	fprintf(stderr, "}\n");
}

/********************************** GJK INTERNALS **********************************/

/**
 * Gilbert-Johnson-Keerthi intersection algorithm in 3D. Based on the original paper. 
 *
 * For understanding, see [ Collision Detection in Interactive 3D environments, chapter 4.3.1 - 4.3.8 ]
 */
struct simplex
{
	vec3 p[4];
	u64 id[4];
	f32 dot[4];
	u32 type;
};

#define SIMPLEX_0	0
#define SIMPLEX_1	1
#define SIMPLEX_2	2
#define SIMPLEX_3	3

static struct simplex gjk_internal_simplex_init(void)
{
	struct simplex simplex = 
	{
		.id = {UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX},
		.dot = { -1.0f, -1.0f, -1.0f, -1.0f },
		.type = UINT32_MAX,
	};

	return simplex;
}

static u32 gjk_internal_johnsons_algorithm(struct simplex *simplex, vec3 c_v, vec4 lambda)
{
	vec3 a;

	if (simplex->type == 0)
	{
		vec3_copy(c_v, simplex->p[0]);
	}
	else if (simplex->type == 1)
	{
		vec3_sub(a, simplex->p[0], simplex->p[1]);
		const f32 delta_01_1 = vec3_dot(a, simplex->p[0]);

		if (delta_01_1 > 0.0f)
		{
			vec3_sub(a, simplex->p[1], simplex->p[0]);
			const f32 delta_01_0 = vec3_dot(a, simplex->p[1]);
			if (delta_01_0 > 0.0f)
			{
				const f32 delta = delta_01_0 + delta_01_1;
				lambda[0] = delta_01_0 / delta;
				lambda[1] = delta_01_1 / delta;
				vec3_set(c_v,
				       	(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[1])[0]),
				       	(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[1])[1]),
				       	(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[1])[2]));
			}
			else
			{
				simplex->type = 0;
				vec3_copy(c_v, simplex->p[1]);
				vec3_copy(simplex->p[0], simplex->p[1]);
			}
		}
		else
		{
			/* 
			 * numerical issues, new simplex should always contain newly added point
			 * of simplex, terminate next iteration. Let c_v stay the same as in the
			 * previous iteration.
			 */
			return 1;
		}
	}
	else if (simplex->type == 2)
	{
		vec3_sub(a, simplex->p[1], simplex->p[0]);
		const f32 delta_01_0 = vec3_dot(a, simplex->p[1]);
		vec3_sub(a, simplex->p[0], simplex->p[1]);
		const f32 delta_01_1 = vec3_dot(a, simplex->p[0]);
		vec3_sub(a, simplex->p[0], simplex->p[2]);
		const f32 delta_012_2 = delta_01_0 * vec3_dot(a, simplex->p[0]) + delta_01_1 * vec3_dot(a, simplex->p[1]);
		if (delta_012_2 > 0.0f)
		{
			vec3_sub(a, simplex->p[2], simplex->p[0]);
			const f32 delta_02_0 = vec3_dot(a, simplex->p[2]);
			vec3_sub(a, simplex->p[0], simplex->p[2]);
			const f32 delta_02_2 = vec3_dot(a, simplex->p[0]);
			vec3_sub(a, simplex->p[0], simplex->p[1]);
			const f32 delta_012_1 = delta_02_0 * vec3_dot(a, simplex->p[0]) + delta_02_2 * vec3_dot(a, simplex->p[2]);
			if (delta_012_1 > 0.0f)
			{
				vec3_sub(a, simplex->p[2], simplex->p[1]);
				const f32 delta_12_1 = vec3_dot(a, simplex->p[2]);
				vec3_sub(a, simplex->p[1], simplex->p[2]);
				const f32 delta_12_2 = vec3_dot(a, simplex->p[1]);
				vec3_sub(a, simplex->p[1], simplex->p[0]);
				const f32 delta_012_0 = delta_12_1 * vec3_dot(a, simplex->p[1]) + delta_12_2 * vec3_dot(a, simplex->p[2]);
				if (delta_012_0 > 0.0f)
				{
					const f32 delta = delta_012_0 + delta_012_1 + delta_012_2;
					lambda[0] = delta_012_0 / delta;
					lambda[1] = delta_012_1 / delta;
					lambda[2] = delta_012_2 / delta;
					vec3_set(c_v,
						(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[1])[0] + lambda[2]*(simplex->p[2])[0]),
						(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[1])[1] + lambda[2]*(simplex->p[2])[1]),
						(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[1])[2] + lambda[2]*(simplex->p[2])[2]));
				}
				else
				{
					if (delta_12_2 > 0.0f)
					{
						if (delta_12_1 > 0.0f)
						{
							const f32 delta = delta_12_1 + delta_12_2;
							lambda[0] = delta_12_1 / delta;
							lambda[1] = delta_12_2 / delta;
							vec3_set(c_v,
							       	(lambda[0]*(simplex->p[1])[0] + lambda[1]*(simplex->p[2])[0]),
							       	(lambda[0]*(simplex->p[1])[1] + lambda[1]*(simplex->p[2])[1]),
							       	(lambda[0]*(simplex->p[1])[2] + lambda[1]*(simplex->p[2])[2]));
							simplex->type = 1;
							vec3_copy(simplex->p[0], simplex->p[1]);
							vec3_copy(simplex->p[1], simplex->p[2]);
							simplex->id[0] = simplex->id[1];
							simplex->dot[0] = simplex->dot[1];
						}
						else
						{
							simplex->type = 0;
							vec3_copy(c_v, simplex->p[2]);
							vec3_copy(simplex->p[0], simplex->p[2]);
							simplex->id[1] = UINT32_MAX;
							simplex->dot[1] = -1.0f;
						}


					}
					else
					{
						return 1;
					}
				}

			}
			else
			{
				if (delta_02_2 > 0.0f)
				{
					if (delta_02_0 > 0.0f)
					{
						const f32 delta = delta_02_0 + delta_02_2;
						lambda[0] = delta_02_0 / delta;
						lambda[1] = delta_02_2 / delta;
						vec3_set(c_v,
						       	(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[2])[0]),
						       	(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[2])[1]),
						       	(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[2])[2]));
						simplex->type = 1;
						vec3_copy(simplex->p[1], simplex->p[2]);
					}
					else
					{
						simplex->type = 0;
						vec3_copy(c_v, simplex->p[2]);
						vec3_copy(simplex->p[0], simplex->p[2]);
						simplex->id[1] = UINT32_MAX;
						simplex->dot[1] = -1.0f;
					}
				}
			}
		}
		else
		{
			return 1;
		}
	}
	else
	{
		vec3_sub(a, simplex->p[1], simplex->p[0]);
		const f32 delta_01_0 = vec3_dot(a, simplex->p[1]);
		vec3_sub(a, simplex->p[0], simplex->p[1]);
		const f32 delta_01_1 = vec3_dot(a, simplex->p[0]);
		vec3_sub(a, simplex->p[0], simplex->p[2]);
		const f32 delta_012_2 = delta_01_0 * vec3_dot(a, simplex->p[0]) + delta_01_1 * vec3_dot(a, simplex->p[1]);

		vec3_sub(a, simplex->p[2], simplex->p[0]);
		const f32 delta_02_0 = vec3_dot(a, simplex->p[2]);
		vec3_sub(a, simplex->p[0], simplex->p[2]);
		const f32 delta_02_2 = vec3_dot(a, simplex->p[0]);
		vec3_sub(a, simplex->p[0], simplex->p[1]);
		const f32 delta_012_1 = delta_02_0 * vec3_dot(a, simplex->p[0]) + delta_02_2 * vec3_dot(a, simplex->p[2]);

		vec3_sub(a, simplex->p[2], simplex->p[1]);
		const f32 delta_12_1 = vec3_dot(a, simplex->p[2]);
		vec3_sub(a, simplex->p[1], simplex->p[2]);
		const f32 delta_12_2 = vec3_dot(a, simplex->p[1]);
		vec3_sub(a, simplex->p[1], simplex->p[0]);
		const f32 delta_012_0 = delta_12_1 * vec3_dot(a, simplex->p[1]) + delta_12_2 * vec3_dot(a, simplex->p[2]);

		vec3_sub(a, simplex->p[0], simplex->p[3]);
		const f32 delta_0123_3 = delta_012_0 * vec3_dot(a, simplex->p[0]) + delta_012_1 * vec3_dot(a, simplex->p[1]) + delta_012_2 * vec3_dot(a, simplex->p[2]);

		if (delta_0123_3 > 0.0f)
		{
			vec3_sub(a, simplex->p[0], simplex->p[3]);
			const f32 delta_013_3 = delta_01_0 * vec3_dot(a, simplex->p[0]) + delta_01_1 * vec3_dot(a, simplex->p[1]);

			vec3_sub(a, simplex->p[3], simplex->p[0]);
			const f32 delta_03_0 = vec3_dot(a, simplex->p[3]);
			vec3_sub(a, simplex->p[0], simplex->p[3]);
			const f32 delta_03_3 = vec3_dot(a, simplex->p[0]);
			vec3_sub(a, simplex->p[0], simplex->p[1]);
			const f32 delta_013_1 = delta_03_0 * vec3_dot(a, simplex->p[0]) + delta_03_3 * vec3_dot(a, simplex->p[3]);

			vec3_sub(a, simplex->p[3], simplex->p[1]);
			const f32 delta_13_1 = vec3_dot(a, simplex->p[3]);
			vec3_sub(a, simplex->p[1], simplex->p[3]);
			const f32 delta_13_3 = vec3_dot(a, simplex->p[1]);
			vec3_sub(a, simplex->p[1], simplex->p[0]);
			const f32 delta_013_0 = delta_13_1 * vec3_dot(a, simplex->p[1]) + delta_13_3 * vec3_dot(a, simplex->p[3]);

			vec3_sub(a, simplex->p[0], simplex->p[2]);
			const f32 delta_0123_2 = delta_013_0 * vec3_dot(a, simplex->p[0]) + delta_013_1 * vec3_dot(a, simplex->p[1]) + delta_013_3 * vec3_dot(a, simplex->p[3]);

			if (delta_0123_2 > 0.0f)
			{
				vec3_sub(a, simplex->p[0], simplex->p[3]);
				const f32 delta_023_3 = delta_02_0 * vec3_dot(a, simplex->p[0]) + delta_02_2 * vec3_dot(a, simplex->p[2]);

				vec3_sub(a, simplex->p[0], simplex->p[2]);
				const f32 delta_023_2 = delta_03_0 * vec3_dot(a, simplex->p[0]) + delta_03_3 * vec3_dot(a, simplex->p[3]);

				vec3_sub(a, simplex->p[3], simplex->p[2]);
				const f32 delta_23_2 = vec3_dot(a, simplex->p[3]);
				vec3_sub(a, simplex->p[2], simplex->p[3]);
				const f32 delta_23_3 = vec3_dot(a, simplex->p[2]);
				vec3_sub(a, simplex->p[2], simplex->p[0]);
				const f32 delta_023_0 = delta_23_2 * vec3_dot(a, simplex->p[2]) + delta_23_3 * vec3_dot(a, simplex->p[3]);

				vec3_sub(a, simplex->p[0], simplex->p[1]);
				const f32 delta_0123_1 = delta_023_0 * vec3_dot(a, simplex->p[0]) + delta_023_2 * vec3_dot(a, simplex->p[2]) + delta_023_3 * vec3_dot(a, simplex->p[3]);

				if (delta_0123_1 > 0.0f)
				{
					vec3_sub(a, simplex->p[3], simplex->p[1]);
					const f32 delta_123_1 = delta_23_2 * vec3_dot(a, simplex->p[2]) + delta_23_3 * vec3_dot(a, simplex->p[3]);

					vec3_sub(a, simplex->p[3], simplex->p[2]);
					const f32 delta_123_2 = delta_13_1 * vec3_dot(a, simplex->p[1]) + delta_13_3 * vec3_dot(a, simplex->p[3]);

					vec3_sub(a, simplex->p[1], simplex->p[3]);
					const f32 delta_123_3 = delta_12_1 * vec3_dot(a, simplex->p[1]) + delta_12_2 * vec3_dot(a, simplex->p[2]);

					vec3_sub(a, simplex->p[3], simplex->p[0]);
					const f32 delta_0123_0 = delta_123_1 * vec3_dot(a, simplex->p[1]) + delta_123_2 * vec3_dot(a, simplex->p[2]) + delta_123_3 * vec3_dot(a, simplex->p[3]);

					if (delta_0123_0 > 0.0f)
					{
						/* intersection */
						const f32 delta = delta_0123_0 + delta_0123_1 + delta_0123_2 + delta_0123_3;
						lambda[0] = delta_0123_0 / delta;
						lambda[1] = delta_0123_1 / delta;
						lambda[2] = delta_0123_2 / delta;
						lambda[3] = delta_0123_3 / delta;
						vec3_set(c_v,
							(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[1])[0] + lambda[2]*(simplex->p[2])[0] + lambda[3]*(simplex->p[3])[0]),
							(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[1])[1] + lambda[2]*(simplex->p[2])[1] + lambda[3]*(simplex->p[3])[1]),
							(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[1])[2] + lambda[2]*(simplex->p[2])[2] + lambda[3]*(simplex->p[3])[2]));
					}
					else
					{
						/* check 123 subset */
						if (delta_123_3 > 0.0f)
						{
							if (delta_123_2 > 0.0f)
							{
								if (delta_123_1 > 0.0f)
								{
									const f32 delta = delta_123_1 + delta_123_2 + delta_123_3;
									lambda[0] = delta_123_1 / delta;
									lambda[1] = delta_123_2 / delta;
									lambda[2] = delta_123_3 / delta;
									vec3_set(c_v,
										(lambda[0]*(simplex->p[1])[0] + lambda[1]*(simplex->p[2])[0] + lambda[2]*(simplex->p[3])[0]),
										(lambda[0]*(simplex->p[1])[1] + lambda[1]*(simplex->p[2])[1] + lambda[2]*(simplex->p[3])[1]),
										(lambda[0]*(simplex->p[1])[2] + lambda[1]*(simplex->p[2])[2] + lambda[2]*(simplex->p[3])[2]));
									simplex->type = 2;
									vec3_copy(simplex->p[0], simplex->p[1]);		
									vec3_copy(simplex->p[1], simplex->p[2]);		
									vec3_copy(simplex->p[2], simplex->p[3]);		
									simplex->dot[0] = simplex->dot[1];
									simplex->dot[1] = simplex->dot[2];
									simplex->id[0] = simplex->id[1];
									simplex->id[1] = simplex->id[2];
								}
								else
								{
									/* check 23 */
									if (delta_23_3 > 0.0f)
									{
										if (delta_23_2 > 0.0f)
										{
											const f32 delta = delta_23_2 + delta_23_3;
											lambda[0] = delta_23_2 / delta;
											lambda[1] = delta_23_3 / delta;
											vec3_set(c_v,
												(lambda[0]*(simplex->p[2])[0] + lambda[1]*(simplex->p[3])[0]),
												(lambda[0]*(simplex->p[2])[1] + lambda[1]*(simplex->p[3])[1]),
												(lambda[0]*(simplex->p[2])[2] + lambda[1]*(simplex->p[3])[2]));
											simplex->type = 1;
											vec3_copy(simplex->p[0], simplex->p[2]);		
											vec3_copy(simplex->p[1], simplex->p[3]);		
											simplex->dot[0] = simplex->dot[2];
											simplex->dot[2] = -1.0f;
											simplex->id[0] = simplex->id[2];
											simplex->id[2] = UINT32_MAX;
										}
										else
										{
											vec3_copy(c_v, simplex->p[3]);
											simplex->type = 0;
											vec3_copy(simplex->p[0], simplex->p[3]);
											simplex->dot[1] = -1.0f;
											simplex->dot[2] = -1.0f;
											simplex->id[1] = UINT32_MAX;
											simplex->id[2] = UINT32_MAX;
										}
									}
									else
									{
										return 1;
									}
								}
							}
							else
							{
								/* check 13 subset */
								if (delta_13_3 > 0.0f)
								{
									if (delta_13_1 > 0.0f)
									{
										const f32 delta = delta_13_1 + delta_13_3;
										lambda[0] = delta_13_1 / delta;
										lambda[1] = delta_13_3 / delta;
										vec3_set(c_v,
											(lambda[0]*(simplex->p[1])[0] + lambda[1]*(simplex->p[3])[0]),
											(lambda[0]*(simplex->p[1])[1] + lambda[1]*(simplex->p[3])[1]),
											(lambda[0]*(simplex->p[1])[2] + lambda[1]*(simplex->p[3])[2]));
										simplex->type = 1;
										vec3_copy(simplex->p[0], simplex->p[1]);
										vec3_copy(simplex->p[1], simplex->p[3]);		
										simplex->dot[0] = simplex->dot[1];
										simplex->dot[2] = -1.0f;
										simplex->id[0] = simplex->id[1];
										simplex->id[2] = UINT32_MAX;
									}
									else
									{
										vec3_copy(c_v, simplex->p[3]);
										simplex->type = 0;
										vec3_copy(simplex->p[0], simplex->p[3]);
										simplex->dot[1] = -1.0f;
										simplex->dot[2] = -1.0f;
										simplex->id[1] = UINT32_MAX;
										simplex->id[2] = UINT32_MAX;
									}
								}
								else
								{
									return 1;
								}
							}	
						}
						else
						{
							return 1;
						}
					}
				}
				else
				{
					/* check 023 subset */
					if (delta_023_3 > 0.0f)
					{
						if (delta_023_2 > 0.0f)
						{
							if (delta_023_0 > 0.0f)
							{
								const f32 delta = delta_023_0 + delta_023_2 + delta_023_3;
								lambda[0] = delta_023_0 / delta;
								lambda[1] = delta_023_2 / delta;
								lambda[2] = delta_023_3 / delta;
								vec3_set(c_v,
									(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[2])[0] + lambda[2]*(simplex->p[3])[0]),
									(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[2])[1] + lambda[2]*(simplex->p[3])[1]),
									(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[2])[2] + lambda[2]*(simplex->p[3])[2]));
								simplex->type = 2;
								vec3_copy(simplex->p[1], simplex->p[2]);		
								vec3_copy(simplex->p[2], simplex->p[3]);		
								simplex->dot[1] = simplex->dot[2];
								simplex->id[1] = simplex->id[2];
							}
							else
							{
								/* check 23 subset */
								if (delta_23_3 > 0.0f)
								{
									if (delta_23_2 > 0.0f)
									{
										const f32 delta = delta_23_2 + delta_23_3;
										lambda[0] = delta_23_2 / delta;
										lambda[1] = delta_23_3 / delta;
										vec3_set(c_v,
											(lambda[0]*(simplex->p[2])[0] + lambda[1]*(simplex->p[3])[0]),
											(lambda[0]*(simplex->p[2])[1] + lambda[1]*(simplex->p[3])[1]),
											(lambda[0]*(simplex->p[2])[2] + lambda[1]*(simplex->p[3])[2]));
										simplex->type = 1;
										vec3_copy(simplex->p[0], simplex->p[2]);
										vec3_copy(simplex->p[1], simplex->p[3]);
										simplex->dot[0] = simplex->dot[2];
										simplex->dot[2] = -1.0f;
										simplex->id[0] = simplex->id[2];
										simplex->id[2] = UINT32_MAX;
									}
									else
									{
										vec3_copy(c_v, simplex->p[3]);
										simplex->type = 0;
										vec3_copy(simplex->p[0], simplex->p[3]);
										simplex->dot[1] = -1.0f;
										simplex->dot[2] = -1.0f;
										simplex->id[1] = UINT32_MAX;
										simplex->id[2] = UINT32_MAX;
									}
								}
								else
								{
									return 1;
								}
							}
						}
						else
						{
							/* check 03 subset */
							if (delta_03_3 > 0.0f)
							{
								if (delta_03_0 > 0.0f)
								{
									const f32 delta = delta_03_0 + delta_03_3;
									lambda[0] = delta_03_0 / delta;
									lambda[1] = delta_03_3 / delta;
									vec3_set(c_v,
										(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[3])[0]),
										(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[3])[1]),
										(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[3])[2]));
									simplex->type = 1;
									vec3_copy(simplex->p[1], simplex->p[3]);
									simplex->dot[2] = -1.0f;
									simplex->id[2] = UINT32_MAX;
								}
								else
								{
									vec3_copy(c_v, simplex->p[3]);
									simplex->type = 0;
									vec3_copy(simplex->p[0], simplex->p[3]);
									simplex->dot[1] = -1.0f;
									simplex->dot[2] = -1.0f;
									simplex->id[1] = UINT32_MAX;
									simplex->id[2] = UINT32_MAX;
								}
							}
							else
							{
								return 1;
							}
						}
					}
					else
					{
						return 1;
					}
				}
			}
			else
			{
				/* check 013 subset */
				if (delta_013_3 > 0.0f)
				{
					if (delta_013_1 > 0.0f)
					{
						if (delta_013_0 > 0.0f)
						{
							const f32 delta = delta_013_0 + delta_013_1 + delta_013_3;
							lambda[0] = delta_013_0 / delta;
							lambda[1] = delta_013_1 / delta;
							lambda[2] = delta_013_3 / delta;
							vec3_set(c_v,
								(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[1])[0] + lambda[2]*(simplex->p[3])[0]),
								(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[1])[1] + lambda[2]*(simplex->p[3])[1]),
								(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[1])[2] + lambda[2]*(simplex->p[3])[2]));
							simplex->type = 2;
							vec3_copy(simplex->p[2], simplex->p[3]);
						}
						else
						{
							/* check 13 subset */
							if (delta_13_3 > 0.0f)
							{
								if (delta_13_1 > 0.0f)
								{
									const f32 delta = delta_13_1 + delta_13_3;
									lambda[0] = delta_13_1 / delta;
									lambda[1] = delta_13_3 / delta;
									vec3_set(c_v,
										(lambda[0]*(simplex->p[1])[0] + lambda[1]*(simplex->p[3])[0]),
										(lambda[0]*(simplex->p[1])[1] + lambda[1]*(simplex->p[3])[1]),
										(lambda[0]*(simplex->p[1])[2] + lambda[1]*(simplex->p[3])[2]));
									simplex->type = 1;
									vec3_copy(simplex->p[0], simplex->p[1]);
									vec3_copy(simplex->p[1], simplex->p[3]);
									simplex->dot[2] = -1.0f;
									simplex->id[2] = UINT32_MAX;
								}
								else
								{
									vec3_copy(c_v, simplex->p[3]);
									simplex->type = 0;
									vec3_copy(simplex->p[0], simplex->p[3]);
									simplex->dot[1] = -1.0f;
									simplex->dot[2] = -1.0f;
									simplex->id[1] = UINT32_MAX;
									simplex->id[2] = UINT32_MAX;
								}
							}
							else
							{
								return 1;
							}
						}	
					}
					else
					{
						/* check 03 subset */
						if (delta_03_3 > 0.0f)
						{
							if (delta_03_0 > 0.0f)
							{
								const f32 delta = delta_03_0 + delta_03_3;
								lambda[0] = delta_03_0 / delta;
								lambda[1] = delta_03_3 / delta;
								vec3_set(c_v,
									(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[3])[0]),
									(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[3])[1]),
									(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[3])[2]));
								simplex->type = 1;
								vec3_copy(simplex->p[1], simplex->p[3]);
								simplex->dot[2] = -1.0f;
								simplex->id[2] = UINT32_MAX;
							}
							else
							{
								vec3_copy(c_v, simplex->p[3]);
								simplex->type = 0;
								vec3_copy(simplex->p[0], simplex->p[3]);
								simplex->dot[1] = -1.0f;
								simplex->dot[2] = -1.0f;
								simplex->id[1] = UINT32_MAX;
								simplex->id[2] = UINT32_MAX;
							}
						}
						else
						{
							return 1;
						}
					}
				}
				else
				{
					return 1;
				}
			}
		}
		else
		{
			return 1;
		}
	}

	return 0;
}

struct gjk_input
{
	vec3ptr v;
	vec3 pos;
	mat3 rot;
	u32 v_count;
};

static void gjk_internal_closest_points(vec3 c1, vec3 c2, struct gjk_input *in1, struct simplex *simplex, const vec4 lambda)
{
	if (simplex->type == 0)
	{
		mat3_vec_mul(c1, in1->rot, in1->v[simplex->id[0] >> 32]);
		vec3_translate(c1, in1->pos);
		vec3_sub(c2, c1, simplex->p[0]);
	}
	else
	{
		vec3 tmp1, tmp2;
		vec3_set(c1, 0.0f, 0.0f, 0.0f);
		vec3_set(c2, 0.0f, 0.0f, 0.0f);
		for (u32 i = 0; i <= simplex->type; ++i)
		{
			mat3_vec_mul(tmp1, in1->rot, in1->v[simplex->id[i] >> 32]);
			vec3_translate(tmp1, in1->pos);
			vec3_sub(tmp2, tmp1, simplex->p[i]);
			vec3_translate_scaled(c1, tmp1, lambda[i]);
			vec3_translate_scaled(c2, tmp2, lambda[i]);
		}
	}

}	

static u32 gjk_internal_support(vec3 support, const vec3 dir, struct gjk_input *in)
{
	f32 max = -FLT_MAX;
	u32 max_index = 0;
	vec3 p;
	for (u32 i = 0; i < in->v_count; ++i)
	{
		mat3_vec_mul(p, in->rot, in->v[i]);
		const f32 dot = vec3_dot(p, dir);
		if (max < dot)
		{
			max_index = i;
			max = dot; 
		}
	}

	mat3_vec_mul(support, in->rot, in->v[max_index]);
	vec3_translate(support,in->pos);
	return max_index;

}

static f32 gjk_distance_sq(vec3 c1, vec3 c2, struct gjk_input *in1, struct gjk_input *in2)
{
	assert(in1->v_count > 0);
	assert(in2->v_count > 0);
	
	const f32 abs_tol = 100.0f * F32_EPSILON;
	const f32 tol = 100.0f * F32_EPSILON;

	struct simplex simplex = gjk_internal_simplex_init();
	vec3 dir, c_v, tmp, s1, s2;
	vec4 lambda;
	u64 support_id;
	f32 ma; /* max dot product of current simplex */
	f32 dist_sq = FLT_MAX; 
	const f32 rel = tol * tol;

	/* arbitrary starting search direction */
	vec3_set(c_v, 1.0f, 0.0f, 0.0f);
	u64 old_support = UINT64_MAX;

	//TODO
	const u32 max_iter = 128;
	for (u32 i = 0; i < max_iter; ++i)
	{
		simplex.type += 1;
		vec3_scale(dir, c_v, -1.0f);

		const u32 i1 = gjk_internal_support(s1, dir, in1);
		vec3_negative_to(tmp, dir);
		const u32 i2 = gjk_internal_support(s2, tmp, in2);
		vec3_sub(simplex.p[simplex.type], s1, s2);
		support_id = ((u64) i1 << 32) | (u64) i2;

		if (dist_sq - vec3_dot(simplex.p[simplex.type], c_v) <= rel * dist_sq + abs_tol
				|| simplex.id[0] == support_id || simplex.id[1] == support_id 
				|| simplex.id[2] == support_id || simplex.id[3] == support_id)
		{
			assert(simplex.id != 0);
			assert(dist_sq != FLT_MAX);
			simplex.type -= 1;
			gjk_internal_closest_points(c1, c2, in1, &simplex, lambda);
			return dist_sq;
		}

		/* find closest point v to origin using naive Johnson's algorithm, update simplex data 
		 * Degenerate Case: due to numerical issues, determinant signs may flip, which may result
		 * either in wrong sub-simplex being chosen, or no valid simplex at all. In that case c_v
		 * stays the same, and we terminate the algorithm. [See page 142].
		 */
		if (gjk_internal_johnsons_algorithm(&simplex, c_v, lambda))
		{
			assert(dist_sq != FLT_MAX);
			simplex.type -= 1;
			gjk_internal_closest_points(c1, c2, in1, &simplex, lambda);
			return dist_sq;
		}

		simplex.id[simplex.type] = support_id;
		simplex.dot[simplex.type] = vec3_dot(simplex.p[simplex.type], simplex.p[simplex.type]);

		/* 
		 * If the simplex is of type 3, or a tetrahedron, we have encapsulated 0, or, if v is sufficiently
		 * close to the origin, within a margin of error, return an intersection.
		 */
		if (simplex.type == 3)
		{
			return 0.0f;
		}
		else
		{
			ma = simplex.dot[0];
			ma = f32_max(ma, simplex.dot[1]);
			ma = f32_max(ma, simplex.dot[2]);
			ma = f32_max(ma, simplex.dot[3]);

			/* For error bound discussion, see sections 4.3.5, 4.3.6 */
			dist_sq = vec3_dot(c_v, c_v);
			if (dist_sq <= abs_tol * ma)
			{
				return 0.0f;
			}
		}
	}

	return 0.0f;
}

/********************************** DISTANCE METHODS **********************************/

static f32 sphere_distance(vec3 c1, vec3 c2, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert(b1->shape_type == COLLISION_SHAPE_SPHERE && b2->shape_type == COLLISION_SHAPE_SPHERE);

	const struct collision_shape *shape1 = string_database_address(pipeline->shape_db, b1->shape_handle);
	const struct collision_shape *shape2 = string_database_address(pipeline->shape_db, b2->shape_handle);

	f32 dist_sq = 0.0f;

	const f32 r_sum = shape1->sphere.radius + shape2->sphere.radius + 2.0f * margin;
	if (vec3_distance_squared(b1->position, b2->position) > r_sum*r_sum)
	{
		vec3 dir;
		vec3_sub(dir, b2->position, b1->position);
		vec3_mul_constant(dir, 1.0f/vec3_length(dir));
		vec3_copy(c1, b1->position);
		vec3_copy(c2, b2->position);
		vec3_translate_scaled(c1, dir,  shape1->sphere.radius);
		vec3_translate_scaled(c2, dir, -shape2->sphere.radius);
		dist_sq = vec3_distance_squared(c1, c2);
	}

	return f32_sqrt(dist_sq);
}

static f32 capsule_sphere_distance(vec3 c1, vec3 c2, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert(b1->shape_type == COLLISION_SHAPE_CAPSULE && b2->shape_type == COLLISION_SHAPE_SPHERE);

	const struct collision_shape *shape2 = string_database_address(pipeline->shape_db, b2->shape_handle);

	const struct collision_capsule *cap = &((struct collision_shape *) string_database_address(pipeline->shape_db, b1->shape_handle))->capsule;
	f32 r_sum = cap->radius + shape2->sphere.radius + 2.0f * margin;

	mat3 rot;
	quat_to_mat3(rot, b1->rotation);

	vec3 s_p1, s_p2, diff;
	vec3_sub(c2, b2->position, b1->position);
	mat3_vec_mul(s_p1, rot, cap->p1);
	vec3_negative_to(s_p2, s_p1);
	struct segment s = segment_construct(s_p1, s_p2);

	f32 dist = 0.0f;
	if (segment_point_distance_sq(c1, &s, c2) > r_sum*r_sum)
	{
		vec3_translate(c1, b1->position);
		vec3_translate(c2, b1->position);
		vec3_sub(diff, c2, c1);
		vec3_mul_constant(diff, 1.0f / vec3_length(diff));
		vec3_translate_scaled(c1, diff, cap->radius);
		vec3_translate_scaled(c2, diff, -shape2->sphere.radius);

		dist = f32_sqrt(vec3_distance_squared(c1, c2));
	}

	return dist;
}

static f32 capsule_distance(vec3 c1, vec3 c2, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert(b1->shape_type == COLLISION_SHAPE_CAPSULE && b2->shape_type == COLLISION_SHAPE_CAPSULE);


	struct collision_capsule *cap1 = &((struct collision_shape *) string_database_address(pipeline->shape_db, b1->shape_handle))->capsule;
	struct collision_capsule *cap2 = &((struct collision_shape *) string_database_address(pipeline->shape_db, b2->shape_handle))->capsule;
	f32 r_sum = cap1->radius + cap2->radius + 2.0f * margin;

	mat3 rot;
	vec3 p0, p1; /* line points */

	quat_to_mat3(rot, b1->rotation);
	mat3_vec_mul(p0, rot, cap1->p1);
	vec3_negative_to(p1, p0);
	vec3_translate(p0, b1->position);
	vec3_translate(p1, b1->position);
	struct segment s1 = segment_construct(p0, p1);
	
	quat_to_mat3(rot, b2->rotation);
	mat3_vec_mul(p0, rot, cap2->p1);
	vec3_negative_to(p1, p0);
	vec3_translate(p0, b2->position);
	vec3_translate(p1, b2->position);
	struct segment s2 = segment_construct(p0, p1);

	f32 dist = 0.0f;
	if (segment_distance_sq(c1, c2, &s1, &s2) > r_sum*r_sum)
	{
		vec3_sub(p0, c2, c1);
		vec3_normalize(p1, p0);
		vec3_translate_scaled(c1, p1, cap1->radius);
		vec3_translate_scaled(c2, p1, -cap2->radius);
		dist = f32_sqrt(vec3_distance_squared(c1, c2));
	}

	return dist;
}

static f32 hull_sphere_distance(vec3 c1, vec3 c2, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert (b1->shape_type == COLLISION_SHAPE_CONVEX_HULL);
	assert (b2->shape_type == COLLISION_SHAPE_SPHERE);

	const struct collision_shape *shape1 = string_database_address(pipeline->shape_db, b1->shape_handle);
	const struct collision_shape *shape2 = string_database_address(pipeline->shape_db, b2->shape_handle);

	struct gjk_input g1 = { .v = shape1->hull.v, .v_count = shape1->hull.v_count, };
	vec3_copy(g1.pos, b1->position);
	quat_to_mat3(g1.rot, b1->rotation);

	vec3 n = VEC3_ZERO;
	struct gjk_input g2 = { .v = &n, .v_count = 1, };
	vec3_copy(g2.pos, b2->position);
	mat3_identity(g2.rot);

	f32 dist_sq = gjk_distance_sq(c1, c2, &g1, &g2);
	const f32 r_sum = shape2->sphere.radius + 2.0f * margin;

	if (dist_sq <= r_sum*r_sum)
	{
		dist_sq = 0.0f;
	}
	else
	{
		vec3_sub(n, c2, c1);
		vec3_mul_constant(n, 1.0f / vec3_length(n));
		vec3_translate_scaled(c1, n, margin);
		vec3_translate_scaled(c2, n, -(shape2->sphere.radius + margin));
	}

	return f32_sqrt(dist_sq);
}

static f32 hull_capsule_distance(vec3 c1, vec3 c2, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert (b1->shape_type == COLLISION_SHAPE_CONVEX_HULL);
	assert (b2->shape_type == COLLISION_SHAPE_CAPSULE);

	const struct collision_shape *shape1 = string_database_address(pipeline->shape_db, b1->shape_handle);
	const struct collision_shape *shape2 = string_database_address(pipeline->shape_db, b2->shape_handle);

	struct gjk_input g1 = { .v = shape1->hull.v, .v_count = shape1->hull.v_count, };
	vec3_copy(g1.pos, b1->position);
	quat_to_mat3(g1.rot, b1->rotation);

	vec3 segment[2];
	vec3_copy(segment[0], shape2->capsule.p1);
	vec3_negative_to(segment[1], segment[0]);
	struct gjk_input g2 = { .v = segment, .v_count = 2, };
	vec3_copy(g2.pos, b2->position);
	mat3_identity(g2.rot);

	f32 dist_sq = gjk_distance_sq(c1, c2, &g1, &g2);
	const f32 r_sum = shape2->capsule.radius + 2.0f * margin;

	if (dist_sq <= r_sum*r_sum)
	{
		dist_sq = 0.0f;
	}
	else
	{
		vec3 n;
		vec3_sub(n, c2, c1);
		vec3_mul_constant(n, 1.0f / vec3_length(n));
		vec3_translate_scaled(c1, n, margin);
		vec3_translate_scaled(c2, n, -(shape2->sphere.radius + margin));
	}

	return f32_sqrt(dist_sq);

}

static f32 hull_distance(vec3 c1, vec3 c2, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert (b1->shape_type == COLLISION_SHAPE_CONVEX_HULL);
	assert (b2->shape_type == COLLISION_SHAPE_CONVEX_HULL);

	const struct collision_shape *shape1 = string_database_address(pipeline->shape_db, b1->shape_handle);
	const struct collision_shape *shape2 = string_database_address(pipeline->shape_db, b2->shape_handle);

	struct gjk_input g1 = { .v = shape1->hull.v, .v_count = shape1->hull.v_count, };
	vec3_copy(g1.pos, b1->position);
	quat_to_mat3(g1.rot, b1->rotation);

	struct gjk_input g2 = { .v = shape2->hull.v, .v_count = shape2->hull.v_count, };
	vec3_copy(g2.pos, b2->position);
	quat_to_mat3(g2.rot, b2->rotation);

	f32 dist_sq = gjk_distance_sq(c1, c2, &g1, &g2);
	if (dist_sq <= 4.0f*margin*margin)
	{
		dist_sq = 0.0f;
		vec3 n;
		vec3_sub(n, c2, c1);
		vec3_mul_constant(n, 1.0f / vec3_length(n));
		vec3_translate_scaled(c1, n, margin);
		vec3_translate_scaled(c2, n, margin);
	}

	return f32_sqrt(dist_sq);
}

/********************************** INTERSECTION TESTS **********************************/

static u32 sphere_test(const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert(b1->shape_type == COLLISION_SHAPE_SPHERE && b2->shape_type == COLLISION_SHAPE_SPHERE);

	const struct collision_shape *shape1 = string_database_address(pipeline->shape_db, b1->shape_handle);
	const struct collision_shape *shape2 = string_database_address(pipeline->shape_db, b2->shape_handle);
	
	const f32 r_sum = shape1->sphere.radius + shape2->sphere.radius + 2.0f * margin;
	return vec3_distance_squared(b1->position, b2->position) <= r_sum*r_sum;
}

static u32 capsule_sphere_test(const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert(b1->shape_type == COLLISION_SHAPE_CAPSULE && b2->shape_type == COLLISION_SHAPE_SPHERE);

	const struct collision_shape *shape1 = string_database_address(pipeline->shape_db, b1->shape_handle);
	const struct collision_shape *shape2 = string_database_address(pipeline->shape_db, b2->shape_handle);

	const struct collision_capsule *cap = &shape1->capsule;
	f32 r_sum = cap->radius + shape2->sphere.radius + 2.0f * margin;

	mat3 rot;
	quat_to_mat3(rot, b1->rotation);

	vec3 c1, c2, s_p1, s_p2;
	vec3_sub(c2, b2->position, b1->position);
	mat3_vec_mul(s_p1, rot, cap->p1);
	vec3_negative_to(s_p2, s_p1);
	struct segment s = segment_construct(s_p1, s_p2);

	return segment_point_distance_sq(c1, &s, c2) <= r_sum*r_sum;
}

static u32 capsule_test(const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	vec3 c1, c2;
	return capsule_distance(c1, c2, pipeline, b1, b2, margin) == 0.0f;
}

static u32 hull_sphere_test(const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	vec3 c1, c2;
	return hull_sphere_distance(c1, c2, pipeline, b1, b2, margin) == 0.0f;
}

static u32 hull_capsule_test(const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	vec3 c1, c2;
	return hull_capsule_distance(c1, c2, pipeline, b1, b2, margin) == 0.0f;
}

static u32 hull_test(const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	vec3 c1, c2;
	return hull_distance(c1, c2, pipeline, b1, b2, margin) == 0.0f;
}

/********************************** CONTACT MANIFOLD METHODS **********************************/

static u32 sphere_contact(struct arena *garbage, struct contact_manifold *cm, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert(b1->shape_type == COLLISION_SHAPE_SPHERE);
	assert(b2->shape_type == COLLISION_SHAPE_SPHERE);

	const struct collision_shape *shape1 = string_database_address(pipeline->shape_db, b1->shape_handle);
	const struct collision_shape *shape2 = string_database_address(pipeline->shape_db, b2->shape_handle);

	u32 contact_generated = 0;

	const f32 r_sum = shape1->sphere.radius + shape2->sphere.radius + 2.0f * margin;
	const f32 dist_sq = vec3_distance_squared(b1->position, b2->position);
	if (dist_sq <= r_sum*r_sum)
	{
		contact_generated = 1;
		cm->v_count = 1;
		if (dist_sq <= COLLISION_POINT_DIST_SQ)
		{
			//TODO(Degenerate): spheres have same center => normal returned should depend on the context.
			vec3_set(cm->n, 0.0f, 1.0f, 0.0f);
		}
		else
		{
			vec3_sub(cm->n, b2->position, b1->position);
			vec3_mul_constant(cm->n, 1.0f/vec3_length(cm->n));
		}

		vec3 c1, c2;
		vec3_copy(c1, b1->position);
		vec3_copy(c2, b2->position);
		vec3_translate_scaled(c1, cm->n, shape1->sphere.radius + margin);
		vec3_translate_scaled(c2, cm->n, -(shape2->sphere.radius + margin));
		cm->depth[0] = vec3_dot(c1, cm->n) - vec3_dot(c2, cm->n);
		vec3_interpolate(cm->v[0], c1, c2, 0.5f);
	}

	return contact_generated;
}

static u32 capsule_sphere_contact(struct arena *garbage, struct contact_manifold *cm, const struct physics_pipeline *pipeline,  const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert(b1->shape_type == COLLISION_SHAPE_CAPSULE);
	assert(b2->shape_type == COLLISION_SHAPE_SPHERE);

	const struct collision_shape *shape1 = string_database_address(pipeline->shape_db, b1->shape_handle);
	const struct collision_shape *shape2 = string_database_address(pipeline->shape_db, b2->shape_handle);

	u32 contact_generated = 0;

	const struct collision_capsule *cap = &shape1->capsule;
	const f32 r_sum = cap->radius + shape2->sphere.radius + 2.0f * margin;

	mat3 rot;
	quat_to_mat3(rot, b1->rotation);

	vec3 c1, c2, s_p1, s_p2, diff;
	vec3_sub(c2, b2->position, b1->position);
	mat3_vec_mul(s_p1, rot, cap->p1);
	vec3_negative_to(s_p2, s_p1);
	struct segment s = segment_construct(s_p1, s_p2);
	const f32 dist_sq = segment_point_distance_sq(c1, &s, c2);

	if (dist_sq <= r_sum*r_sum)
	{
		contact_generated = 1;
		cm->v_count = 1;
		if (dist_sq <= COLLISION_POINT_DIST_SQ)
		{
			//TODO Degerate case: normal should be context dependent
			vec3_copy(cm->v[0], b1->position);
			if (s.dir[0]*s.dir[0] < s.dir[1]*s.dir[1])
			{
				if (s.dir[0]*s.dir[0] < s.dir[2]*s.dir[2]) { vec3_set(cm->v[2], 1.0f, 0.0f, 0.0f); }
				else { vec3_set(cm->v[2], 0.0f, 0.0f, 1.0f); }
			}
			else
			{
				if (s.dir[1]*s.dir[1] < s.dir[2]*s.dir[2]) { vec3_set(cm->v[0], 0.0f, 1.0f, 0.0f); }
				else { vec3_set(cm->v[2], 0.0f, 0.0f, 1.0f); }
			}
				
			vec3_set(cm->v[2], 1.0f, 0.0f, 0.0f);
			vec3_cross(diff, cm->v[2], s.dir);
			vec3_normalize(cm->n, diff);
			cm->depth[0] = r_sum;
		}
		else
		{
			vec3_sub(diff, c2, c1);
			vec3_normalize(cm->n, diff);
			vec3_translate_scaled(c1, cm->n, cap->radius + margin);
			vec3_translate_scaled(c2, cm->n, -(shape2->sphere.radius + margin));
			cm->depth[0] = vec3_dot(c1, cm->n) - vec3_dot(c2, cm->n);
			vec3_interpolate(cm->v[0], c1, c2, 0.5f);
			vec3_translate(cm->v[0], b1->position);
		}
			
	}

	return contact_generated;
}

static u32 capsule_contact(struct arena *garbage, struct contact_manifold *cm, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert(b1->shape_type == COLLISION_SHAPE_CAPSULE);
	assert(b2->shape_type == COLLISION_SHAPE_CAPSULE);

	u32 contact_generated = 0;

	const struct collision_capsule *cap1 = &((struct collision_shape *) string_database_address(pipeline->shape_db, b1->shape_handle))->capsule;
	const struct collision_capsule *cap2 = &((struct collision_shape *) string_database_address(pipeline->shape_db, b2->shape_handle))->capsule;
	f32 r_sum = cap1->radius + cap2->radius + 2.0f * margin;

	mat3 rot;
	vec3 c1, c2, p0, p1; /* line points */

	quat_to_mat3(rot, b1->rotation);
	mat3_vec_mul(p0, rot, cap1->p1);
	vec3_negative_to(p1, p0);
	vec3_translate(p0, b1->position);
	vec3_translate(p1, b1->position);
	struct segment s1 = segment_construct(p0, p1);
	
	quat_to_mat3(rot, b2->rotation);
	mat3_vec_mul(p0, rot, cap2->p1);
	vec3_negative_to(p1, p0);
	vec3_translate(p0, b2->position);
	vec3_translate(p1, b2->position);
	struct segment s2 = segment_construct(p0, p1);

	const f32 dist_sq = segment_distance_sq(c1, c2, &s1, &s2);
	if (dist_sq <= r_sum*r_sum)
	{
		contact_generated = 1;
		vec3 cross;
		vec3_cross(cross, s1.dir, s2.dir);
		const f32 cross_dist_sq = vec3_length_squared(cross);
		if (dist_sq <= COLLISION_POINT_DIST_SQ)
		{
			/* Degenerate Case 1: Parallel capsules,*/
			cm->depth[0] = r_sum;
			vec3_copy(cm->v[0], b1->position);
			if (cross_dist_sq <= COLLISION_POINT_DIST_SQ)
			{
				cm->v_count = 1;

				//TODO Normal should be context dependent
				if (s1.dir[0]*s1.dir[0] < s1.dir[1]*s1.dir[1])
				{
					if (s1.dir[0]*s1.dir[0] < s1.dir[2]*s1.dir[2]) { vec3_set(cm->n, 1.0f, 0.0f, 0.0f); }
					else { vec3_set(cm->n, 0.0f, 0.0f, 1.0f); }
				}
				else
				{
					if (s1.dir[1]*s1.dir[1] < s1.dir[2]*s1.dir[2]) { vec3_set(cm->n, 0.0f, 1.0f, 0.0f); }
					else { vec3_set(cm->n, 0.0f, 0.0f, 1.0f); }
				}
				vec3_cross(p0, s1.dir, cm->n);
				vec3_normalize(cm->n, p0);
			}
			/* Degenerate Case 2: Non-Parallel capsules, */
			else
			{
				cm->v_count = 1;
				vec3_normalize(cm->n, cross);
			}
		}
		else
		{
			vec3_sub(cm->n, c2, c1);
			vec3_mul_constant(cm->n, 1.0f / vec3_length(cm->n));
			vec3_translate_scaled(c1, cm->n, cap1->radius + margin);
			vec3_translate_scaled(c2, cm->n, -(cap2->radius + margin));
			const f32 d = vec3_dot(c1, cm->n) - vec3_dot(c2, cm->n);
			cm->depth[0] = d;
			if (cross_dist_sq <= COLLISION_POINT_DIST_SQ)
			{
				const f32 t1 = segment_point_closest_bc_parameter(&s1, s2.p0);
				const f32 t2 = segment_point_closest_bc_parameter(&s1, s2.p1);

				if (t1 != t2)
				{
					cm->v_count = 2;
					cm->depth[1] = d;
					segment_bc(cm->v[0], &s1, t1);
					segment_bc(cm->v[1], &s1, t2);
				}
				/* end-point contact point */
				else
				{
					cm->v_count = 1;
					vec3_interpolate(cm->v[0], c1, c2, 0.5f);
				}
			}
			else
			{
				cm->v_count = 1;
				vec3_interpolate(cm->v[0], c1, c2, 0.5f);
			}
		}
	}

	return contact_generated;
}

static u32 hull_sphere_contact(struct arena *garbage, struct contact_manifold *cm, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert (b1->shape_type == COLLISION_SHAPE_CONVEX_HULL);
	assert (b2->shape_type == COLLISION_SHAPE_SPHERE);

	const struct collision_shape *shape1 = string_database_address(pipeline->shape_db, b1->shape_handle);
	const struct collision_shape *shape2 = string_database_address(pipeline->shape_db, b2->shape_handle);

	u32 contact_generated = 0;

	struct gjk_input g1 = { .v = shape1->hull.v, .v_count = shape1->hull.v_count, };
	vec3_copy(g1.pos, b1->position);
	quat_to_mat3(g1.rot, b1->rotation);

	vec3 zero = VEC3_ZERO;
	struct gjk_input g2 = { .v = &zero, .v_count = 1, };
	vec3_copy(g2.pos, b2->position);
	mat3_identity(g2.rot);

	vec3 c1, c2;
	const f32 dist_sq = gjk_distance_sq(c1, c2, &g1, &g2);
	const f32 r_sum = shape2->sphere.radius + 2.0f * margin;

	/* Deep Penetration */
	if (dist_sq <= margin*margin)
	{
		contact_generated = 1;
		cm->v_count = 1;

		vec3 n;	
		const struct collision_hull *h = &shape1->hull;
		f32 min_depth = FLT_MAX;
		vec3 diff;
		vec3 p, best_p;
		for (u32 fi = 0; fi < h->f_count; ++fi)
		{
			collision_hull_face_normal(p, h, fi);
			mat3_vec_mul(n, g1.rot, p);
			mat3_vec_mul(p, g1.rot, h->v[h->e[h->f[fi].first].origin]);
			vec3_translate(p, b1->position);
			vec3_sub(diff, p, b2->position);
			const f32 depth = vec3_dot(n, diff);
			if (depth < min_depth)
			{
				vec3_copy(best_p, p);
				vec3_copy(cm->n, n);
				min_depth = depth;
			}
		}

		vec3_sub(diff, best_p, b2->position);
		cm->depth[0] = vec3_dot(cm->n, diff) + shape2->sphere.radius + 2.0f * margin;

		vec3_copy(cm->v[0], b2->position);
		vec3_translate_scaled(cm->v[0], cm->n, margin + min_depth);
	}
	/* Shallow Penetration */
	else if (dist_sq <= r_sum*r_sum)
	{
		contact_generated = 1;
		cm->v_count = 1;

		vec3_sub(cm->n, c2, c1);
		vec3_mul_constant(cm->n, 1.0f / vec3_length(cm->n));

		vec3_translate_scaled(c1, cm->n, margin);
		vec3_translate_scaled(c2, cm->n, -(shape2->sphere.radius + margin));
		cm->depth[0] = vec3_dot(c1, cm->n) - vec3_dot(c2, cm->n);

		vec3_interpolate(cm->v[0], c1, c2, 0.5f);

	}

	return contact_generated;
}

static u32 hull_capsule_contact(struct arena *garbage, struct contact_manifold *cm, const struct physics_pipeline *pipeline,  const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert(b1->shape_type == COLLISION_SHAPE_CONVEX_HULL);
	assert(b2->shape_type == COLLISION_SHAPE_CAPSULE);

	u32 contact_generated = 0;

	const struct collision_shape *shape1 = string_database_address(pipeline->shape_db, b1->shape_handle);
	const struct collision_shape *shape2 = string_database_address(pipeline->shape_db, b2->shape_handle);

	const struct collision_hull *h = &shape1->hull;
	struct gjk_input g1 = { .v = h->v, .v_count = h->v_count, };
	vec3_copy(g1.pos, b1->position);
	quat_to_mat3(g1.rot, b1->rotation);

	vec3 segment[2];
	vec3_copy(segment[0], shape2->capsule.p1);
	vec3_negative_to(segment[1], segment[0]);
	struct gjk_input g2 = { .v = segment, .v_count = 2, };
	vec3_copy(g2.pos, b2->position);
	//mat3_identity(g2.rot);
	quat_to_mat3(g2.rot, b2->rotation);

	vec3 c1, c2;
	const f32 dist_sq = gjk_distance_sq(c1, c2, &g1, &g2);
	const f32 r_sum = shape2->capsule.radius + 2.0f * margin;
	if (dist_sq <= r_sum*r_sum)
	{
		contact_generated = 1;

		vec3 p1, p2, tmp;
		mat3_vec_mul(p1, g2.rot, g2.v[0]);
		mat3_vec_mul(p2, g2.rot, g2.v[1]);
		vec3_translate(p1, g2.pos);
		vec3_translate(p2, g2.pos);
		struct segment cap_s = segment_construct(p1, p2);

		g2.v_count = 1;
		const u32 cap_p0_inside = (gjk_distance_sq(p1, tmp, &g1, &g2) == 0.0f) ? 1 : 0;
		vec3_copy(g2.v[0], g2.v[1]);
		const u32 cap_p1_inside = (gjk_distance_sq(p2, tmp, &g1, &g2) == 0.0f) ? 1 : 0;

		/* Deep Penetration */
		if (dist_sq <= margin*margin)
		{
			u32 edge_best = 0; 
			u32 best_index = 0;

			f32 max_d0 = -FLT_MAX;
			f32 max_d1 = -FLT_MAX;
			f32 max_signed_depth = -FLT_MAX;

			for (u32 fi = 0; fi < h->f_count; ++fi)
			{
				struct plane pl = collision_hull_face_plane(h, g1.rot, b1->position, fi);

				const f32 d0 = plane_point_signed_distance(&pl, cap_s.p0);
				const f32 d1 = plane_point_signed_distance(&pl, cap_s.p1);
				const f32 d = f32_min(d0, d1);
				if (max_signed_depth < d)
				{
					best_index = fi;
					max_signed_depth = d;
					max_d0 = d0;
					max_d1 = d1;
				}
			}

			/* For an edge to define seperating axis, either both or no end-points of the capsule must be inside */
			if (cap_p0_inside == cap_p1_inside)
			{
				for (u32 ei = 0; ei < h->e_count; ++ei)
				{
					struct segment edge_s = collision_hull_half_edge_segment(h, g1.rot, g1.pos, best_index);
					
					const f32 d = -f32_sqrt(segment_distance_sq(c1, c2, &edge_s, &cap_s));
					if (max_signed_depth < d)
					{
						edge_best = 1;
						best_index = ei;
						max_signed_depth = d;
						max_d0 = d;
					}
				}
			}

			//TODO Is this correct?
			cm->depth[0] = f32_max(-max_d0, 0.0f);
			cm->depth[1] = f32_max(-max_d1, 0.0f);
			if (edge_best)
			{
				cm->v_count = 1;
				struct segment edge_s = collision_hull_half_edge_segment(h, g1.rot, g1.pos, best_index);
				segment_distance_sq(c1, c2, &edge_s, &cap_s);
				vec3_sub(cm->n, c1, c2);
				vec3_mul_constant(cm->n, 1.0f / vec3_length(cm->n));
				vec3_copy(cm->v[0], c1);
			}
			else
			{
				cm->v_count = 2;
				collision_hull_face_normal(c1, h, best_index);
				mat3_vec_mul(cm->n, g1.rot, c1);
				struct segment s = collision_hull_face_clip_segment(h, g1.rot, g1.pos, best_index, &cap_s);
				const struct plane pl = collision_hull_face_plane(h, g1.rot, g1.pos, best_index);

				//COLLISION_DEBUG_ADD_SEGMENT(cap_s);
				//COLLISION_DEBUG_ADD_SEGMENT(s);
			

				if (cap_p0_inside == 1 && cap_p1_inside == 0)
				{
					vec3_copy(cm->v[0], s.p0);
					plane_segment_clip(cm->v[1], &pl, &s);
				}
				else if (cap_p0_inside == 0 && cap_p1_inside == 1)
				{
					plane_segment_clip(cm->v[0], &pl, &s);
					vec3_copy(cm->v[1], s.p1);
				}
				else
				{
					vec3_copy(cm->v[0], s.p0);
					vec3_copy(cm->v[1], s.p1);
				}
				
				vec3_translate_scaled(cm->v[0], cm->n, -plane_point_signed_distance(&pl, cm->v[0]));
				vec3_translate_scaled(cm->v[1], cm->n, -plane_point_signed_distance(&pl, cm->v[1]));
			}
		}
		/* Shallow Penetration */
		else
		{
			//COLLISION_DEBUG_ADD_SEGMENT(segment_construct(cap_s.p0, cap_s.p1));
			//COLLISION_DEBUG_ADD_SEGMENT(segment_construct(c1, c2));

			vec3_sub(cm->n, c2, c1);
			vec3_mul_constant(cm->n, 1.0f / vec3_length(cm->n));

			const struct collision_hull *h = &shape1->hull;

			/* (1) compute closest face points for end-point segement */
			vec3 s_dir, diff;
			vec3_normalize(s_dir, cap_s.dir);

			struct segment s = segment_construct(p1, p2);
			u32 fi;
			vec3 n1;
			u32 parallel = 0;
		
			/* If projected segment is not a point */
			if (vec3_dot(s.dir, s.dir) > COLLISION_POINT_DIST_SQ)
			{
				/* (2) Check if capsule is infront of some parallel plane   */
				/* find parallel face with vec3_dot(face_normal, segment_points) > 0.0f */
				struct hull_face *f;
				for (fi = 0; fi < h->f_count; ++fi)
				{
					f = h->f + fi;
					collision_hull_face_normal(n1, h, fi);

					const f32 d1d1 = vec3_dot(n1, n1);
					const f32 d2d2 = vec3_dot(s_dir, s_dir);
					const f32 d1d2 = vec3_dot(n1, s_dir);
					const f32 denom = d1d1*d2d2 - d1d2*d1d2;

					/* denom = (1-cos(theta)^2) == 1.0f <=> capsule and face normal orthogonal */
					//if (d1d2*d1d2 <= COLLISION_POINT_DIST_SQ)
					if (denom >= 1.0f - COLLISION_POINT_DIST_SQ)
					{	
						mat3_vec_mul(p2, g2.rot, g2.v[0]);
						vec3_translate(p2, g2.pos);
						mat3_vec_mul(p1, g1.rot, h->v[h->e[f->first].origin]);
						vec3_translate(p1, g1.pos);
						vec3_sub(diff, p2, p1);
						
						/* is capsule infront of face? */
						if (vec3_dot(diff, n1) > 0.0f)
						{
							vec3 center;
							vec3_interpolate(center, s.p0, s.p1, 0.5f);
							vec3_translate(n1, center);
							parallel = 1;
							break;
						}
					}
				}
			}

			if (parallel)
			{
				cm->v_count = 2;
				collision_hull_face_normal(cm->n, h, fi);
				vec3_translate_scaled(c1, cm->n, margin);
				vec3_translate_scaled(c2, cm->n, -(shape2->capsule.radius + margin));
				cm->depth[0] = vec3_dot(cm->n, c1) - vec3_dot(cm->n, c2);
				cm->depth[1] = cm->depth[0];
				struct segment s = collision_hull_face_clip_segment(h, g1.rot, g1.pos, fi, &cap_s);
				vec3_copy(cm->v[0], s.p0);
				vec3_copy(cm->v[1], s.p1);
				vec3_translate_scaled(cm->v[0], cm->n, -(shape2->capsule.radius + 2.0f*margin -cm->depth[0]));
				vec3_translate_scaled(cm->v[1], cm->n, -(shape2->capsule.radius + 2.0f*margin -cm->depth[1]));
			}
			else
			{
				cm->v_count = 1;
				vec3_sub(cm->n, c2, c1);
				vec3_mul_constant(cm->n, 1.0f / vec3_length(cm->n));
				vec3_translate_scaled(c1, cm->n, margin);
				vec3_translate_scaled(c2, cm->n, -(shape2->capsule.radius + margin));
				cm->depth[0] = vec3_dot(cm->n, c1) - vec3_dot(cm->n, c2);
				vec3_copy(cm->v[0], c1);
			}
		}
	}

	return contact_generated;
}

struct sat_face_query
{
	constvec3ptr v;
	vec3 normal;
	u32 fi;
	f32 depth;
};

struct sat_edge_query
{
	struct segment s1;
	struct segment s2;
	vec3 normal;
	f32 depth;
};

static u32 hull_contact_internal_face_contact(struct arena *mem_tmp, struct contact_manifold *cm, struct sat_face_query query[2], const struct rigid_body *b1, const struct collision_hull *hull1, constvec3ptr v1_world, const struct rigid_body *b2, const struct collision_hull *hull2, constvec3ptr v2_world)
{
	vec3 n_ref, n_inc, tmp1, tmp2, n;

	/* (1) setup reference_face */
	const struct collision_hull *ref_hull, *incollision_hull;
	struct hull_face *ref_face, *inc_face;
	u32 ref_i, inc_i;

	if (query[0].depth > query[1].depth)
	{
		ref_i = 0;
		inc_i = 1;
		vec3_copy(cm->n, query[0].normal);
		cm->depth[0] = -query[0].depth;
		ref_hull = hull1;
		incollision_hull = hull2;
		query[ref_i].v = v1_world;
		query[inc_i].v = v2_world;
	}
	else
	{
		ref_i = 1;
		inc_i = 0;
		vec3_scale(cm->n, query[1].normal, -1.0f);
		cm->depth[0] = -query[1].depth;
		ref_hull = hull2;
		incollision_hull = hull1;
		query[ref_i].v = v2_world;
		query[inc_i].v = v1_world;
	}

	ref_face = ref_hull->f + query[ref_i].fi;
	vec3_copy(n_ref, query[ref_i].normal);
	vec3_copy(n_inc, query[inc_i].normal);

	/* (2) determine incident_face */
	u32 inc_fi = 0;
	f32 min_dot = 1.0f;
	for (u32 fi = 0; fi < incollision_hull->f_count; ++fi)
	{
		vec3_sub(tmp1, query[inc_i].v[incollision_hull->e[incollision_hull->f[fi].first + 1].origin], query[inc_i].v[incollision_hull->e[incollision_hull->f[fi].first].origin]);
		vec3_sub(tmp2, query[inc_i].v[incollision_hull->e[incollision_hull->f[fi].first + 2].origin], query[inc_i].v[incollision_hull->e[incollision_hull->f[fi].first].origin]);
		vec3_cross(n, tmp1, tmp2);
		vec3_mul_constant(n, 1.0f / vec3_length(n));

		const f32 dot = vec3_dot(n_ref, n);
		if (dot < min_dot)
		{
			min_dot = dot;
			inc_fi = fi;
		}

	}
	inc_face = incollision_hull->f + inc_fi;

	/* (3) Setup world polygons */
	vec3ptr ref_v = arena_push(mem_tmp, ref_face->count * sizeof(vec3));
	vec3ptr inc_v = arena_push(mem_tmp, inc_face->count * sizeof(vec3));

	for (u32 i = 0; i < ref_face->count; ++i)
	{
		const u32 vi = ref_hull->e[ref_face->first + i].origin;
		vec3_copy(ref_v[i], query[ref_i].v[vi]);
	}

	for (u32 i = 0; i < inc_face->count; ++i)
	{
		const u32 vi = incollision_hull->e[inc_face->first + i].origin;
		vec3_copy(inc_v[i], query[inc_i].v[vi]);
	}

	/* (4) clip incident_face to reference_face */
	vec3ptr cp = arena_push(mem_tmp, inc_face->count * 2 * sizeof(vec3));
	f32 *depth = arena_push(mem_tmp, inc_face->count * 2 * sizeof(f32));
	f32 *min_t = arena_push(mem_tmp, inc_face->count * sizeof(f32));
	f32 *max_t = arena_push(mem_tmp, inc_face->count * sizeof(f32));
	u32 segments_outside = 0;

	for (u32 i = 0; i < inc_face->count; ++i)
	{
		struct segment inc_edge = segment_construct(inc_v[i], inc_v[(i+1) % inc_face->count]);
		min_t[i] = 0.0f;
		max_t[i] = 1.0f;
		for (u32 j = 0; j < ref_face->count; ++j)
		{
			vec3_sub(tmp1, ref_v[(j+1) % ref_face->count], ref_v[j]);
			vec3_cross(n, tmp1, n_ref);
			vec3_mul_constant(n, 1.0f / vec3_length(n));
			struct plane clip_plane = plane_construct(n, ref_v[j]);

			vec3_interpolate(tmp1, ref_v[(j+1) % ref_face->count], ref_v[j], 0.5f);
			vec3_add(tmp2, tmp1, n);
			COLLISION_DEBUG_ADD_SEGMENT(segment_construct(tmp1, tmp2));

			const f32 bc_c = plane_segment_clip_parameter(&clip_plane, &inc_edge);
			const f32 dot = vec3_dot(inc_edge.dir, clip_plane.normal);
			if (min_t[i] <= bc_c && bc_c <= max_t[i])
			{
				if (dot >= 0.0f)
				{
					max_t[i] = bc_c;
				}
				else
				{
					min_t[i] = bc_c;
				}
			}
			/* segment is fully infront of a plane */
			else if ((f32_abs(bc_c) == F32_INFINITY && plane_point_is_infront(&clip_plane, inc_edge.p0)) 
					|| (bc_c > 1.0f && dot < 0.0f) 
					|| (bc_c < 0.0f && dot > 0.0f))
			{
				max_t[i] = F32_INFINITY;
				segments_outside += 1;
				break;
			}
		}
	}

	f32 max_depth = -FLT_MAX;
	u32 deepest_point = 0;
	u32 cp_count = 0;
	/* insident face cosumes whole of reference face */
	if (segments_outside == inc_face->count)
	{
		//fprintf(stderr, "All segments outside\n");
		(ref_i == 0)
			? vec3_copy(cm->n, n_inc)
			: vec3_negative_to(cm->n, n_inc);
			
		for (u32 i = 0; i < ref_face->count; ++i)
		{
			vec3_sub(tmp1, ref_v[i], inc_v[0]);
			depth[cp_count] = -vec3_dot(tmp1, n_inc);
			if (depth[cp_count] >= 0.0f)
			{
				vec3_scale(cp[cp_count], n_inc, depth[cp_count]);
				vec3_translate(cp[cp_count], ref_v[i]);
				if (max_depth < depth[cp_count])
				{
					max_depth = depth[cp_count];
					deepest_point = cp_count;
				}
				cp_count += 1;
			}
		}
	}
	else
	{
		//fprintf(stderr, "%u segments outside\n", segments_outside);
		for (u32 i = 0; i < inc_face->count; ++i)
		{
			if (max_t[i] != F32_INFINITY)
			{
				vec3_interpolate(cp[cp_count], inc_v[(i+1) % inc_face->count], inc_v[i], min_t[i]);
				vec3_sub(tmp1, cp[cp_count], ref_v[0]);
				depth[cp_count] = -vec3_dot(tmp1, n_ref);
				if (depth[cp_count] >= 0.0f)
				{
					vec3_translate_scaled(cp[cp_count], n_ref, depth[cp_count]);
					if (max_depth < depth[cp_count])
					{
						max_depth = depth[cp_count];
						deepest_point = cp_count;
					}
					cp_count += 1;
				}

				if (max_t[i] != 1.0f)
				{
					vec3_interpolate(cp[cp_count], inc_v[(i+1) % inc_face->count], inc_v[i], max_t[i]);
					vec3_sub(tmp1, cp[cp_count], ref_v[0]);
					depth[cp_count] = -vec3_dot(tmp1, n_ref);
					if (depth[cp_count] >= 0.0f)
					{
						vec3_translate_scaled(cp[cp_count], n_ref, depth[cp_count]);
						if (max_depth < depth[cp_count])
						{
							max_depth = depth[cp_count];
							deepest_point = cp_count;
						}
						cp_count += 1;
					}
				}
			}
		}
	}

	for (u32 i = 0; i < cp_count; ++i)
	{
		COLLISION_DEBUG_ADD_SEGMENT(segment_construct(cp[i], cp[(i+1) % cp_count]));
	}

	u32 is_colliding = 1;
	switch (cp_count)
	{
		case 0:
		{
			is_colliding = 0;
		} break;

		case 1:
		{
			cm->v_count = 1;
			vec3_copy(cm->v[0], cp[0]);
			cm->depth[0] = depth[0];
		} break;

		case 2:
		{
			cm->v_count = 2;
			vec3_copy(cm->v[0], cp[0]);
			vec3_copy(cm->v[1], cp[1]);
			cm->depth[0] = depth[0];
			cm->depth[1] = depth[1];

		} break;

		case 3:
		{
			cm->v_count = 3;
			vec3_sub(tmp1, cp[1], cp[0]);	
			vec3_sub(tmp2, cp[2], cp[0]);	
			vec3_cross(n, tmp1, tmp2);
			if (vec3_dot(n, cm->n) >= 0.0f)
			{
				vec3_copy(cm->v[0], cp[0]);
				vec3_copy(cm->v[1], cp[1]);
				vec3_copy(cm->v[2], cp[2]);
				cm->depth[0] = depth[0];
				cm->depth[1] = depth[1];
				cm->depth[2] = depth[2];
			}
			else
			{
				vec3_copy(cm->v[0], cp[0]);
				vec3_copy(cm->v[2], cp[1]);
				vec3_copy(cm->v[1], cp[2]);
				cm->depth[0] = depth[0];
				cm->depth[2] = depth[1];
				cm->depth[1] = depth[2];
			}
		} break;

		default:
		{
			/* (1) First point is deepest point */
			cm->v_count = 4;
			vec3_copy(cm->v[0], cp[deepest_point]);
			cm->depth[0] = depth[deepest_point];

			/* (2) Third point is point furthest away from deepest point */
			f32 max_dist = 0.0f;
			u32 max_i = 0;
			for (u32 i = 0; i < cp_count; ++i)
			{
				const f32 dist = vec3_distance_squared(cp[deepest_point], cp[i]);
				if (max_dist < dist)
				{
					max_dist = dist;
					max_i = i;
				}
			}
			vec3_copy(cm->v[2], cp[max_i]);
			cm->depth[2] = depth[max_i];

			/* (3, 4) Second point and forth is point that gives largest (in magnitude) 
			 * areas with the previous points on each side of the previous segment 
			 */
			u32 max_neg_i = 0;
			u32 max_pos_i = 0;
			f32 max_neg = 0.0f;
			f32 max_pos = 0.0f;

			for (u32 i = 0; i < cp_count; ++i)
			{
				vec3_sub(tmp1, cm->v[0], cp[i]);
				vec3_sub(tmp2, cm->v[2], cp[i]);
				vec3_cross(n, tmp1, tmp2);
				const f32 d = vec3_length_squared(n);

				/* candidate for cm->v[3] */
				if (vec3_dot(n, cm->n) >= 0.0f)
				{
					if (max_pos < d)
					{
						max_pos = d;
						max_pos_i = i;
					}
				}
				else 
				{
					if (max_neg < d)
					{
						max_neg = d;
						max_neg_i = i;
					}
				}
			}

			vec3_copy(cm->v[1], cp[max_neg_i]);
			vec3_copy(cm->v[3], cp[max_pos_i]);
			cm->depth[1] = depth[max_neg_i];
			cm->depth[3] = depth[max_pos_i];
		} break;
	}

	return is_colliding;
}

static u32 hull_contact_internal_fv_seperation(struct sat_face_query *query, const struct collision_hull *h1, constvec3ptr v1_world, const struct collision_hull *h2, constvec3ptr v2_world)
{
	for (u32 fi = 0; fi < h1->f_count; ++fi)
	{
		const u32 f_v0 = h1->e[h1->f[fi].first    ].origin;
		const u32 f_v1 = h1->e[h1->f[fi].first + 1].origin;
		const u32 f_v2 = h1->e[h1->f[fi].first + 2].origin;
		const struct plane sep_plane = plane_construct_from_ccw_triangle(v1_world[f_v0], v1_world[f_v1], v1_world[f_v2]);

		f32 min_dist = FLT_MAX;
		for (u32 i = 0; i < h2->v_count; ++i)
		{
			const f32 dist = plane_point_signed_distance(&sep_plane, v2_world[i]);
			if (dist < min_dist)
			{
				min_dist = dist;
			}
		}

		if (min_dist > 0.0f) { return 1; }

		if (query->depth < min_dist)
		{
			query->fi = fi;
			query->depth = min_dist;
			/* We switch the sign of the normal outside the function, if need be */
			vec3_copy(query->normal, sep_plane.normal);
		}
	}

	return 0;
}

static u32 internal_ee_is_minkowski_face(const vec3 n1_1, const vec3 n1_2, const vec3 n2_1, const vec3 n2_2, const vec3 arc_n1, const vec3 arc_n2)
{
	const f32 n1_1d = vec3_dot(n1_1, arc_n2);
	const f32 n1_2d = vec3_dot(n1_2, arc_n2);
	const f32 n2_1d = vec3_dot(n2_1, arc_n1);
	const f32 n2_2d = vec3_dot(n2_2, arc_n1);

	/*
	 * last check is the hemisphere test: arc plane normals points "to the left" of the arc 1->2. 
	 * Thus, given the fact that the two first tests passes, which tells us that the two arcs 
	 * cross each others planes, the hemisphere test finally tells us if the arcs cross each other.
	 *
	 * If n2_1 lies in the positive half-space defined by arc1_n, and we know that n2_2 lies in the
	 * negative half-space, then the two arcs cross each other iff n2_1->n2_2 CCW relative to n1_2.
	 * This holds since from the first two check and n2_1->n2_2 CCW relative to n1_2, it must hold
	 * that arc_n2*n1_1 < 0.0f. If the arc is CW to n1_2, arc_n2*n1_1 > 0.0f.
	 *
	 * Similarly, if n2_1 lies in the negative half-space, then the two arcs cross each other iff 
	 * n2_1->n2_2 CW relative to n1_2 <=> arc_n2*n1_1 > 0.0f.
	 *
	 * It follows that intersection <=> (arc_n1*n2_1 > 0 && arc_n2*n1_1 < 0) || 
	 * 				    (arc_n1*n2_1 < 0 && arc_n2*n1_1 > 0)
	 *				<=>  arc_n1*n2_1 * arc_n2*n1_1) < 0
	 */
	return (n1_1d*n1_2d < 0.0f && n2_1d*n2_2d < 0.0f && n1_1d*n2_1d < 0.0f) ? 1 : 0;
}

/*
 * For full algorithm: see GDC talk by Dirk Gregorius - 
 * 	Physics for Game Programmers: The Separating Axis Test between Convex Polyhedra
 */
static u32 hull_contact_internal_ee_seperation(struct sat_edge_query *query, const struct collision_hull *h1, constvec3ptr v1_world, const struct collision_hull *h2, constvec3ptr v2_world, const vec3 h1_world_center)
{
	vec3 n1_1, n1_2, n2_1, n2_2, e1, e2;
	for (u32 e1_1 = 0; e1_1 < h1->e_count; ++e1_1)
	{
		for (u32 e2_1 = 0; e2_1 < h2->e_count; ++e2_1) 
		{
			u32 e1_2 = h1->e[e1_1].twin;
			u32 e2_2 = h2->e[e2_1].twin;
			const struct hull_face *f1 = h1->f + h1->e[e1_1].face_ccw;
			const struct hull_face *f2 = h2->f + h2->e[e2_1].face_ccw;
			collision_hull_face_direction(n1_1, h1, h1->e[e1_1].face_ccw);
			collision_hull_face_direction(n1_2, h1, h1->e[e1_2].face_ccw);
			collision_hull_face_direction(n2_1, h2, h2->e[e2_1].face_ccw);
			collision_hull_face_direction(n2_2, h2, h2->e[e2_2].face_ccw);

			/* we are working with minkowski difference A - B, so gauss map of B is (-B) */
			vec3_negative(n2_1);	
			vec3_negative(n2_2);

			/* No need to negate here, since e2 is orthogonal n2_1 and n2_2 either way */
			collision_hull_half_edge_direction(e1, h1, e1_1);
			collision_hull_half_edge_direction(e2, h2, e2_1);

			/* 
			 * test if A, -B edges intersect on gauss map, only if they do, 
			 * they are a candidate for collision
			 */
			if (internal_ee_is_minkowski_face(n1_1, n1_2, n2_1, n2_2, e1, e2))
			{
				e1_2 = f1->first + ((e1_1 - f1->first + 1) % f1->count);
				e2_2 = f2->first + ((e2_1 - f2->first + 1) % f2->count);
				const struct segment s1 = segment_construct(v1_world[h1->e[e1_1].origin], v1_world[h1->e[e1_2].origin]);
				const struct segment s2 = segment_construct(v2_world[h2->e[e2_1].origin], v2_world[h2->e[e2_2].origin]);
				const f32 d1d1 = vec3_dot(s1.dir, s1.dir);
				const f32 d2d2 = vec3_dot(s2.dir, s2.dir);
				const f32 d1d2 = vec3_dot(s1.dir, s2.dir);
				if (d1d1*d2d2 - d1d2*d1d2 <= F32_EPSILON*100.0f) { continue; }

				vec3_cross(e1, s1.dir, s2.dir);
				vec3_mul_constant(e1, 1.0f / vec3_length(e1));
				vec3_sub(e2, v1_world[h1->e[e1_2].origin], h1_world_center);
				/* plane normal points from A -> B */
				if (vec3_dot(e1, e2) < 0.0f)
				{
					vec3_negative(e1);
				}
				
				/* check segmente-segment distance interval signed plane distance, > 0.0f => we have found a seperating axis */
				vec3_sub(e2, v2_world[h2->e[e2_2].origin], v1_world[h1->e[e1_2].origin]);
				const f32 dist = vec3_dot(e1, e2);

				if (dist > 0.0f) 
				{
					//COLLISION_DEBUG_ADD_PLANE(sep_plane, v1_world[h1->e[e1_1].origin], 0.5f, 0.0f, 0.7f, 0.9f, 0.7f);
					return 1;
				}
			
				if (query->depth < dist)
				{
					query->depth = dist;
					vec3_copy(query->normal, e1);
					query->s1 = s1;
					query->s2 = s2;
				}
			}
		}
	}

	return 0;
}

/*
 * For the Algorithm, see
 * 	(Game Physics Pearls, Chapter 4)
 *	(GDC 2013 Dirk Gregorius, https://www.gdcvault.com/play/1017646/Physics-for-Game-Programmers-The)
 */
static u32 hull_contact(struct arena *tmp, struct contact_manifold *cm, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert(b1->shape_type == COLLISION_SHAPE_CONVEX_HULL);
	assert(b2->shape_type == COLLISION_SHAPE_CONVEX_HULL);

	/* 
	 * we want penetration depth d and direction normal n (b1->b2), 
	 * i.e. A - n*d just touches B,  or B + n*d just touches A.
	 */

	/*
	 * n = seperation normal from A to B
	 * Plane PA = plane n*x - dA denotes the plane with normal n that just touches A, pointing towards B 
	 * Plane PB = plane (-n)*x - dB denotes the plane with normal (-n) that just touches B, pointing towards A
	 *
	 * We seek * (n,d) = sup_{s on unit-sphere}(d : (s,d)). If we find a seperating axis, no contact manifold
	 * is generated and we get an early exit, returning 0;
	 */

	//TODO: Margins??
	arena_push_record(tmp);
	u32 is_colliding = 1;

	mat3 rot1, rot2;
	quat_to_mat3(rot1, b1->rotation);
	quat_to_mat3(rot2, b2->rotation);

	struct collision_hull *h1 = &((struct collision_shape *) string_database_address(pipeline->shape_db, b1->shape_handle))->hull;
	struct collision_hull *h2 = &((struct collision_shape *) string_database_address(pipeline->shape_db, b2->shape_handle))->hull;

	vec3ptr v1_world = arena_push(tmp, h1->v_count * sizeof(vec3));
	vec3ptr v2_world = arena_push(tmp, h2->v_count * sizeof(vec3));

	vec3 h1_world_center = VEC3_ZERO;
	for (u32 i = 0; i < h1->v_count; ++i)
	{
		mat3_vec_mul(v1_world[i], rot1, h1->v[i]);
		vec3_translate(v1_world[i], b1->position);
		vec3_translate(h1_world_center, v1_world[i]);
	}
	vec3_mul_constant(h1_world_center, 1.0f/h1->v_count);

	for (u32 i = 0; i < h2->v_count; ++i)
	{
		mat3_vec_mul(v2_world[i], rot2, h2->v[i]);
		vec3_translate(v2_world[i], b2->position);
	}

	struct sat_face_query f_query[2] = { { .depth = -FLT_MAX }, { .depth = -FLT_MAX } };
	struct sat_edge_query e_query = { .depth = -FLT_MAX };

	if (hull_contact_internal_fv_seperation(&f_query[0], h1, v1_world, h2, v2_world))
	{
		is_colliding = 0;
		goto sat_cleanup;
	}

	if (hull_contact_internal_fv_seperation(&f_query[1], h2, v2_world, h1, v1_world))
	{
		is_colliding = 0;
		goto sat_cleanup;
	}

	if (hull_contact_internal_ee_seperation(&e_query, h1, v1_world, h2, v2_world, h1_world_center))
	{
		is_colliding = 0;
		goto sat_cleanup;
	}

	//fprintf(stderr, "%f\n%f\n%f\n", f_query[0].depth, f_query[1].depth, e_query.depth);
	if ((1.0f - 100.0f * F32_EPSILON) * f_query[0].depth >= e_query.depth || (1.0f - 100.0f * F32_EPSILON) * f_query[1].depth >= e_query.depth)
	{
		is_colliding = hull_contact_internal_face_contact(tmp, cm, f_query, b1, h1, v1_world, b2, h2, v2_world);
	}
	/* edge_contact */
	else
	{
		vec3 c1, c2;
		segment_distance_sq(c1, c2, &e_query.s1, &e_query.s2);
		//COLLISION_DEBUG_ADD_SEGMENT(e_query.s1);
		//COLLISION_DEBUG_ADD_SEGMENT(e_query.s2);
		COLLISION_DEBUG_ADD_SEGMENT(segment_construct(c1,c2));

		cm->v_count = 1;
		cm->depth[0] = -e_query.depth;
		vec3_interpolate(cm->v[0], c1, c2, 0.5f);
		vec3_copy(cm->n, e_query.normal);
	}

sat_cleanup:
	arena_pop_record(tmp);
	return is_colliding;
}

/********************************** RAYCAST **********************************/

f32 collision_sphere_raycast_parameter(const struct physics_pipeline *pipeline, const struct rigid_body *b, const struct ray *ray)
{
	assert(b->shape_type == COLLISION_SHAPE_SPHERE);
	const struct collision_shape *shape = string_database_address(pipeline->shape_db, b->shape_handle);
	struct sphere sph = sphere_construct(b->position, shape->sphere.radius);
	return sphere_raycast_parameter(&sph, ray);	
}

f32 collision_capsule_raycast_parameter(const struct physics_pipeline *pipeline, const struct rigid_body *b, const struct ray *ray)
{
	assert(b->shape_type == COLLISION_SHAPE_CAPSULE);

	const struct collision_shape *shape = string_database_address(pipeline->shape_db, b->shape_handle);
	mat3 rot;
	vec3 p0, p1;
	quat_to_mat3(rot, b->rotation);
	mat3_vec_mul(p0, rot, shape->capsule.p1);
	vec3_negative_to(p1, p0);
	vec3_translate(p0, b->position);
	vec3_translate(p1, b->position);
	struct segment s = segment_construct(p0, p1);

	const f32 r = shape->capsule.radius;
	const f32 dist_sq = ray_segment_distance_sq(p0, p1, ray, &s);
	if (dist_sq > r*r) { return F32_INFINITY; }

	struct sphere sph = sphere_construct(p1, r);
	return sphere_raycast_parameter(&sph, ray);
}

f32 collision_hull_raycast_parameter(const struct physics_pipeline *pipeline, const struct rigid_body *b, const struct ray *ray)
{
	assert(b->shape_type == COLLISION_SHAPE_CONVEX_HULL);

	vec3 n, p;
	mat3 rot;
	quat_to_mat3(rot, b->rotation);
	const struct collision_hull *h = &((struct collision_shape *) string_database_address(pipeline->shape_db, b->shape_handle))->hull;
	f32 t_best = F32_INFINITY;

	for (u32 fi = 0; fi < h->f_count; ++fi)
	{
		collision_hull_face_normal(p, h, fi);
		mat3_vec_mul(n, rot, p);
		vec3_translate(p, b->position);

		struct plane pl = collision_hull_face_plane(h, rot, b->position, fi);
		const f32 t = plane_raycast_parameter(&pl, ray);
		if (t < t_best && t >= 0.0f)
		{
			ray_point(p, ray, t);
			if (collision_hull_face_projected_point_test(h, rot, b->position, fi, p))
			{
				t_best = t;
			}
		}
	}

	return t_best;
}

/********************************** LOOKUP TABLES FOR SHAPES **********************************/

u32 (*shape_tests[COLLISION_SHAPE_COUNT][COLLISION_SHAPE_COUNT])(const struct physics_pipeline *, const struct rigid_body *, const struct rigid_body *, const f32 margin) =
{
	{ sphere_test, 		0, 			0, 		0, },
	{ capsule_sphere_test,	capsule_test, 		0, 		0, },
	{ hull_sphere_test, 	hull_capsule_test,	hull_test,	0, },
	{ 0, 			0, 			0, 		0, },
};

f32 (*distance_methods[COLLISION_SHAPE_COUNT][COLLISION_SHAPE_COUNT])(vec3 c1, vec3 c2, const struct physics_pipeline *, const struct rigid_body *, const struct rigid_body *, const f32) =
{
	{ sphere_distance,	 	0,			0, 		0, },
	{ capsule_sphere_distance,	capsule_distance, 	0, 		0, },
	{ hull_sphere_distance, 	hull_capsule_distance, 	hull_distance,	0, },
	{ 0, 				0, 			0,		0, },
};

u32 (*contact_methods[COLLISION_SHAPE_COUNT][COLLISION_SHAPE_COUNT])(struct arena *, struct contact_manifold *, const struct physics_pipeline *, const struct rigid_body *, const struct rigid_body *, const f32) =
{
	{ sphere_contact,	 	0, 			0,		0, },
	{ capsule_sphere_contact, 	capsule_contact,	0, 		0, },
	{ hull_sphere_contact, 	  	hull_capsule_contact,	hull_contact, 	0, },
	{ 0, 			  	0,			0, 		0, },
};

f32 (*shape_raycast_parameter_methods[COLLISION_SHAPE_COUNT])(const struct physics_pipeline *, const struct rigid_body *, const struct ray *) =
{
	collision_sphere_raycast_parameter,
	collision_capsule_raycast_parameter,
	collision_hull_raycast_parameter,
	0,
};

u32 body_body_test(const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert(margin >= 0.0f);
	return (b1->shape_type >= b2->shape_type)  
		? shape_tests[b1->shape_type][b2->shape_type](pipeline, b1, b2, margin)
		: shape_tests[b2->shape_type][b1->shape_type](pipeline, b2, b1, margin);
}

f32 body_body_distance(vec3 c1, vec3 c2, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	assert(margin >= 0.0f);
	return (b1->shape_type >= b2->shape_type)  
		? distance_methods[b1->shape_type][b2->shape_type](c1, c2, pipeline, b1, b2, margin)
		: distance_methods[b2->shape_type][b1->shape_type](c2, c1, pipeline, b2, b1, margin);
}

u32 body_body_contact_manifold(struct arena *tmp, struct contact_manifold *cm, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin)
{
	kas_assert(margin >= 0.0f);

	/* TODO: Cannot do as above, we must make sure that CM is in correct A->B order,  maybe push this issue up? */
	return (b1->shape_type >= b2->shape_type)  
		? contact_methods[b1->shape_type][b2->shape_type](tmp, cm, pipeline, b1, b2, margin)
		: contact_methods[b2->shape_type][b1->shape_type](tmp, cm, pipeline, b2, b1, margin);
}

f32 body_raycast_parameter(const struct physics_pipeline *pipeline, const struct rigid_body *b, const struct ray *ray)
{
	return shape_raycast_parameter_methods[b->shape_type](pipeline, b, ray);
}

u32 body_raycast(vec3 intersection, const struct physics_pipeline *pipeline, const struct rigid_body *b, const struct ray *ray)
{
	const f32 t = body_raycast_parameter(pipeline, b, ray);
	if (t == F32_INFINITY) return 0;

	vec3_copy(intersection, ray->origin);
	vec3_translate_scaled(intersection, ray->dir, t);
	return 1;
}

/********************************** COLLISION STATE **********************************/

void collision_state_clear_frame(struct collision_state *c_state)
{
	c_state->proxy_overlap = NULL;
	c_state->overlap_count = 0;

	c_state->cm = NULL;
	c_state->cm_count = 0;
}
