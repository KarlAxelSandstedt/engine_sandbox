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

#ifndef __SYSTEM_DEFINITION_H__
#define __SYSTEM_DEFINITION_H__

#include "kas_common.h"
#include "kas_string.h"
#include "kas_vector.h"
#include "list.h"

/* MACROS, GLOBALS and SYSTEM-LEVEL STRUCTS */
#if __OS__ == __LINUX__
#include "linux_public.h"
#elif __OS__ == __WEB__
#include "wasm_public.h"
#elif __OS__ == __WIN64__
#include "win_public.h"
#endif


/************************************************************************/
/* 				System Architecture			*/
/************************************************************************/

enum arch_type
{
	ARCH_INTEL64,
	ARCH_AMD64,
};

struct kas_arch_config
{
	utf8 	vendor_string;
	utf8	processor_string;

	enum arch_type 	type;
	u32		logical_core_count;

	u64		pagesize;	/* bytes */
	u64		cacheline;	/* bytes */

	/* cpuid flags */
	u32		sse : 1;
	u32		sse2 : 1;
	u32		sse3 : 1;
	u32		ssse3 : 1;
	u32		sse4_1 : 1;
	u32		sse4_2 : 1;
	u32		avx : 1;
	u32		avx2 : 1;
	u32		bmi1 : 1;	/* bit manipulation instructions (ctz, ctzl, ...) */

	u32		rdtsc : 1;	/* profiling timer support  */
	u32		rdtscp : 1;	/* profiling timer support  */
	u32		tsc_invariant : 1;  /* tsc works as a wallclock timer, always ticking and at same frequency */
};
extern const struct kas_arch_config *g_arch_config;

/************************************************************************/
/* 			system mouse/keyboard handling 			*/
/************************************************************************/

#define	KEY_MOD_NONE	0
#define	KEY_MOD_LSHIFT	(1 << 0)
#define	KEY_MOD_RSHIFT	(1 << 1)
#define	KEY_MOD_LCTRL	(1 << 2)
#define	KEY_MOD_RCTRL	(1 << 3)
#define	KEY_MOD_LALT	(1 << 4)
#define	KEY_MOD_RALT	(1 << 5)	/* Alt Gr? 		*/
#define	KEY_MOD_LGUI	(1 << 6)	/* Left windows-key?	*/
#define	KEY_MOD_RGUI	(1 << 7)	/* Right windows-key? 	*/
#define KEY_MOD_NUM	(1 << 8)	/* Num-lock  		*/ 
#define KEY_MOD_CAPS	(1 << 9)	
#define KEY_MOD_ALTGR	(1 << 10)	
#define KEY_MOD_SCROLL	(1 << 11)	/* Scroll-lock */

#define KEY_MOD_SHIFT	(KEY_MOD_LSHIFT | KEY_MOD_RSHIFT)
#define KEY_MOD_CTRL	(KEY_MOD_LCTRL | KEY_MOD_RCTRL)
#define KEY_MOD_ALT	(KEY_MOD_LALT | KEY_MOD_RALT)
#define KEY_MOD_GUI	(KEY_MOD_LGUI | KEY_MOD_RGUI)

enum kas_keycode
{
	KAS_SHIFT,
	KAS_CTRL,
	KAS_SPACE,
	KAS_BACKSPACE,
	KAS_ESCAPE,
	KAS_ENTER,
	KAS_F1,
	KAS_F2,
	KAS_F3,
	KAS_F4,
	KAS_F5,
	KAS_F6,
	KAS_F7,
	KAS_F8,
	KAS_F9,
	KAS_F10,
	KAS_F11,
	KAS_F12,
	KAS_TAB,
	KAS_UP,
	KAS_DOWN,
	KAS_LEFT,
	KAS_RIGHT,
	KAS_DELETE,
	KAS_HOME,
	KAS_END,
	KAS_0,
	KAS_1,
	KAS_2,
	KAS_3,
	KAS_4,
	KAS_5,
	KAS_6,
	KAS_7,
	KAS_8,
	KAS_9,
	KAS_A,
	KAS_B,  
	KAS_C, 
	KAS_D, 
	KAS_E, 
	KAS_F, 
	KAS_G, 
	KAS_H, 
	KAS_I, 
	KAS_J, 
	KAS_K, 
	KAS_L, 
	KAS_M, 
	KAS_N, 
	KAS_O, 
	KAS_P, 
	KAS_Q, 
	KAS_R, 
	KAS_S, 
	KAS_T, 
	KAS_U, 
	KAS_V, 
	KAS_W, 
	KAS_X, 
	KAS_Y, 
	KAS_Z, 
	KAS_NO_SYMBOL,
	KAS_KEY_COUNT
};

