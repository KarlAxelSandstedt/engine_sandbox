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

#include <string.h>

#include "r_local.h"

struct r_core r_core_storage = { 0 };
struct r_core *g_r_core = &r_core_storage;

static void r_proxy3d_vertex_assert_layout(void)
{
	kas_static_assert((u64) &(((struct r_proxy3d_v *)0)->position) 		== R_PROXY3D_V_POSITION_OFFSET, "unexpected layout of r_proxy3d_vertex");
	kas_static_assert((u64) &(((struct r_proxy3d_v *)0)->color) 		== R_PROXY3D_V_COLOR_OFFSET, "unexpected layout of r_proxy3d_vertex");
	kas_static_assert((u64) &(((struct r_proxy3d_v *)0)->normal) 		== R_PROXY3D_V_NORMAL_OFFSET, "unexpected layout of r_proxy3d_vertex");
	kas_static_assert((u64) &(((struct r_proxy3d_v *)0)->translation) 	== R_PROXY3D_V_TRANSLATION_OFFSET, "unexpected layout of r_proxy3d_vertex");
	kas_static_assert((u64) &(((struct r_proxy3d_v *)0)->rotation) 		== R_PROXY3D_V_ROTATION_OFFSET, "unexpected layout of r_proxy3d_vertex");
	kas_static_assert(R_PROXY3D_V_PACKED_SIZE 				== R_PROXY3D_V_ROTATION_OFFSET + sizeof(vec4), "unexpected layout of r_proxy3d_vertex");
}

static void r_unit_static_assert(void)
{
	kas_static_assert(&(((struct r_unit *)0)->header) == 0, "intrusive node data structure must be at offset 0");
}

