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

#include "r_local.h"
#include "ui_public.h"

void r_ui_draw(struct ui *ui)
{
	const vec4 zero4 = { 0.0f, 0.0f, 0.0f, 0.0f };
	const vec3 zero3 = { 0.0f, 0.0f, 0.0f };

	struct ui_draw_bucket *b = ui->bucket_first;
	for (u32 i = 0; i < ui->bucket_count; ++i)
	{
		b = b->next;
		/* we reverse depth since in ui, larger depths goes infront, but in renderer lower depths drawn last */
		const u64 depth = ((1 << R_CMD_DEPTH_BITS) - 1) - (UI_CMD_DEPTH_GET(b->cmd) << UI_CMD_LAYER_BITS);
		const u64 transparency = (UI_CMD_DEPTH_GET(b->cmd) == UI_CMD_LAYER_TEXT_SELECTION)
			? R_CMD_TRANSPARENCY_SUBTRACTIVE
			: R_CMD_TRANSPARENCY_ADDITIVE;
		struct r_instance *instance = r_instance_add_non_cached(
				r_command_key(
					R_CMD_SCREEN_LAYER_HUD,
					depth + UI_CMD_LAYER_GET(b->cmd),	
					R_CMD_TRANSPARENCY_ADDITIVE,
					r_material_construct(PROGRAM_UI, MESH_NONE, UI_CMD_TEXTURE_GET(b->cmd)),
					R_CMD_PRIMITIVE_TRIANGLE,
					R_CMD_INSTANCED));
		instance->type = R_INSTANCE_UI;
		instance->ui_bucket = b;

		if (UI_CMD_LAYER_GET(b->cmd) == UI_CMD_LAYER_TEXT)
		{
			u64 total_glyph_count = 0;
			struct ui_draw_node *draw_node = instance->ui_bucket->list;
			for (u32 i = 0; i < instance->ui_bucket->count; ++i)
			{
				const struct ui_node *n = hierarchy_index_address(ui->node_hierarchy, draw_node->index);
				draw_node = draw_node->next;
				struct text_line *line = n->layout_text->line;
				for (u32 l = 0; l < n->layout_text->line_count; ++l)
				{
					total_glyph_count += line->glyph_count;
 					line = line->next;
				}
			}
			instance->ui_bucket->count = total_glyph_count;
		}
	}
}

void r_ui_buffer_shared_layout_setter(void)
{
	kas_glEnableVertexAttribArray(0);
	kas_glEnableVertexAttribArray(1);
	kas_glEnableVertexAttribArray(2);
	kas_glEnableVertexAttribArray(3);
	kas_glEnableVertexAttribArray(4);
	kas_glEnableVertexAttribArray(5);
	kas_glEnableVertexAttribArray(6);
	kas_glEnableVertexAttribArray(7);
	kas_glEnableVertexAttribArray(8);
	kas_glEnableVertexAttribArray(9);
	kas_glEnableVertexAttribArray(10);

	kas_glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, S_UI_STRIDE, (void *)S_NODE_RECT_OFFSET);
	kas_glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, S_UI_STRIDE, (void *)S_VISIBLE_RECT_OFFSET);
	kas_glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, S_UI_STRIDE, (void *)S_UV_RECT_OFFSET);
	kas_glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, S_UI_STRIDE, (void *)S_BACKGROUND_COLOR_OFFSET);
	kas_glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, S_UI_STRIDE, (void *)S_BORDER_COLOR_OFFSET);
	kas_glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, S_UI_STRIDE, (void *)S_SPRITE_COLOR_OFFSET);
	kas_glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, S_UI_STRIDE, (void *)S_EXTRA_OFFSET);
	kas_glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, S_UI_STRIDE, (void *)S_GRADIENT_COLOR_BR_OFFSET);
	kas_glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, S_UI_STRIDE, (void *)S_GRADIENT_COLOR_TR_OFFSET);
	kas_glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, S_UI_STRIDE, (void *)S_GRADIENT_COLOR_TL_OFFSET);
	kas_glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, S_UI_STRIDE, (void *)S_GRADIENT_COLOR_BL_OFFSET);

	kas_glVertexAttribDivisor(0, 1);
	kas_glVertexAttribDivisor(1, 1);
	kas_glVertexAttribDivisor(2, 1);
	kas_glVertexAttribDivisor(3, 1);
	kas_glVertexAttribDivisor(4, 1);
	kas_glVertexAttribDivisor(5, 1);
	kas_glVertexAttribDivisor(6, 1);
	kas_glVertexAttribDivisor(7, 1);
	kas_glVertexAttribDivisor(8, 1);
	kas_glVertexAttribDivisor(9, 1);
	kas_glVertexAttribDivisor(10, 1);
}

void r_ui_buffer_local_layout_setter(void)
{
}
