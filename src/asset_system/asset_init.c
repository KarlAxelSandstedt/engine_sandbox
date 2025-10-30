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
#include "log.h"

struct sprite sprite_storage[SPRITE_COUNT];
struct sprite *g_sprite = sprite_storage;

#ifdef KAS_DEV

static struct asset_png led_png_arr[] =
{
	{
		.filepath = "../asset_components/textures/led_sprite_sheet.png",
		.width = 0,
		.height = 0,
		.sprite_width = 64,
		.valid = 0,
		.handle = FILE_HANDLE_INVALID,
	},
};

static struct asset_png dynamic_png_arr[] =
{
	{
		.filepath = "../asset_components/textures/sorcerer_hero_sprite_sheet.png",
		.width = 0,
		.height = 0,
		.sprite_width = 64,
		.valid = 0,
		.handle = FILE_HANDLE_INVALID,
	},
};

static struct asset_ttf hack_regular_ttf =
{
	.filepath = "../asset_components/ttf/Hack-Regular.ttf",
	.valid = 0,
	.handle = FILE_HANDLE_INVALID,
};

#endif

static u8 none_ssff_pixel[4] = { 0, 0, 0, 0 };
static struct asset_ssff none_ssff = 
{ 
	.filepath = "",
	.loaded = 1,
	.ssff = NULL,
	.width = 1,
	.height = 1,
	.pixel = none_ssff_pixel,
	.sprite_info = &sprite_storage[SPRITE_NONE],
	.count = 1,
	.texture_id = TEXTURE_NONE,
#ifdef KAS_DEV
	.valid = 1,
#endif
};

static struct asset_ssff led_ssff =
{
	.filepath = "../assets/sprites/led.ssff",
	.texture_id = TEXTURE_LED,
	.loaded = 0,
	.ssff = NULL,
	.pixel = NULL,
	.sprite_info = NULL,
	.count = 0,
	.width = 512,
	.height = 512,
#ifdef KAS_DEV
	.valid = 0,
	.png_count = sizeof(led_png_arr) / sizeof(struct asset_png),
	.png = led_png_arr,
#endif
};

static struct asset_ssff dynamic_ssff =
{
	.filepath = "../assets/sprites/dynamic.ssff",
	.texture_id = TEXTURE_DYNAMIC,
	.loaded = 0,
	.ssff = NULL,
	.pixel = NULL,
	.sprite_info = NULL,
	.count = 0,
	.width = 512,
	.height = 512,
#ifdef KAS_DEV
	.valid = 0,
	.png_count = sizeof(dynamic_png_arr) / sizeof(struct asset_png),
	.png = dynamic_png_arr,
#endif
};

static struct asset_font default_font_small =
{
	.filepath = "../assets/fonts/default_small.kasfnt",
	.loaded = 0,
	.font = NULL,
	.pixel_glyph_height = 14,
	//.glyph_height = 20,
	//.dpi_x = 96,
	//.dpi_y = 96,
	.texture_id = TEXTURE_FONT_DEFAULT_SMALL,
#ifdef KAS_DEV
	.valid = 0,
	.ttf = &hack_regular_ttf,
#endif
};

static struct asset_font default_font_medium =
{
	.filepath = "../assets/fonts/default_medium.kasfnt",
	.loaded = 0,
	.font = NULL,
	.pixel_glyph_height = 20,
	//.glyph_height = 32,
	//.dpi_x = 96,
	//.dpi_y = 96,
	.texture_id = TEXTURE_FONT_DEFAULT_MEDIUM,
#ifdef KAS_DEV
	.valid = 0,
	.ttf = &hack_regular_ttf,
#endif
};

void dynamic_ssff_set_sprite_parameters(struct asset_ssff *dynamic_ssff, const struct ssff_texture_return *param)
{
	dynamic_ssff->pixel = param->pixel;
	dynamic_ssff->count = param->count;
	dynamic_ssff->sprite_info = param->sprite;

	u32 count = 0;
	g_sprite[SPRITE_SORCERER_IDLE_1] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_IDLE_1].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_IDLE_2] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_IDLE_2].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_CAST_TRANSITION_1] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_CAST_TRANSITION_1].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_STAND_CAST_1] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_STAND_CAST_2].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_STAND_CAST_2] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_STAND_CAST_2].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_STAND_CAST_3] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_STAND_CAST_3].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_STAND_CAST_4] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_STAND_CAST_4].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_STAND_CAST_5] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_STAND_CAST_5].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_WALK_CAST_1] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_WALK_CAST_1].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_WALK_CAST_2] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_WALK_CAST_2].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_WALK_CAST_3] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_WALK_CAST_3].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_WALK_CAST_4] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_WALK_CAST_4].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_WALK_CAST_5] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_WALK_CAST_5].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_RUN_CAST_1] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_RUN_CAST_1].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_RUN_CAST_2] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_RUN_CAST_2].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_RUN_CAST_3] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_RUN_CAST_3].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_RUN_CAST_4] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_RUN_CAST_4].ssff_id = SSFF_DYNAMIC_ID;
	g_sprite[SPRITE_SORCERER_RUN_CAST_5] = param->sprite[count++];
	g_sprite[SPRITE_SORCERER_RUN_CAST_5].ssff_id = SSFF_DYNAMIC_ID;
	kas_assert_string(count == param->count, "unexpected sprite count in dynamic sprite sheet, or in hardcoded values");
}