enum mouse_button
{
	MOUSE_BUTTON_LEFT,
	MOUSE_BUTTON_RIGHT,
	MOUSE_BUTTON_SCROLL,
	MOUSE_BUTTON_NONMAPPED,
	MOUSE_BUTTON_COUNT
};

enum mouse_scroll
{
	MOUSE_SCROLL_UP,
	MOUSE_SCROLL_DOWN,
	MOUSE_SCROLL_COUNT
};

/************************************************************************/
/* 			      system events 				*/
/************************************************************************/

enum system_event_type 
{
	SYSTEM_SCROLL,
	SYSTEM_KEY_PRESSED,
	SYSTEM_KEY_RELEASED,
	SYSTEM_BUTTON_PRESSED,
	SYSTEM_BUTTON_RELEASED,
	SYSTEM_CURSOR_POSITION,
	SYSTEM_TEXT_INPUT,
	SYSTEM_WINDOW_CLOSE,
	SYSTEM_WINDOW_CURSOR_ENTER,
	SYSTEM_WINDOW_CURSOR_LEAVE,
	SYSTEM_WINDOW_FOCUS_IN,
	SYSTEM_WINDOW_FOCUS_OUT,
	SYSTEM_WINDOW_EXPOSE,
	SYSTEM_WINDOW_CONFIG,
	SYSTEM_WINDOW_MINIMIZE,
	SYSTEM_NO_EVENT,
};

struct system_event {
	POOL_SLOT_STATE;
	DLL_SLOT_STATE;
	u64			native_handle;	/* window handle 			*/
	u64			ns_timestamp;	/* external event time; NOT OUR CLOCK 	*/
	enum system_event_type 	type;

	/* Input key */
	enum kas_keycode keycode; 	
	enum kas_keycode scancode;

	/* Input Mouse button */
	enum mouse_button button; 

	/* mouse scrolling */
	struct 
	{
		enum mouse_scroll direction;
		u32 count;
	} scroll;

	vec2 cursor_position; 			/* system window coordinate space cursor position */
	vec2 cursor_motion; 			/* system window coordinate space relative motion */
	vec2 native_cursor_window_position;	/* native window coordinate space cursor position */
	vec2 native_cursor_window_delta;	/* native window coordinate space cursor delta */

	utf8	utf8;
};

/************************************************************************/
/* 				System Debug 				*/
/************************************************************************/

extern void (*fatal_cleanup_and_exit)(const u32 thread);

/************************************************************************/
/* 				System IO 				*/
/************************************************************************/

#define FILE_READ	0
#define FILE_WRITE	(1 << 0)
#define FILE_TRUNCATE	(1 << 1)

enum fs_error
{
	FS_SUCCESS = 0,
	FS_BUFFER_TO_SMALL,
	FS_ALREADY_EXISTS,
	FS_HANDLE_INVALID,
	FS_FILE_IS_NOT_DIRECTORY,
	FS_DIRECTORY_NOT_EMPTY,
	FS_PERMISSION_DENIED,
	FS_TYPE_INVALID,
	FS_PATH_INVALID,
	FS_ERROR_UNSPECIFIED,
	FS_COUNT
};

enum file_type
{
	FILE_NONE,
	FILE_REGULAR,
	FILE_DIRECTORY,
	FILE_UNRECOGNIZED,
	FILE_COUNT
};

struct file
{
	file_handle		handle;	/* WARNING: not necessarily opened 		*/
	enum file_type		type;	/* file type 					*/
	utf8			path;	/* context dependent: relative or absolute 	*/
};

struct file file_null(void);

#endif