static void r_cmd_static_assert(void)
{
	kas_static_assert(R_CMD_SCREEN_LAYER_BITS
			+ R_CMD_DEPTH_BITS
			+ R_CMD_TRANSPARENCY_BITS
			+ R_CMD_MATERIAL_BITS
			+ R_CMD_PRIMITIVE_BITS
			+ R_CMD_INSTANCED_BITS
			+ R_CMD_UNUSED_BITS == 64, "r_cmd definitions should span whole 64 bits");

	//TODO Show no overlap between masks
	kas_static_assert((R_CMD_SCREEN_LAYER_MASK & R_CMD_DEPTH_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	kas_static_assert((R_CMD_SCREEN_LAYER_MASK & R_CMD_TRANSPARENCY_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	kas_static_assert((R_CMD_SCREEN_LAYER_MASK & R_CMD_MATERIAL_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	kas_static_assert((R_CMD_SCREEN_LAYER_MASK & R_CMD_PRIMITIVE_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	kas_static_assert((R_CMD_SCREEN_LAYER_MASK & R_CMD_INSTANCED_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	kas_static_assert((R_CMD_DEPTH_MASK & R_CMD_TRANSPARENCY_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	kas_static_assert((R_CMD_DEPTH_MASK & R_CMD_MATERIAL_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	kas_static_assert((R_CMD_DEPTH_MASK & R_CMD_PRIMITIVE_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	kas_static_assert((R_CMD_DEPTH_MASK & R_CMD_INSTANCED_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	kas_static_assert((R_CMD_TRANSPARENCY_MASK & R_CMD_MATERIAL_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	kas_static_assert((R_CMD_TRANSPARENCY_MASK & R_CMD_PRIMITIVE_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	kas_static_assert((R_CMD_TRANSPARENCY_MASK & R_CMD_INSTANCED_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	kas_static_assert((R_CMD_MATERIAL_MASK & R_CMD_PRIMITIVE_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	kas_static_assert((R_CMD_MATERIAL_MASK & R_CMD_INSTANCED_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	kas_static_assert((R_CMD_PRIMITIVE_MASK & R_CMD_INSTANCED_MASK) == 0, "R_CMD_*_MASK values should not overlap");

	kas_static_assert(R_CMD_SCREEN_LAYER_MASK
			+ R_CMD_DEPTH_MASK
			+ R_CMD_TRANSPARENCY_MASK
			+ R_CMD_MATERIAL_MASK
			+ R_CMD_PRIMITIVE_MASK
			+ R_CMD_INSTANCED_MASK
			+ R_CMD_UNUSED_MASK == U64_MAX, "sum of r_cmd masks should be U64");
}

static void material_static_assert(void)
{
	kas_static_assert(MATERIAL_PROGRAM_BITS + MATERIAL_TEXTURE_BITS + MATERIAL_UNUSED_BITS 
			== R_CMD_MATERIAL_BITS, "material definitions should span whole material bit range");

	kas_static_assert((MATERIAL_PROGRAM_MASK & MATERIAL_TEXTURE_MASK) == 0
			, "MATERIAL_*_MASK values should not overlap");

	kas_static_assert(MATERIAL_PROGRAM_MASK + MATERIAL_TEXTURE_MASK + MATERIAL_UNUSED_MASK
			== (R_CMD_MATERIAL_MASK >> R_CMD_MATERIAL_LOW_BIT)
			, "sum of material masks should fill the material mask");

	kas_static_assert(PROGRAM_COUNT <= (1 << MATERIAL_PROGRAM_BITS), "Material program mask to small, increase size");
	kas_static_assert(TEXTURE_COUNT <= (1 << MATERIAL_TEXTURE_BITS), "Material program mask to small, increase size");
}

u64 r_material_construct(const u64 program, const u64 texture)
{
	kas_assert(program <= (MATERIAL_PROGRAM_MASK >> MATERIAL_PROGRAM_LOW_BIT));
	kas_assert(texture <= (MATERIAL_TEXTURE_MASK >> MATERIAL_TEXTURE_LOW_BIT));

	return (program << MATERIAL_PROGRAM_LOW_BIT) | (texture << MATERIAL_TEXTURE_LOW_BIT);
}

void r_proxy3d_buffer_layout_setter(void)
{
	kas_glEnableVertexAttribArray(0);
	kas_glEnableVertexAttribArray(1);
	kas_glEnableVertexAttribArray(2);
	kas_glEnableVertexAttribArray(3);
	kas_glEnableVertexAttribArray(4);

	kas_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, R_PROXY3D_V_PACKED_SIZE, (void *) (0));
	kas_glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, R_PROXY3D_V_PACKED_SIZE, (void *) (sizeof(vec3)));
	kas_glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, R_PROXY3D_V_PACKED_SIZE, (void *) (sizeof(vec3) + sizeof(vec4)));
	kas_glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, R_PROXY3D_V_PACKED_SIZE, (void *) (sizeof(vec3) + sizeof(vec4) + sizeof(vec3)));
	kas_glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, R_PROXY3D_V_PACKED_SIZE, (void *) (sizeof(vec3) + sizeof(vec4) + sizeof(vec3) + sizeof(vec3)));
}

struct r_static_range *r_static_range_init(struct arena *mem, const u64 vertex_offset, const u64 index_offset)
{
	struct r_static_range *range = arena_push(mem, sizeof(struct r_static_range));
	if (range)
	{
		range->next = NULL;
		range->vertex_size = 0;
		range->vertex_offset = vertex_offset;
		range->index_count = 0;
		range->index_offset = index_offset;
	}
	return range;
}

u32 r_static_alloc(const u64 screen, const u64 transparency, const u64 depth, const u64 material, const u64 primitive)
{
	 return r_unit_alloc(R_UNIT_PARENT_NONE, R_UNIT_TYPE_STATIC, screen, transparency, depth, material, primitive);
}

void r_static_dealloc(const u32 unit)
{
	kas_assert(r_unit_lookup(unit)->type == R_UNIT_TYPE_STATIC);
	r_unit_dealloc(&g_r_core->frame, unit);
}
 
struct r_static *r_static_lookup(const u32 unit)
{
	struct r_unit *u = r_unit_lookup(unit);
	if (u)
	{
		kas_assert(u->type == R_UNIT_TYPE_STATIC);
		return array_list_intrusive_address(g_r_core->static_list, u->type_index);
	}
	else
	{
		return NULL;
	}

}

u32 r_unit_alloc(const u32 parent_handle, const enum r_unit_type type, const u64 screen, const u64 transparency, const u64 depth, const u64 material, const u64 primitive)
{
	const u32 max_count = g_r_core->unit_hierarchy->list->max_count;
	struct slot slot = hierarchy_index_add(g_r_core->unit_hierarchy, parent_handle);
	const u32 unit_i = slot.index;
	struct r_unit *unit = slot.address;
	unit->generation = (max_count < g_r_core->unit_hierarchy->list->max_count)
		? 0
		: unit->generation + 1;

	if (unit_i >= g_r_core->unit_allocation.bit_count)
	{
		bit_vec_increase_size(&g_r_core->unit_allocation, g_r_core->unit_allocation.bit_count << 1, 0);
	}
	bit_vec_set_bit(&g_r_core->unit_allocation, unit_i, 1);

	unit->type = type;
	switch (type)
	{
		case R_UNIT_TYPE_PROXY3D:
		{
			if (parent_handle != R_UNIT_PARENT_NONE)
			{
				const struct r_unit *parent = ((struct r_unit *) g_r_core->unit_hierarchy->list->slot) + parent_handle;
				kas_assert(parent->type == R_UNIT_TYPE_PROXY3D);
				unit->type_index = hierarchy_index_add(g_r_core->proxy3d_hierarchy, parent->type_index).index;
			}
			else
			{
				unit->type_index = hierarchy_index_add(g_r_core->proxy3d_hierarchy, HI_ROOT_STUB_INDEX).index;
			}
		} break;

		case R_UNIT_TYPE_STATIC:
		{
			unit->type_index = array_list_intrusive_reserve_index(g_r_core->static_list);
		} break;

		default:
		{
			kas_assert_string(0, "Unimplemented r_unit_type\n");
		} break;
	}

	return unit_i;
}

void r_unit_dealloc(struct arena *tmp, const u32 handle)
{
	const u32 allocated = bit_vec_get_bit(&g_r_core->unit_allocation, handle);
	kas_assert(allocated);
	if (allocated)
	{
		struct r_unit *unit = r_unit_lookup(handle);
		switch (unit->type)
		{
			case R_UNIT_TYPE_PROXY3D:
			{
				hierarchy_index_remove(tmp, g_r_core->proxy3d_hierarchy, handle);
			} break;

			case R_UNIT_TYPE_STATIC:
			{
				array_list_intrusive_remove_index(g_r_core->static_list, unit->type_index);
			} break;

			default:
			{
				kas_assert_string(0, "Unimplemented r_unit_type\n");
			} break;
		}

		//push handle
		struct hierarchy_index_iterator it = hierarchy_index_iterator_init(tmp, g_r_core->unit_hierarchy, handle);
		//TODO root node cannot be iterated over in the same way as offspring nodes...
		while(it.count)
		{
			const u32 index = hierarchy_index_iterator_next_df(&it);
			struct r_unit *unit = r_unit_lookup(index);
			kas_assert(bit_vec_get_bit(&g_r_core->unit_allocation, index));
			bit_vec_set_bit(&g_r_core->unit_allocation, index, 0);
		}
		hierarchy_index_iterator_release(&it);
		if (it.forced_malloc)
		{
			log_string(T_RENDERER, S_ERROR, "hierarchy iterator arena ran out of memory deallocating sub-hierarchy and forced heap allocations, increase arena memory!");
		}

		//TODO We could skip double traversal by doing node removals in first traversal... do later if needed
		hierarchy_index_remove(tmp, g_r_core->unit_hierarchy, handle);
	}
}

struct r_unit *r_unit_lookup(const u32 handle)
{
	const u32 allocated = bit_vec_get_bit(&g_r_core->unit_allocation, handle);
	return (allocated) 
		? ((struct r_unit *) g_r_core->unit_hierarchy->list->slot) + handle 
		: NULL;
}

void r_unit_set_flags(const u32 unit_handle, const u32 flags)
{
	struct r_unit *unit = r_unit_lookup(unit_handle);
	if (unit)
	{
		unit->draw_flags |= flags;
	}
}

void r_unit_unset_flags(const u32 unit_handle, const u32 flags)
{
	struct r_unit *unit = r_unit_lookup(unit_handle);
	if (unit)
	{
		unit->draw_flags &= ~flags;
	}
}
