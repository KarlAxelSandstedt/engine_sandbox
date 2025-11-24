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

#include "kas_common.h"
#include "sys_public.h"
#include "sys_local.h"
#include "kas_string.h"

const char *kas_button_string_map[] =
{
	"MOUSE_BUTTON_LEFT",
	"MOUSE_BUTTON_RIGHT",
	"MOUSE_BUTTON_SCROLL",
	"MOUSE_BUTTON_NONMAPPED",
	"MOUSE_BUTTON_COUNT",
};

const char *kas_keycode_string_map[] = 
{
	"KAS_SHIFT",
	"KAS_CTRL",
	"KAS_SPACE",
	"KAS_BACKSPACE",
	"KAS_ESCAPE",
	"KAS_ENTER",
	"KAS_PLUS",
	"KAS_MINUS",
	"KAS_F1",
	"KAS_F2",
	"KAS_F3",
	"KAS_F4",
	"KAS_F5",
	"KAS_F6",
	"KAS_F7",
	"KAS_F8",
	"KAS_F9",
	"KAS_F10",
	"KAS_F11",
	"KAS_F12",
	"KAS_TAB",
	"KAS_UP",
	"KAS_DOWN",
	"KAS_LEFT",
	"KAS_RIGHT",
	"KAS_DELETE",
	"KAS_HOME",
	"KAS_END",
	"KAS_0",
	"KAS_1",
	"KAS_2",
	"KAS_3",
	"KAS_4",
	"KAS_5",
	"KAS_6",
	"KAS_7",
	"KAS_8",
	"KAS_9",
	"KAS_A",
	"KAS_B", 
	"KAS_C", 
	"KAS_D", 
	"KAS_E", 
	"KAS_F", 
	"KAS_G", 
	"KAS_H", 
	"KAS_I", 
	"KAS_J", 
	"KAS_K", 
	"KAS_L", 
	"KAS_M", 
	"KAS_N", 
	"KAS_O", 
	"KAS_P", 
	"KAS_Q", 
	"KAS_R", 
	"KAS_S", 
	"KAS_T", 
	"KAS_U", 
	"KAS_V", 
	"KAS_W", 
	"KAS_X", 
	"KAS_Y", 
	"KAS_Z", 
	"KAS_NO_SYMBOL",
	"KAS_KEY_COUN"
};

const char *kas_keycode_to_string(const enum kas_keycode key)
{
	return kas_keycode_string_map[key];
}

const char *kas_button_to_string(const enum mouse_button button)
{
	return kas_button_string_map[button];
}