void led_ssff_set_sprite_parameters(struct asset_ssff *led_ssff, const struct ssff_texture_return *param)
{
	led_ssff->pixel = param->pixel;
	led_ssff->count = param->count;
	led_ssff->sprite_info = param->sprite;

	u32 count = 0;
	g_sprite[SPRITE_LED_REFRESH_BUTTON] = param->sprite[count++];
	g_sprite[SPRITE_LED_REFRESH_BUTTON].ssff_id = SSFF_LED_ID;
	g_sprite[SPRITE_LED_REFRESH_BUTTON_HIGHLIGHT] = param->sprite[count++];
	g_sprite[SPRITE_LED_REFRESH_BUTTON_HIGHLIGHT].ssff_id = SSFF_LED_ID;
	g_sprite[SPRITE_LED_REFRESH_BUTTON_PRESSED] = param->sprite[count++];
	g_sprite[SPRITE_LED_REFRESH_BUTTON_PRESSED].ssff_id = SSFF_LED_ID;
	g_sprite[SPRITE_LED_FOLDER] = param->sprite[count++];
	g_sprite[SPRITE_LED_FOLDER].ssff_id = SSFF_LED_ID;
	g_sprite[SPRITE_LED_FILE] = param->sprite[count++];
	g_sprite[SPRITE_LED_FILE].ssff_id = SSFF_LED_ID;
	g_sprite[SPRITE_LED_PLAY] = param->sprite[count++];
	g_sprite[SPRITE_LED_PLAY].ssff_id = SSFF_LED_ID;
	g_sprite[SPRITE_LED_PAUSE] = param->sprite[count++];
	g_sprite[SPRITE_LED_PAUSE].ssff_id = SSFF_LED_ID;
	g_sprite[SPRITE_LED_STOP] = param->sprite[count++];
	g_sprite[SPRITE_LED_STOP].ssff_id = SSFF_LED_ID;
	kas_assert_string(count == param->count, "unexpected sprite count in level editor sprite sheet, or in hardcoded values");
}

static struct asset_ssff **internal_asset_ssff_array_init(struct arena *mem_persistent)
{
	struct asset_ssff **ssff = arena_push(mem_persistent, SSFF_COUNT * sizeof(struct asset_ssff *));
	if (ssff == NULL)
	{
		log_string(T_ASSET, S_FATAL, "Failed to alloc asset ssff array");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}

	ssff[SSFF_NONE_ID] = &none_ssff;
	ssff[SSFF_DYNAMIC_ID] = &dynamic_ssff;
	ssff[SSFF_LED_ID] = &led_ssff;

	return ssff;
}

static struct asset_font **internal_asset_font_array_init(struct arena *mem_persistent)
{
	struct asset_font **font = arena_push(mem_persistent, SSFF_COUNT * sizeof(struct asset_font *));
	if (font == NULL)
	{
		log_string(T_ASSET, S_FATAL, "Failed to alloc asset font array");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}

	font[FONT_NONE] = NULL, 
	font[FONT_DEFAULT_SMALL] = &default_font_small;
	font[FONT_DEFAULT_MEDIUM] = &default_font_medium;

	return font;
}

static struct asset_database storage = { 0 };
struct asset_database *g_asset_db = &storage;

void asset_database_init(struct arena *mem_persistent)
{
	g_sprite[SPRITE_NONE].ssff_id = SSFF_NONE_ID;
	vec2u32_set(g_sprite[SPRITE_NONE].pixel_size, 1, 1);
	vec2_set(g_sprite[SPRITE_NONE].bl, 0.0f, 0.0f);
	vec2_set(g_sprite[SPRITE_NONE].tr, 0.0f, 0.0f);

	g_asset_db->ssff = internal_asset_ssff_array_init(mem_persistent);
	g_asset_db->font = internal_asset_font_array_init(mem_persistent);

#if	KAS_DEV
	internal_freetype_init();
#endif
}

void asset_database_cleanup(void)
{
#if	KAS_DEV
	internal_freetype_free();
#endif
}
