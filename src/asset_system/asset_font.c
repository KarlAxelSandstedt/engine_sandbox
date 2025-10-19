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

#include "asset_local.h"

#ifdef	KAS_DEV

#include "ft2build.h"
#include FT_FREETYPE_H

FT_Library g_ft_library = { 0 };

void internal_freetype_init(void)
{
	if (FT_Init_FreeType(&g_ft_library))
	{
		log_string(T_ASSET, S_FATAL, "Failed to initiate freetype2 library");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}
}

void internal_freetype_free(void)
{
	FT_Done_FreeType(g_ft_library);
}

typedef	struct font_glyph font_glyph;
DECLARE_STACK(font_glyph);
DEFINE_STACK(font_glyph);
void font_build(struct arena *mem, const enum font_id id)
{
	struct asset_font *asset = g_asset_db->font[id];

	arena_push_record(mem);

	FT_Face	face;
	const u32 face_index = 0;
	u32 error = FT_New_Face(g_ft_library, (const char *) asset->ttf->filepath, face_index, &face);
	if (error)
	{
		log_string(T_ASSET, S_FATAL, "Failed to initiate freetype face");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}

	error = FT_Set_Pixel_Sizes(face, 0, asset->pixel_glyph_height);
	if (error)
	{
		log_string(T_ASSET, S_FATAL, "Failed to set freetype pixel size");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}

	i32 total_glyph_width = 0;
	const u32 glyph_max_count = 4096;
	stack_ptr stack_pixels = stack_ptr_alloc(mem, glyph_max_count, !STACK_GROWABLE);
	stack_font_glyph stack_glyph = stack_font_glyph_alloc(mem, glyph_max_count, !STACK_GROWABLE);
	
	/* setup no_found glyph */
	//error = FT_Load_Char(face, 0, FT_LOAD_DEFAULT | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_NORMAL | FT_RENDER_MODE_NORMAL);
	error = FT_Load_Char(face, 0, FT_LOAD_DEFAULT | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_NORMAL | FT_RENDER_MODE_NORMAL);
	if (error)
	{
		log_string(T_ASSET, S_FATAL, "Failed to load not-found-glyph");
	}

	total_glyph_width += face->glyph->bitmap.width;
	u8 *pixels = arena_push_memcpy(mem, face->glyph->bitmap.buffer, face->glyph->bitmap.width * face->glyph->bitmap.rows);
	stack_ptr_push(&stack_pixels, pixels);
	const u32 glyph_unknown_index = stack_glyph.next;
	stack_font_glyph_push(&stack_glyph, (struct font_glyph)
	{
		.size = { (i32) face->glyph->bitmap.width, (i32) face->glyph->bitmap.rows },
		.bearing = { face->glyph->bitmap_left, face->glyph->bitmap_top },
		.advance = (i32) face->glyph->advance.x >> 6,
		.codepoint = 0,
	});

	/* load ascii, Latin-1 Suplement, Latin Extend A, Latin Extend B  */
	for (u32 c = 0x1; c <= 0x024f; ++c)
	{
		/* load embeded bitmap if found, otherwise load native image of glyph and 
		 * render anti-aliased coverage bitmap (8bit pixmap) */
		//error = FT_Load_Char(face, c, FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_NORMAL | FT_RENDER_MODE_NORMAL);
		error = FT_Load_Char(face, c, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL);
		if (error)
		{
			log_string(T_ASSET, S_ERROR, "Failed to load glyph");
			continue;
		}

		total_glyph_width += face->glyph->bitmap.width;
		pixels = arena_push_memcpy(mem, face->glyph->bitmap.buffer, face->glyph->bitmap.width * face->glyph->bitmap.rows);
		stack_ptr_push(&stack_pixels, pixels);
		stack_font_glyph_push(&stack_glyph, (struct font_glyph)
			{
				.size = { (i32) face->glyph->bitmap.width, (i32) face->glyph->bitmap.rows },
				.bearing = { face->glyph->bitmap_left, face->glyph->bitmap_top },
				.advance = (i32) face->glyph->advance.x >> 6,
				.codepoint = c,
			});
	}

	/* Greek and Coptic */
	for (u32 c = 0x0370; c <= 0x03ff; ++c)
	{
		/* load embeded bitmap if found, otherwise load native image of glyph and 
		 * render anti-aliased coverage bitmap (8bit pixmap) */
		//error = FT_Load_Char(face, c, FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_NORMAL | FT_RENDER_MODE_NORMAL);
		error = FT_Load_Char(face, c, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL | FT_RENDER_MODE_NORMAL);
		if (error)
		{
			log_string(T_ASSET, S_ERROR, "Failed to load glyph");
			continue;
		}

		total_glyph_width += face->glyph->bitmap.width;
		pixels = arena_push_memcpy(mem, face->glyph->bitmap.buffer, face->glyph->bitmap.width * face->glyph->bitmap.rows);
		stack_ptr_push(&stack_pixels, pixels);
		stack_font_glyph_push(&stack_glyph, (struct font_glyph)
			{
				.size = { (i32) face->glyph->bitmap.width, (i32) face->glyph->bitmap.rows },
				.bearing = { face->glyph->bitmap_left, face->glyph->bitmap_top },
				.advance = (i32) face->glyph->advance.x >> 6,
				.codepoint = c,
			});
	}

	const u32 hash_len = (u32) power_of_two_ceil(stack_glyph.next);
	struct font *font = arena_push(mem, sizeof(struct font));
	font->codepoint_to_glyph_map = hash_map_alloc(mem, hash_len, hash_len, !HASH_GROWABLE);
	font->glyph_count = stack_glyph.next;
	font->glyph_unknown_index = glyph_unknown_index;
	font->glyph = stack_glyph.arr;

	font->ascent = (f32) face->size->metrics.ascender / 64;
	font->descent = (face->size->metrics.descender > 0.0f)
			? (f32) -face->size->metrics.descender / 64
			: (f32) face->size->metrics.descender / 64;
	font->linespace = (f32) face->size->metrics.height / 64;

	font->size = 0;
	font->pixmap_width = (u32) power_of_two_ceil(asset->pixel_glyph_height);
	font->pixmap_height = 0;
	font->pixmap = NULL;

	/* find mininum square fitting all bitmaps */
	while (1)
	{
		/* rows needed if we allowed partial fitting of bitmaps; */
		const u32 clipped_rows_required = (total_glyph_width % font->pixmap_width)
				? 1 + (total_glyph_width / font->pixmap_width)
				: (total_glyph_width / font->pixmap_width);

		const u32 total_glyph_width_padded = total_glyph_width + clipped_rows_required*asset->pixel_glyph_height;
		/* for every clipped row, pad an additional glyph to make sure we get a correct upper bound */
		const u32 rows_required = (total_glyph_width_padded % font->pixmap_width)
				? 1 + (total_glyph_width_padded / font->pixmap_width)
				: (total_glyph_width_padded / font->pixmap_width);

		const u32 pixmap_height_required = rows_required * asset->pixel_glyph_height;
		if (pixmap_height_required <= font->pixmap_width)
		{
			 break;
		}
		font->pixmap_width *= 2;
	}

	font->pixmap_height = font->pixmap_width;
	font->pixmap = arena_push(mem, font->pixmap_width * font->pixmap_height);
	font->size = sizeof(u64) + 3*sizeof(f32) + 4*sizeof(u32)
		+ stack_glyph.next * (2*sizeof(vec2i32) + 2*sizeof(u32) + 2*sizeof(vec2))
		+ sizeof(u32) + hash_len*sizeof(u32)
		+ sizeof(u32) + hash_len*sizeof(u32)
		+ font->pixmap_width * font->pixmap_height;
	memset(font->pixmap, 0, font->pixmap_width * font->pixmap_height);

	const f32 pixel_halfsize = 1.0f / (2.0f*font->pixmap_width);
	vec2u32 offset = { 0, 0 };
	for (u32 i = 0; i < stack_glyph.next; ++i)
	{
		u8 *alpha = font->pixmap;

		struct font_glyph *g = stack_glyph.arr + i;
		hash_map_add(font->codepoint_to_glyph_map, g->codepoint, i);
		pixels = stack_pixels.arr[i];
		if (offset[0] + g->size[0] > font->pixmap_width)
		{
			offset[0] = 0;		
			offset[1] += asset->pixel_glyph_height;
		}
		kas_assert(offset[1] + g->size[1] <= font->pixmap_height);

		for (i32 y = 0; y < g->size[1]; ++y)
		{
			for (i32 x = 0; x < g->size[0]; ++x)
			{
				kas_assert(offset[1] + y < font->pixmap_height);
				kas_assert(offset[0] + x < font->pixmap_width);
				alpha[(offset[1] + g->size[1] - 1 - y)*font->pixmap_width + (offset[0] + x)] = pixels[y*g->size[0] + x];
			}
		}

		g->bl[0] = 2.0f*offset[0] * pixel_halfsize; 
		g->tr[0] = 2.0f*(offset[0] + g->size[0]) * pixel_halfsize; 
		g->bl[1] = 2.0f*offset[1] * pixel_halfsize; 
		g->tr[1] = 2.0f*(offset[1] + g->size[1]) * pixel_halfsize; 

		offset[0] += g->size[0];
	}

	if (FT_HAS_KERNING(face))
	{
		kas_assert_string(0, "Font supports kerning, but we do not!\n");
	}

	font_serialize(asset, font);

	FT_Done_Face(face);
	arena_pop_record(mem);
}

