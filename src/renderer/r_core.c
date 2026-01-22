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
	ds_StaticAssert(R_CMD_SCREEN_LAYER_BITS
			+ R_CMD_DEPTH_BITS
			+ R_CMD_TRANSPARENCY_BITS
			+ R_CMD_MATERIAL_BITS
			+ R_CMD_PRIMITIVE_BITS
			+ R_CMD_INSTANCED_BITS
			+ R_CMD_ELEMENTS_BITS
			+ R_CMD_UNUSED_BITS == 64, "r_cmd definitions should span whole 64 bits");

	//TODO Show no overlap between masks
	ds_StaticAssert((R_CMD_SCREEN_LAYER_MASK & R_CMD_DEPTH_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_SCREEN_LAYER_MASK & R_CMD_TRANSPARENCY_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_SCREEN_LAYER_MASK & R_CMD_MATERIAL_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_SCREEN_LAYER_MASK & R_CMD_PRIMITIVE_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_SCREEN_LAYER_MASK & R_CMD_INSTANCED_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_SCREEN_LAYER_MASK & R_CMD_ELEMENTS_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_DEPTH_MASK & R_CMD_TRANSPARENCY_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_DEPTH_MASK & R_CMD_MATERIAL_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_DEPTH_MASK & R_CMD_PRIMITIVE_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_DEPTH_MASK & R_CMD_INSTANCED_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_DEPTH_MASK & R_CMD_ELEMENTS_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_TRANSPARENCY_MASK & R_CMD_MATERIAL_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_TRANSPARENCY_MASK & R_CMD_PRIMITIVE_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_TRANSPARENCY_MASK & R_CMD_INSTANCED_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_TRANSPARENCY_MASK & R_CMD_ELEMENTS_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_MATERIAL_MASK & R_CMD_PRIMITIVE_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_MATERIAL_MASK & R_CMD_INSTANCED_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_MATERIAL_MASK & R_CMD_ELEMENTS_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_PRIMITIVE_MASK & R_CMD_INSTANCED_MASK) == 0, "R_CMD_*_MASK values should not overlap");
	ds_StaticAssert((R_CMD_PRIMITIVE_MASK & R_CMD_ELEMENTS_MASK) == 0, "R_CMD_*_MASK values should not overlap");

	ds_StaticAssert(R_CMD_SCREEN_LAYER_MASK
			+ R_CMD_DEPTH_MASK
			+ R_CMD_TRANSPARENCY_MASK
			+ R_CMD_MATERIAL_MASK
			+ R_CMD_PRIMITIVE_MASK
			+ R_CMD_INSTANCED_MASK
			+ R_CMD_ELEMENTS_MASK
			+ R_CMD_UNUSED_MASK == U64_MAX, "sum of r_cmd masks should be U64");
}

static void material_static_assert(void)
{
	ds_StaticAssert(MATERIAL_PROGRAM_BITS + MATERIAL_MESH_BITS + MATERIAL_TEXTURE_BITS + MATERIAL_UNUSED_BITS 
			== R_CMD_MATERIAL_BITS, "material definitions should span whole material bit range");

	ds_StaticAssert((MATERIAL_PROGRAM_MASK & MATERIAL_TEXTURE_MASK) == 0
			, "MATERIAL_*_MASK values should not overlap");
	ds_StaticAssert((MATERIAL_PROGRAM_MASK & MATERIAL_MESH_MASK) == 0
			, "MATERIAL_*_MASK values should not overlap");
	ds_StaticAssert((MATERIAL_TEXTURE_MASK & MATERIAL_MESH_MASK) == 0
			, "MATERIAL_*_MASK values should not overlap");

	ds_StaticAssert(MATERIAL_PROGRAM_MASK + MATERIAL_MESH_MASK + MATERIAL_TEXTURE_MASK + MATERIAL_UNUSED_MASK
			== (R_CMD_MATERIAL_MASK >> R_CMD_MATERIAL_LOW_BIT)
			, "sum of material masks should fill the material mask");

	ds_StaticAssert(PROGRAM_COUNT <= (1 << MATERIAL_PROGRAM_BITS), "Material program mask to small, increase size");
	ds_StaticAssert(TEXTURE_COUNT <= (1 << MATERIAL_TEXTURE_BITS), "Material program mask to small, increase size");
}
