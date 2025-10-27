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

#ifndef __KAS_STRING_H__
#define __KAS_STRING_H__

#include <stdarg.h>
#include "allocator.h"

/*
 *	String Library 
 *
 *	utf8 - NOT null-terminated unicode string
 *	utf32 - 32bit unicode string
 */


#define utf8_inline(cstr) (utf8)  		\
	{					\
		.len =	sizeof(cstr)-1,		\
		.size =	sizeof(cstr),		\
		.buf =  (u8 *) cstr,		\
	}

typedef struct utf8 utf8;
struct utf8 
{
	u8 *buf;	
	u32 size;	/* Number of bytes in buffer */
	u32 len;	/* Length of string (not bytes) (not including **POSSIBLE** '\0') */
};

/*
 * utf32 - utf32 encoded buffer, not null terminated 
 */
typedef struct utf32 utf32;
struct utf32
{
	u32 len;	/* number of codepoints in buf */
	u32 max_len;	/* buffer length in (u32) */
	u32 *buf;	/* buf[max_len] */
};

/************************************** Helpers ***************************************/

enum parse_retval_type
{
	PARSE_SUCCESS = 0,
	PARSE_UNDERFLOW,
	PARSE_OVERFLOW,
	PARSE_STRING_INVALID,
	PARSE_NO_OP,
	PARSE_COUNT
};

struct parse_retval
{
	enum parse_retval_type 	op_result;	/* 0 on success, >= 1 on failure */
	union 
	{
		u8	u8;
		u16	u16;
		u32	u32;
		u64	u64;
		i8	i8;
		i16	i16;
		i32	i32;
		i64	i64;
		f32	f32;
	};
};

/* return true if codepoint is whitespace (' ', '\t' or \n') or any of ('=', '-', ':', ';'. '\\', '/') */
u32	is_wordbreak(const u32 codepoint);

/************************************** C-Strings ***************************************/

u32			cstr_hash(const char *cstr);

char *			cstr_utf8(struct arena *mem, const utf8 utf8);

/* new_offset is set to be just past the end of str. */
f32			f32_cstr(char **new_offset, const char *str);
f64			f64_cstr(char **new_offset, const char *str);

/************************************** UTF8 ***************************************/

/* return required size to hold ut8.buf[len] */
u64 			utf8_size_required(const utf8 utf8);
/* return 1 if string contents are equilvanet, 0 otherwise */
u32			utf8_equivalence(const utf8 str1, const utf8 str2);	
u32			utf8_hash(const utf8 utf8);

#define			UTF8_BAD_CODEPOINT	U32_MAX
/* return utf32 codepoint on success, or UTF8_BAD_CODEPOINT on bad offset */
u32 			utf8_read_codepoint(u64 *new_offset, const utf8 *str, const u64 offset);	
/* return length written into buf, or 0 of failure. */
u32			utf8_write_codepoint(u8 *buf, const u32 bufsize, const u32 codepoint);

void			utf8_debug_print(const utf8 str);

utf8 			utf8_empty(void);
utf8			utf8_alloc(struct arena *mem, const u64 bufsize);
utf8			utf8_buffered(u8 buf[], const u64 bufsize);

utf8			utf8_copy_buffered(u8 buf[], const u64 bufsize, const utf8 str);
utf8			utf8_copy_buffered_and_return_required_size(u64 *reqsize, u8 buf[], const u64 bufsize, const utf8 str);
utf8			utf8_cstr_buffered(u8 buf[], const u64 bufsize, const char *cstr);
utf8			utf8_f32_buffered(u8 buf[], const u64 bufsize, const u32 decimals, const f32 val);
utf8			utf8_f64_buffered(u8 buf[], const u64 bufsize, const u32 decimals, const f64 val);
utf8			utf8_u64_buffered(u8 buf[], const u64 bufsize, const u64 val);
utf8			utf8_i64_buffered(u8 buf[], const u64 bufsize, const i64 val);
utf8			utf8_utf32_buffered(u8 buf[], const u64 bufsize, const utf32 str);
utf8			utf8_utf32_buffered_null_terminated(u8 buf[], const u64 bufsize, const utf32 str);
utf8			utf8_utf32_buffered_and_return_required_size(u64 *reqsize, u8 buf[], const u64 bufsize, const utf32 str);
utf8 			utf8_utf32_buffered_null_terminated_and_return_required_size(u64 *reqsize, u8 buf[], const u64 bufsize, const utf32 str);

utf8			utf8_copy(struct arena *mem, const utf8 str);
utf8			utf8_cstr(struct arena *mem, const char *cstr);
utf8			utf8_f32(struct arena *mem, const u32 decimals, const f32 val);
utf8			utf8_f64(struct arena *mem, const u32 decimals, const f64 val);
utf8			utf8_u64(struct arena *mem, const u64 val);
utf8			utf8_i64(struct arena *mem, const i64 val);
utf8			utf8_utf32(struct arena *mem, const utf32 str);
utf8			utf8_utf32_null_terminated(struct arena *mem, const utf32 str);