void font_serialize(const struct asset_font *asset, const struct font *font)
{
	struct arena tmp = arena_alloc_1MB();

	struct file file = file_null();
	if (file_try_create_at_cwd(&tmp, &file, asset->filepath, FILE_TRUNCATE) != FS_SUCCESS)
	{
		log_string(T_ASSET, S_FATAL, "Failed to create .font file");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}

	file_set_size(&file, font->size);
	void *buf = file_memory_map_partial(&file, font->size, 0, FS_PROT_READ | FS_PROT_WRITE, FS_MAP_SHARED);
	struct serialize_stream ss = ss_buffered(buf, font->size);

	ss_write_u64_be(&ss, font->size);
	ss_write_f32_be(&ss, font->ascent);
	ss_write_f32_be(&ss, font->descent);
	ss_write_f32_be(&ss, font->linespace);
	ss_write_u32_be(&ss, font->pixmap_width);
	ss_write_u32_be(&ss, font->pixmap_height);
	ss_write_u32_be(&ss, font->glyph_unknown_index);
	ss_write_u32_be(&ss, font->glyph_count);

	for (u32 i = 0; i < font->glyph_count; ++i)
	{
		ss_write_i32_be(&ss, font->glyph[i].size[0]);	
		ss_write_i32_be(&ss, font->glyph[i].size[1]);	
		ss_write_i32_be(&ss, font->glyph[i].bearing[0]);	
		ss_write_i32_be(&ss, font->glyph[i].bearing[1]);	
		ss_write_u32_be(&ss, font->glyph[i].advance);	
		ss_write_u32_be(&ss, font->glyph[i].codepoint);	
		ss_write_f32_be(&ss, font->glyph[i].bl[0]);
		ss_write_f32_be(&ss, font->glyph[i].bl[1]);
		ss_write_f32_be(&ss, font->glyph[i].tr[0]);
		ss_write_f32_be(&ss, font->glyph[i].tr[1]);
	}

	hash_map_serialize(&ss, font->codepoint_to_glyph_map);
	ss_write_u8_array(&ss, font->pixmap, font->pixmap_height * font->pixmap_width);

	file_memory_unmap(buf, font->size);
	file_close(&file);

	arena_free_1MB(&tmp);
}

