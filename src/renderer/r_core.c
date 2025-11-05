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

struct slot r_unit_alloc(const utf8 mesh)
{
	struct slot slot = gpool_add(&g_r_core->unit_pool);
	struct r_unit *unit = slot.address;

	unit->mesh = string_database_reference(g_r_core->mesh_database, mesh).index;
	if (slot.index == STRING_DATABASE_STUB_INDEX)
	{
		log(T_LED, S_WARNING, "Failed to find mesh %k in r_unit_alloc, using stub mesh.", &mesh);
	}
	unit->type = R_UNIT_TYPE_NONE;

	return slot;
}

void r_unit_dealloc(struct arena *tmp, const u32 handle)
{
	struct r_unit *unit = r_unit_address(handle);
	if (POOL_SLOT_ALLOCATED(unit))
	{
		switch (unit->type)
		{
			case R_UNIT_TYPE_NONE:
			{
				
			} break;

			case R_UNIT_TYPE_PROXY3D:
			{
				hierarchy_index_remove(tmp, g_r_core->proxy3d_hierarchy, unit->type_index);
			} break;

			default:
			{
				kas_assert_string(0, "Unimplemented r_unit_type\n");
			} break;
		}

		string_database_dereference(g_r_core->mesh_database, unit->mesh);
		gpool_remove_address(&g_r_core->unit_pool, unit);
	}
}

struct r_unit *r_unit_address(const u32 handle)
{
	struct r_unit *unit = pool_address(&g_r_core->unit_pool, handle);
	kas_assert(POOL_SLOT_ALLOCATED(unit));
	return unit;
}