f32			f32_utf8(struct arena *tmp, const utf8 str);
f64			f64_utf8(struct arena *tmp, const utf8 str);
struct parse_retval 	u64_utf8(const utf8 str);	/* on overflow, return u64 value 0 		*/
struct parse_retval 	i64_utf8(const utf8 str); 	/* on under/overflow, return i64 value 0 	*/

/******************************** UTF8 substring lookup *******************************/

struct kmp_substring
{
	utf32	substring;
	u32 *	backtrack;	/* backtrack[substring.len] : KMP backtracking indices  */
	u32	start;		/* if utf8, (start byte) else if utf32 (start index) */
};
/* initialize substring helper for substring matching using Knuth-Morris-Pratt algorithm (KMP) */
struct kmp_substring	utf8_lookup_substring_init(struct arena *mem, const utf8 str);
/* KMP algorithm: return true on substring found, false otherwise. If true, set kmp_substring to first encounter of substring  */
u32			utf8_lookup_substring(struct kmp_substring *kmp_substring, const utf8 str);

/*********************************** UTF8 Formatting ***********************************/

/*
 * utf8_format - takes format string and applies the following rules to generate a utf8. The final generated
 * 		 string will have its length set in len, which will be 0 on failure.
 *
 * Format follow the utf8 formatting rules:
 * 	%s   	- C string 
 * 	%k 	- utf8 ptr
 * 	%i	- i32
 * 	%u	- u32
 * 	%(d)f  	- f64, optionally replace (d) with number of decimals, defaults to 7
 * 	%li 	- i64
 * 	%lu 	- u64
 * 	%p	- Pointer
 * Note: 
 * 	As variadic arguments will promote themselves (or self-promote), dangers arise when the function expects 64 bit values and the user gives 32 bit values or less.
 * 	The smaller values such as char, unsigned int, floats, will be promoted to some type (perhaps self-promotions) and generate storage for that type. Thus, formatting
 * 	chars or perhaps ints as %lli will result in undefined behaviour as the function will grab 64 bits from a 32 bit storage, generating garbage. Explicit casting removes this error.
 */
utf8 			utf8_format(struct arena *mem, const char *format, ...); 
utf8 			utf8_format_variadic(struct arena *mem, const char *format, va_list args); 
utf8 			utf8_format_buffered(u8 *buf, const u64 bufsize, const char *format, ...); 
utf8 			utf8_format_buffered_variadic(u64 *reqsize, u8 *buf, const u64 bufsize, const char *format, va_list args);

/************************************** UTF32 ***************************************/

void			utf32_debug_print(const utf32 str);

utf32 			utf32_empty(void);
utf32			utf32_alloc(struct arena *mem, const u32 len);
utf32			utf32_buffered(u32 buf[], const u32 len);

utf32			utf32_copy_buffered(u32 buf[], const u64 buflen, const utf32 str);
utf32			utf32_cstr_buffered(u32 buf[], const u64 buflen, const char *cstr);
utf32			utf32_f32_buffered(u32 buf[], const u64 buflen, const u32 decimals, const f32 val);
utf32			utf32_f64_buffered(u32 buf[], const u64 buflen, const u32 decimals, const f64 val);
utf32			utf32_u64_buffered(u32 buf[], const u64 buflen, const u64 val);
utf32			utf32_i64_buffered(u32 buf[], const u64 buflen, const i64 val);
utf32			utf32_utf8_buffered(u32 buf[], const u64 buflen, const utf8 str);

utf32			utf32_copy(struct arena *mem, const utf32 str);
utf32			utf32_cstr(struct arena *mem, const char *cstr);
utf32			utf32_f32(struct arena *mem, const u32 decimals, const f32 val);
utf32			utf32_f64(struct arena *mem, const u32 decimals, const f64 val);
utf32			utf32_u64(struct arena *mem, const u64 val);
utf32			utf32_i64(struct arena *mem, const i64 val);
utf32			utf32_utf8(struct arena *mem, const utf8 str);

f32			f32_utf32(struct arena *tmp, const utf32 str);
f64			f64_utf32(struct arena *tmp, const utf32 str);
struct parse_retval	u64_utf32(const utf32 str);	/* on overflow, return u64 value 0 		*/
struct parse_retval	i64_utf32(const utf32 str); 	/* on under/overflow, return i64 value 0 	*/

/************************************* Text Font Layout *************************************/

struct text_glyph
{
	f32 	x;		/* x[i] = offset of codepoint[i] glyph on line (in whole pixels) 	*/
	u32 	codepoint;	/* text line codepoints 						*/
};

struct text_line
{
	struct text_line *	next;
	u32			glyph_count;
	struct text_glyph *	glyph;
};

struct text_layout
{
	struct text_line *	line;	
	u32			line_count;
	f32			width;		/* max line width */
};

struct font;

/* process utf32 according to font rules and construct required lines. Each line baseline starts at x = 0, with an 
 * height calculated according to linespace = ascent - descent + linegap. */
struct text_layout *utf32_text_layout(struct arena *mem, const utf32 *str, const f32 line_width, const u32 tab_size, const struct font *font);
struct text_layout *utf32_text_layout_include_whitespace(struct arena *mem, const utf32 *str, const f32 line_width, const u32 tab_size, const struct font *font);

#endif