#endif

const struct font *font_deserialize(struct asset_font *asset)
{
	//TODO remove later;
	struct arena tmp = arena_alloc_1MB();
	struct file file = file_null();
	file_try_open_at_cwd(&tmp, &file, asset->filepath, FILE_READ);
	if (file.handle == FILE_HANDLE_INVALID)
	{
		return NULL;
	}

	u64 size = 0;
	void *buf = file_memory_map(&size, &file, FS_PROT_READ, FS_MAP_SHARED);
	struct serialize_stream ss = ss_buffered(buf, size);

	if (ss_bytes_left(&ss) < 8)
	{
		return NULL;
	}

	struct font *font = malloc(sizeof(struct font));
	font->size = ss_read_u64_be(&ss);

	if (ss_bytes_left(&ss) < font->size-8)
	{
		free(font);
		return NULL;
	}

	font->ascent = ss_read_f32_be(&ss);
	font->descent = ss_read_f32_be(&ss);
	font->linespace = ss_read_f32_be(&ss);
	font->pixmap_width = ss_read_u32_be(&ss);
	font->pixmap_height = ss_read_u32_be(&ss);
	font->glyph_unknown_index = ss_read_u32_be(&ss);
	font->glyph_count = ss_read_u32_be(&ss);
	font->glyph = malloc(font->glyph_count * sizeof(struct font_glyph));
	font->pixmap = malloc(font->pixmap_width * font->pixmap_height);

	for (u32 i = 0; i < font->glyph_count; ++i)
	{
		font->glyph[i].size[0] = ss_read_i32_be(&ss);
		font->glyph[i].size[1] = ss_read_i32_be(&ss);
		font->glyph[i].bearing[0] = ss_read_i32_be(&ss);
		font->glyph[i].bearing[1] = ss_read_i32_be(&ss);
		font->glyph[i].advance = ss_read_u32_be(&ss);
		font->glyph[i].codepoint = ss_read_u32_be(&ss);
		font->glyph[i].bl[0] = ss_read_f32_be(&ss);
		font->glyph[i].bl[1] = ss_read_f32_be(&ss);
		font->glyph[i].tr[0] = ss_read_f32_be(&ss);
		font->glyph[i].tr[1] = ss_read_f32_be(&ss);
	}

	font->codepoint_to_glyph_map = hash_map_deserialize(NULL, &ss, 0);
	ss_read_u8_array(font->pixmap, &ss, font->pixmap_height * font->pixmap_width);

	file_memory_unmap(buf, size);
	file_close(&file);

	arena_free_1MB(&tmp);
	asset->loaded = 1;
	return font;
}

