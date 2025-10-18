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

#include "sdl3_wrapper_local.h"

u32 sdl3_wrapper_key_modifiers(void)
{
	const u32 sdl_modifiers = SDL_GetModState();
	return (KEY_MOD_LSHIFT * !!(sdl_modifiers & SDL_KMOD_LSHIFT))
		| (KEY_MOD_RSHIFT * !!(sdl_modifiers & SDL_KMOD_RSHIFT))
		| (KEY_MOD_LCTRL  * !!(sdl_modifiers & SDL_KMOD_LCTRL))
		| (KEY_MOD_RCTRL  * !!(sdl_modifiers & SDL_KMOD_RCTRL))
		| (KEY_MOD_LALT   * !!(sdl_modifiers & SDL_KMOD_LALT))
		| (KEY_MOD_RALT   * !!(sdl_modifiers & SDL_KMOD_RALT))
		| (KEY_MOD_LGUI   * !!(sdl_modifiers & SDL_KMOD_LGUI))
		| (KEY_MOD_RGUI   * !!(sdl_modifiers & SDL_KMOD_RGUI))
		| (KEY_MOD_NUM    * !!(sdl_modifiers & SDL_KMOD_NUM))
		| (KEY_MOD_CAPS   * !!(sdl_modifiers & SDL_KMOD_CAPS))
		| (KEY_MOD_ALTGR  * !!(sdl_modifiers & SDL_KMOD_MODE))
		| (KEY_MOD_SCROLL * !!(sdl_modifiers & SDL_KMOD_SCROLL));
}

enum mouse_button sdl3_wrapper_to_system_mouse_button(const u8 mouse_button)
{
	enum mouse_button button = MOUSE_BUTTON_NONMAPPED;
	switch (mouse_button)
	{
		case SDL_BUTTON_LEFT: { button = MOUSE_BUTTON_LEFT; } break;
		case SDL_BUTTON_RIGHT: { button = MOUSE_BUTTON_RIGHT; } break;
		case SDL_BUTTON_MIDDLE: { button = MOUSE_BUTTON_SCROLL; } break;
	}

	return button;
}

enum kas_keycode sdl3_wrapper_to_system_keycode(const SDL_Keycode sdl_key)
{
	enum kas_keycode key = KAS_NO_SYMBOL;

	if (SDLK_0 <= sdl_key && sdl_key <= SDLK_9)
	{
		key = KAS_0 + (sdl_key - SDLK_0);
	}
	else if (SDLK_A <= sdl_key && sdl_key <= SDLK_Z)
	{
		key = KAS_A + (sdl_key - SDLK_A);
	}
	else if (SDLK_F1 <= sdl_key && sdl_key <= SDLK_F12)
	{
		key = KAS_F1 + (sdl_key - SDLK_F1);
	}
	else if (sdl_key == SDLK_LSHIFT) { key = KAS_SHIFT; }
	else if (sdl_key == SDLK_SPACE) { key = KAS_SPACE; }
	else if (sdl_key == SDLK_BACKSPACE) { key = KAS_BACKSPACE; }
	else if (sdl_key == SDLK_ESCAPE) { key = KAS_ESCAPE; }
	else if (sdl_key == SDLK_TAB) { key = KAS_TAB; }
	else if (sdl_key == SDLK_RETURN) { key = KAS_ENTER; }
	else if (sdl_key == SDLK_LCTRL) { key = KAS_CTRL; }
	else if (sdl_key == SDLK_DELETE) { key = KAS_DELETE; }
	else if (sdl_key == SDLK_HOME) { key = KAS_HOME; }
	else if (sdl_key == SDLK_UP) { key = KAS_UP; }
	else if (sdl_key == SDLK_DOWN) { key = KAS_DOWN; }
	else if (sdl_key == SDLK_LEFT) { key = KAS_LEFT; }
	else if (sdl_key == SDLK_RIGHT) { key = KAS_RIGHT; }
	else if (sdl_key == SDL_SCANCODE_END) { key = KAS_END; }

	return key;
}

enum kas_keycode sdl3_wrapper_to_system_scancode(const SDL_Scancode sdl_key)
{
	enum kas_keycode key = KAS_NO_SYMBOL;

	if (SDL_SCANCODE_0 <= sdl_key && sdl_key <= SDL_SCANCODE_9)
	{
		key = KAS_0 + (sdl_key - SDL_SCANCODE_0);
	}
	else if (SDL_SCANCODE_A <= sdl_key && sdl_key <= SDL_SCANCODE_Z)
	{
		key = KAS_A + (sdl_key - SDL_SCANCODE_A);
	}
	else if (SDL_SCANCODE_F1 <= sdl_key && sdl_key <= SDL_SCANCODE_F12)
	{
		key = KAS_F1 + (sdl_key - SDL_SCANCODE_F1);
	}
	else if (sdl_key == SDL_SCANCODE_LSHIFT) { key = KAS_SHIFT; }
	else if (sdl_key == SDL_SCANCODE_SPACE) { key = KAS_SPACE; }
	else if (sdl_key == SDL_SCANCODE_BACKSPACE) { key = KAS_BACKSPACE; }
	else if (sdl_key == SDL_SCANCODE_ESCAPE) { key = KAS_ESCAPE; }
	else if (sdl_key == SDL_SCANCODE_TAB) { key = KAS_TAB; }
	else if (sdl_key == SDL_SCANCODE_RETURN) { key = KAS_ENTER; }
	else if (sdl_key == SDL_SCANCODE_LCTRL) { key = KAS_CTRL; }
	else if (sdl_key == SDL_SCANCODE_DELETE) { key = KAS_DELETE; }
	else if (sdl_key == SDL_SCANCODE_HOME) { key = KAS_HOME; }
	else if (sdl_key == SDL_SCANCODE_UP) { key = KAS_UP; }
	else if (sdl_key == SDL_SCANCODE_DOWN) { key = KAS_DOWN; }
	else if (sdl_key == SDL_SCANCODE_LEFT) { key = KAS_LEFT; }
	else if (sdl_key == SDL_SCANCODE_RIGHT) { key = KAS_RIGHT; }
	else if (sdl_key == SDL_SCANCODE_END) { key = KAS_END; }

	return key;
}
