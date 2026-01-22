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

#ifndef __ASSET_LOCAL_H__
#define __ASSET_LOCAL_H__

#include "asset_public.h"
#include "sys_public.h"
#include "serialize.h"

/***************************** SPIRTE SHEET FILE FORMAT *****************************/

/*
 * Sprite Sheet File Format (.ssff): Fully compact, no padding 
 *
 * 	ssff_header
 * 	collection[0]
 * 	...
 * 	collection[N1]
 * 	color_table[0]
 * 	collection[0].sprite[0]
 * 	...
 * 	collection[0].sprite[collection[0].color_count-1]
 * 	...
 * 	...
 * 	...
 * 	...
 * 	color_table[N1]
 * 	collection[N1].sprite[0]
 * 	...
 * 	collection[N1].sprite[collection[N1].color_count-1];
 * 	pixel_data[]	
 */

/*
 * ssff_header: .ssff header. The mapping between sprite collections <-> ssff file is immutable.
 * 	  Furthermore, the local spirte ordering of each sprite collection in the file is immutable.
 */
struct ssff_header
{
	u64 		size;			/* sizeof(ssff) + sizeof(data[]) */
	u32		collection_count;	/* number of collections */
	u32		collection_offset;	/* file offest to collection[collection_count] */
	u8		data[];
};

/*
 * ssff_collection: collection of local sprites, think sorcererHero. Each pixel usages bit_depth number of bits. The
 * 	bits index into the collections local color table to determine rgba color. 
 */
struct ssff_collection
{
	u32		color_count;		/* number of colors used in collection */
	u32		color_offset;		/* file offset to color[color_count] */
	u32		bit_depth;		/* number of bits per pixel */
	u32 		sprite_count;		/* sprite_count */
	u32		sprite_offset;		/* sprite file offset */
	u32		width;			/* sprite width sum  */
	u32		height;			/* max sprite height */
};

/*
 * ssff_sprite - local sprite within a ssff_collection; indexable according to the ssff_collection's hardcoded
 * 	identififer. For example, collection[sorcerer_collection_id].sprite[SORCERER_WALK_1]. pixel coordinates
 * 	follows the following rule: x0 < x1, y0 < y1 and 
 *
 * 	(x0,y0) --------------------------- (x1, y0) 
 * 	   |					|
 * 	   |					|
 * 	   |					|
 * 	   |					|
 * 	   |					|
 * 	(x0,y1) --------------------------- (x1, y1) 
 */
struct ssff_sprite
{
	u32 	x0;		
	u32 	x1;
	u32 	y0;
	u32 	y1;
	u32	pixel_offset;		/* file offest to pixel data, stored left-right, top-down */
};

struct ssff_texture_return
{
	void *		pixel;	/* pixel opengl texture data 				*/
	struct sprite *	sprite;	/* sprite information is order of sprite generation 	*/
	u32 		count;	/* uv[count] 						*/
};

#ifdef	DS_DEV
/* build a ssff file header and save it to disk. replace clip color with { 0, 0, 0, 0 } color */
void				ssff_build(struct arena *mem, const u32 ssff_id);
/* save ssff to disk  */
void 				ssff_save(const struct asset_ssff *asset, const struct ssff_header *ssff);
#endif
/* heap allocate and load ssff from disk on success, return NULL on failure */
const struct ssff_header *	ssff_load(struct asset_ssff *asset);
/* heap allocate and construct texture with given width and height from ssff data. push, in order of generation, texture coordinates onto arena, and return values. */
struct ssff_texture_return 	ssff_texture(struct arena *mem, const struct ssff_header *ssff, const u32 width, const u32 height);
/* verbosely print ssff contents */
void				ssff_debug_print(FILE *out, const struct ssff_header *ssff);

/***************************** asset_font.c *****************************/

/* font file format:
	
	{
		size			: u64 (be)  // size of header + data[] 
		ascent			: f32 (be)
		descent         	: f32 (be)
		linespace       	: f32 (be)
		pixmap_width		: u32 (be)
		pixmap_height		: u32 (be)
		glyph_unknown_index  	: u32 (be)
		glyph_count 		: u32 (be)
	}	
	glyph[glyph_count] 
	{
		vec2i32	size;		; i32 (be)	
		vec2i32	bearing;	; i32 (be)	
		u32	advance;	; u32 (be)
		u32	codepoint;	; u32 (be)
		vec2	bl;		; f32 (be)
		vec2	tr;		; f32 (be)

	}

	codepoint_to_glyph_map		;  [serialized];

	pixmap[width*height]		; u8		// bl -> tp pixel sequence
 */

#ifdef	DS_DEV
/* initalize freetype library resources */
void				internal_freetype_init(void);
/* release freetype library resources */
void				internal_freetype_free(void);
/* build a font file header and save it to disk. */
void				font_build(struct arena *mem, const u32 font_id);
/* save font to disk  */
void 				font_serialize(const struct asset_font *asset, const struct font *font);
#endif
/* heap allocate and load font from disk on success, return NULL on failure */
const struct font *		font_deserialize(struct asset_font *asset);
/* debug print .kasfnt file to console */
void 				font_debug_print(FILE *out, const struct font *font);

/***************************** asset_init.c *****************************/

/* set parameters of hardcoded order of sprites in ssff */
void 	dynamic_ssff_set_sprite_parameters(struct asset_ssff *dynamic_ssff, const struct ssff_texture_return *param);
void 	led_ssff_set_sprite_parameters(struct asset_ssff *led_ssff, const struct ssff_texture_return *param);

#endif