void font_debug_print(FILE *out, const struct font *font)
{
	fprintf(out, "font:\n{\n");
	fprintf(out, "\tpixmap_width: %u\n", font->pixmap_width);
	fprintf(out, "\tpixmap_height: %u\n", font->pixmap_height);
	fprintf(out, "\tglyph_count: %u\n", font->glyph_count);

	for (u32 i = 0; i < font->glyph_count; ++i)
	{
		fprintf(out, "\tglyph[%u]:\n", i);
		fprintf(out, "\t{\n");
		fprintf(out, "\t\tsize:    { %i, %i }\n", font->glyph[i].size[0], font->glyph[i].size[1]);
		fprintf(out, "\t\tbearing: { %i, %i }\n", font->glyph[i].bearing[0], font->glyph[i].bearing[1]);
		fprintf(out, "\t\tbl: 	   { %f, %f }\n", font->glyph[i].bl[0], font->glyph[i].bl[1]);
		fprintf(out, "\t\ttr: 	   { %f, %f }\n", font->glyph[i].tr[0], font->glyph[i].tr[1]);
		fprintf(out, "\t\tadvance:   %u\n", font->glyph[i].advance);
		fprintf(out, "\t\tcodepoint: %u\n", font->glyph[i].codepoint);
		fprintf(out, "\t}\n");
	}
	fprintf(out, "}\n");
}

struct asset_font *asset_database_request_font(struct arena *tmp, const enum font_id id)
{
	arena_push_record(tmp);
	struct asset_font *asset = g_asset_db->font[id];
#ifdef KAS_DEV
	if (!asset->valid)
	{
		if (asset->loaded)
		{
			free(asset->font->pixmap);
			free(asset->font->glyph);
			free((void *) asset->font);
		}

		font_build(tmp, id);
		asset->valid = 1;
		asset->loaded = 0;
	}
#endif
	if (!asset->loaded)
	{
		asset->font = font_deserialize(asset);
		//font_debug_print(stderr, asset->font);
	}

	arena_pop_record(tmp);

	return asset;
}

const struct font_glyph *glyph_lookup(const struct font *font, const u32 codepoint)
{
	const struct font_glyph *g;
	u32 index = hash_map_first(font->codepoint_to_glyph_map, codepoint);	
	for (; index != HASH_NULL; index = hash_map_next(font->codepoint_to_glyph_map, index))
	{
		g = font->glyph + index;
		if (g->codepoint == codepoint)
		{
			break;
		} 
	}

	if (index == HASH_NULL)
	{
		g = font->glyph + font->glyph_unknown_index;	
	}

	return g;
}
