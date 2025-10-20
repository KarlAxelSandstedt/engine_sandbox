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
#include "kas_string.h"
#include "sys_public.h"
#include "asset_public.h"
#include "dtoa.h"

#define F32_CONV_BASIC_SIZE 192

u32 is_wordbreak(const u32 codepoint)
{
	u32 wordbreak = 0;
	switch (codepoint)
	{
		case ' ':
		case '\t':
		case '\n':
		case '=':
		case '-':
		case ':':
		case ';':
		case '\\':
		case '/':
		{
			wordbreak = 1;
		} break;

		default: { } break;
	}

	return wordbreak;
}

const utf8 	empty_utf8 = { .len = 0, .size = 0, .buf = (u8 *) "" };
const utf32  	empty_utf32 = { .len = 0, .max_len = 0, .buf = NULL };

utf32 utf32_stream_consume_whitespace(utf32 *stream)
{
	utf32 sub = { .len = 0, .buf = stream->buf };
	for (; sub.len < stream->len; ++sub.len)
	{
		if (stream->buf[sub.len] != ' ' && stream->buf[sub.len] != '\n' && stream->buf[sub.len] != '\t')
		{
			break;
		}
	}
	sub.max_len = sub.len;

	stream->len -= sub.len;
	stream->max_len -= sub.len;
	stream->buf += sub.len;
	return sub;
}

utf32 utf32_stream_consume_non_whitespace(utf32 *stream)
{
	utf32 sub = { .len = 0, .buf = stream->buf };
	for (; sub.len < stream->len; ++sub.len)
	{
		if (stream->buf[sub.len] == ' ' || stream->buf[sub.len] == '\n' || stream->buf[sub.len] == '\t')
		{
			break;
		}
	}
	sub.max_len = sub.len;

	stream->len -= sub.len;
	stream->max_len -= sub.len;
	stream->buf += sub.len;
	return sub;
}

u32 utf8_read_codepoint(u64 *new_offset, const utf8 *str, const u64 offset)
{
	u32 decoded;
	u32 bad_sequence = 0;

	const u32 inv = ~((u32) (str->buf[offset]) << 24);
	const u32 leading_ones_count = clz32(inv);

	switch (leading_ones_count)
	{
		case 0:
		{
			*new_offset = offset + 1;
			decoded = (u32) (str->buf[offset] & 0x7f);
		} break;

		case 2:
		{
			*new_offset = offset + 2;
			decoded  = (u32) (str->buf[offset]   & 0x1f) << 6 
			         | (u32) (str->buf[offset+1] & 0x3f);
				
			/* 
			 * valid value : (10xx xxxx) 
			 * expected    : (val & 0x1100 0000) ^ 1000 0000 == 0000 0000
			 * bad	       : (00,01,11)00 0000   ^ 1000 0000 == 1x00 0000
			 */
			bad_sequence = !!((u32) ((str->buf[offset+1] & 0xc0) ^ 0x80));
		} break;

		case 3:
		{
			*new_offset = offset + 3;
			decoded  = (u32) (str->buf[offset]   & 0x0f) << 12 
			         | (u32) (str->buf[offset+1] & 0x3f) << 6 
			         | (u32) (str->buf[offset+2] & 0x3f);

			bad_sequence = !!((u32) ((str->buf[offset+1] & 0xc0) ^ 0x80)
			             	+ (u32) ((str->buf[offset+2] & 0xc0) ^ 0x80));
		} break;

		case 4:
		{
			*new_offset = offset + 4;
			decoded  = (u32) (str->buf[offset]   & 0x07) << 18 
			         | (u32) (str->buf[offset+1] & 0x3f) << 12 
			         | (u32) (str->buf[offset+2] & 0x3f) << 6 
			         | (u32) (str->buf[offset+3] & 0x3f);

			bad_sequence = !!((u32) ((str->buf[offset+1] & 0xc0) ^ 0x80)
			             	+ (u32) ((str->buf[offset+2] & 0xc0) ^ 0x80)
			  	     	+ (u32) ((str->buf[offset+3] & 0xc0) ^ 0x80));
		} break;
		
		default:
		{
			*new_offset = offset + 1;
			bad_sequence = 1;
		} break;
	}

	decoded = decoded*(1-bad_sequence) + bad_sequence*UTF8_BAD_CODEPOINT;

	return decoded;
}

u32 utf32_whitespace_width(const struct font *font, const utf32 *whitespace, const u32 tab_size)
{
	u32 pixels = 0;

	const struct font_glyph *glyph = glyph_lookup(font, (u32) ' ');
	const u32 space_pixels = glyph->advance;
	const u32 tab_pixels = tab_size*space_pixels;
	u32 new_line = 0;
	for (u32 i = 0; i < whitespace->len; ++i)
	{
		switch (whitespace->buf[i])
		{
			case ' ':
			{
				pixels += space_pixels;
			} break;

			case '\t':
			{
				pixels += tab_pixels;
			} break;

			case '\n':
			{
				new_line = 1;
			} break;

			default:
			{
				assert(0 && "whitespace string contains non-whitespace");
			}
		}
	}

	return (new_line) ? U32_MAX : pixels;
}

utf32 font_stream_substring_on_row(utf32 *text, u32 *x_new_offset, const struct font *font, const u32 x_offset, const u32 line_width)
{
	utf32 sub = { .len = 0, .buf = text->buf };

	const u32 pixels_left = line_width - x_offset;
	u32 substring_pixels = 0;

	const struct font_glyph *linebreak = glyph_lookup(font, (u32) '-');
	u32 substring_with_wordbreak_len = 0;
	u32 substring_pixels_with_wordbreak = 0;
	
	for (; sub.len < text->len; ++sub.len)
	{
		const struct font_glyph *glyph = glyph_lookup(font, text->buf[sub.len]);
		if (substring_pixels + glyph->bearing[0] + glyph->size[0] > pixels_left)
		{
			break;
		}

		substring_pixels += glyph->advance;
		if (substring_pixels + linebreak->bearing[0] + linebreak->size[0] <= pixels_left)
		{
			substring_with_wordbreak_len += 1;
			substring_pixels_with_wordbreak += glyph->advance;
		}
	}
	
	if (0 < sub.len && sub.len < text->len)
	{
		sub.len = substring_with_wordbreak_len;
		substring_pixels = (substring_with_wordbreak_len) ? substring_pixels_with_wordbreak : 0;
	}

	*x_new_offset = x_offset + substring_pixels;
	text->len -= sub.len;
	text->buf += sub.len;
	return sub;
}

struct text_layout *utf32_text_layout(struct arena *mem, const utf32 *str, const f32 line_width, const u32 tab_size, const struct font *font)
{
	struct text_layout *layout = arena_push(mem, sizeof(struct text_layout));
	layout->line_count = 1;
	layout->line = arena_push(mem, sizeof(struct text_line));
	layout->line->next = NULL;
	layout->line->glyph_count = 0;
	layout->line->glyph = (void *) mem->stack_ptr;
	struct text_line *line = layout->line;

	const u32 line_pixels = (line_width == F32_INFINITY) ? U32_MAX : (u32) line_width;

	u32 x_offset = 0;
	u32 begin_new_line = 0;
	utf32 stream = *str;
	while (stream.len)
	{
		utf32 whitespace = utf32_stream_consume_whitespace(&stream);
		const u32 pixels = utf32_whitespace_width(font, &whitespace, tab_size);
		x_offset = (pixels == U32_MAX || x_offset + pixels > (u32) line_pixels) ? line_pixels : x_offset + pixels;

		utf32 word = utf32_stream_consume_non_whitespace(&stream);
		/* fill row(s) with word */
		while (word.len)
		{
			if (begin_new_line)
			{
				layout->line_count += 1;
				line->next = arena_push(mem, sizeof(struct text_line));
				line = line->next;
				line->next = NULL;
				line->glyph_count = 0;
				line->glyph = (void *) mem->stack_ptr;
				begin_new_line = 0;
			}

			u32 x = x_offset;
			/* find substring of word that fits on row, advance substring */
			utf32 sub = font_stream_substring_on_row(&word, &x_offset, font, x_offset, line_pixels);
			for (u32 i = 0; i < sub.len; ++i)
			{
				arena_push_packed(mem, sizeof(struct text_glyph));
				line->glyph[line->glyph_count].x = x;
				line->glyph[line->glyph_count].codepoint = sub.buf[i];
				line->glyph_count += 1;
				x += glyph_lookup(font, sub.buf[i])->advance;
			}

			/* couldn't fit whole word on row */
			if (word.len)
			{
				begin_new_line = 1;
				if (sub.len == 0)
				{
					if (x_offset == 0)
					{
						break;
					}
				}
				else
				{
					arena_push_packed(mem, sizeof(struct text_glyph));
					line->glyph[line->glyph_count].x = x_offset;
					line->glyph[line->glyph_count].codepoint = (u32) '-';
					line->glyph_count += 1;
				}
				x_offset = 0;	
			}
		}
	}

	layout->width = (layout->line_count > 1)
		? line_width
		: (f32) x_offset;
	return layout;
}

struct text_layout *utf32_text_layout_include_whitespace(struct arena *mem, const utf32 *str, const f32 line_width, const u32 tab_size, const struct font *font)
{
	struct text_layout *layout = arena_push(mem, sizeof(struct text_layout));
	layout->line_count = 1;
	layout->line = arena_push(mem, sizeof(struct text_line));
	layout->line->next = NULL;
	layout->line->glyph_count = 0;
	layout->line->glyph = (void *) mem->stack_ptr;
	struct text_line *line = layout->line;

	const u32 line_pixels = (line_width == F32_INFINITY) ? U32_MAX : (u32) line_width;

	const struct font_glyph *glyph = glyph_lookup(font, (u32) ' ');
	const u32 space_pixels = glyph->advance;
	const u32 tab_pixels = tab_size*space_pixels;

	u32 x_offset = 0;
	u32 begin_new_line = 0;
	utf32 stream = *str;
	while (stream.len)
	{
		utf32 whitespace = utf32_stream_consume_whitespace(&stream);

		u32 pixels = 0;
		u32 new_line = 0;
		for (u32 i = 0; i < whitespace.len; ++i)
		{
			arena_push_packed(mem, sizeof(struct text_glyph));
			line->glyph[line->glyph_count].x = x_offset;
			line->glyph[line->glyph_count].codepoint = whitespace.buf[i];
			line->glyph_count += 1;

			switch (whitespace.buf[i])
			{
				case ' ':
				{
					x_offset += space_pixels;
				} break;
		
				case '\t':
				{
					x_offset += tab_pixels;
				} break;
		
				case '\n':
				{
					new_line = 1;
				} break;
		
				default:
				{
					assert(0 && "whitespace string contains non-whitespace");
				}
			}
		}
		x_offset = (new_line || x_offset > (u32) line_pixels) ? line_pixels : x_offset;

		utf32 word = utf32_stream_consume_non_whitespace(&stream);
		/* fill row(s) with word */
		while (word.len)
		{
			if (begin_new_line)
			{	
				layout->line_count += 1;
				line->next = arena_push(mem, sizeof(struct text_line));
				line = line->next;
				line->next = NULL;
				line->glyph_count = 0;
				line->glyph = (void *) mem->stack_ptr;
				begin_new_line = 0;
			}

			u32 x = x_offset;
			/* find substring of word that fits on row, advance substring */
			utf32 sub = font_stream_substring_on_row(&word, &x_offset, font, x_offset, line_pixels);
			for (u32 i = 0; i < sub.len; ++i)
			{
				arena_push_packed(mem, sizeof(struct text_glyph));
				line->glyph[line->glyph_count].x = x;
				line->glyph[line->glyph_count].codepoint = sub.buf[i];
				line->glyph_count += 1;
				x += glyph_lookup(font, sub.buf[i])->advance;
			}

			/* couldn't fit whole word on row */
			if (word.len)
			{
				begin_new_line = 1;
				if (sub.len == 0)
				{
					if (x_offset == 0)
					{
						break;
					}
				}
				else
				{
					arena_push_packed(mem, sizeof(struct text_glyph));
					line->glyph[line->glyph_count].x = x_offset;
					line->glyph[line->glyph_count].codepoint = (u32) '-';
					line->glyph_count += 1;
				}
				x_offset = 0;	
			}
		}
	}

	layout->width = (layout->line_count > 1)
		? line_width
		: (f32) x_offset;
	return layout;
}

char *cstr_utf8(struct arena *mem, const utf8 utf8)
{	
	const u64 size = utf8_size_required(utf8);
	char *ret = arena_push(mem, size+1); 
	if (ret)
	{
		memcpy(ret, utf8.buf, size);
		ret[size] = '\0';
	}	

	return (ret) ? ret : "";
}

f32 f32_cstr(char **new_offset, const char *str)
{
	return (f32) dmg_strtod(str, new_offset);
}

f64 f64_cstr(char **new_offset, const char *str)
{
	return dmg_strtod(str, new_offset);
}

f32 f32_utf8(struct arena *tmp, const utf8 str)
{
	return (f32) f64_utf8(tmp, str);
}

f64 f64_utf8(struct arena *tmp, const utf8 str)
{
	if (str.len == 0)
	{
		return 0;
	}

	const char *cstr = cstr_utf8(tmp, str);
	const f64 val = dmg_strtod(cstr, NULL);

	return val;
}

f32 f32_utf32(struct arena *tmp, const utf32 str)
{
	return (f32) f64_utf32(tmp, str);
}

f64 f64_utf32(struct arena *tmp, const utf32 str)
{
	f64 ret = 0.0f;
	char *buf = arena_push_packed(tmp, str.len+1);
	if (buf)
	{
		for (u32 i = 0; i < str.len; ++i)
		{
			buf[i] = (char) str.buf[i];	
		}
		buf[str.len] = '\0';
		ret = dmg_strtod(buf, NULL);
		arena_pop_packed(tmp, str.len+1);
	}
	return ret;
}

u64 utf8_size_required(const utf8 utf8)
{
	u64 size = 0;
	for (u32 i = 0; i < utf8.len; ++i)
	{
		utf8_read_codepoint(&size, &utf8, size);
	}
	return size;
}

utf8 utf8_f32_buffered(u8 buf[], const u64 bufsize, const u32 decimals, const f32 val)
{                                                                                  
	return utf8_f64_buffered(buf, bufsize, decimals, (f64) val);
}                                                                                  
                                                                                   
utf8 utf8_f64_buffered(u8 buf[], const u64 bufsize, const u32 decimals, const f64 val)
{
	utf8 str = utf8_empty();

	i32 sign;
	i32 decpt;
	char *dmg_str = dmg_dtoa((f64) val, 0, 0, &decpt, &sign, NULL);

	/* INF / Nan */
	if (decpt == 9999)
	{
		str = utf8_cstr_buffered(buf, bufsize, dmg_str);
	}
	else
	{
		const u64 dmg_len = strlen(dmg_str);
		u64 integers_in_dmg = 0; 
		u64 integer_zero_leading = 0; 
		u64 integer_zeroes_trailing = 0;
		u64 decimals_in_dmg = 0;
		u64 decimal_zeroes_leading = 0;
		u64 decimal_zeroes_trailing = 0;	
		u64 decimals_in_dmg_wanted = 0;
		u64 decimals_wanted = decimals;

		/* 0.**** */
		if (decpt < 1)
		{
			decimals_in_dmg = dmg_len;
			integer_zero_leading = 1;
			decimal_zeroes_leading = (u64) (-decpt);
		}
		/* ****.0 */
		else if ((u64) decpt >= dmg_len)
		{
			integers_in_dmg = dmg_len;
			integer_zeroes_trailing = (u64) decpt - dmg_len;
		}
		/* ****.**** */
		else
		{
			integers_in_dmg = (u64) decpt;
			decimals_in_dmg = dmg_len - (u64) decpt;
		}

		if (decimals == 0)
		{
			decimals_wanted = decimal_zeroes_leading + decimals_in_dmg;
			decimals_in_dmg_wanted = decimals_in_dmg;
		}
		else if (decimals <= decimal_zeroes_leading)
		{
			decimals_in_dmg_wanted = 0;
			decimal_zeroes_trailing = decimals;
		}
		else if (decimals <= decimal_zeroes_leading + decimals_in_dmg)
		{
			decimals_in_dmg_wanted = decimals - decimal_zeroes_leading;
		}
		else
		{
			decimals_in_dmg_wanted = decimals_in_dmg;
			decimal_zeroes_trailing = decimals - decimal_zeroes_leading - decimals_in_dmg;
		}

		const u64 req_size = sign
			+ integer_zero_leading 
			+ integers_in_dmg
			+ integer_zeroes_trailing
			+ ((decimals_wanted) ? 1 + decimals_wanted : 0);

		if (req_size <= bufsize)
		{
			u32 i = 0;
			u32 dmg_i = 0;

			if (sign)
			{
				buf[i++] = '-';
			}

			if (integer_zero_leading)
			{
				buf[i++] = '0';			
			}

			for (u32 k = 0; k < integers_in_dmg; ++k)
			{
				buf[i++] = dmg_str[dmg_i++];			
			}

			for (u32 k = 0; k < integer_zeroes_trailing; ++k)
			{
				buf[i++] = '0';			
			}

			if (decimals_wanted)
			{
				buf[i++] = '.';			

				for (u32 k = 0; k < decimal_zeroes_leading; ++k)
				{
					buf[i++] = '0';			
				}

				for (u32 k = 0; k < decimals_in_dmg_wanted; ++k)
				{
					buf[i++] = dmg_str[dmg_i++];			
				}

				for (u32 k = 0; k < decimal_zeroes_trailing; ++k)
				{
					buf[i++] = '0';			
				}
			}

			kas_assert(i == req_size);
			str.buf = buf;
			str.len = i;
			str.size = bufsize;
		}
	}

	freedtoa(dmg_str);

	return str;
}

utf8 utf8_u64_buffered(u8 buf[], const u64 bufsize, const u64 val)
{
	utf8 ret = utf8_empty();

	u8 tmp[64];

	if (bufsize >= 1)
	{
		u64 v = val;
		u32 len = 0;
		do
		{
			tmp[64-1-len++] = '0' + (v % 10);
			v /= 10;
		} while (v && (len < bufsize));

		if (v == 0)
		{
			ret = (utf8) { .buf = buf, .len = len, .size = bufsize, };
			for (u32 i = 0; i < len; ++i)
			{
				buf[i] = tmp[64-len+i];
			}
		}
	}

	return ret;
}

utf8 utf8_i64_buffered(u8 buf[], const u64 bufsize, const i64 val)
{
	utf8 ret = utf8_empty();

	b64 b = { .i = val };
	const u64 sign = b.u >> 63;
	b.u = (1-sign)*b.u + sign*((~b.u) + 1);
	if (sign + 2 <= bufsize)
	{
		buf[0] = '-';
		const utf8 str = utf8_u64_buffered(buf+sign, bufsize-sign, b.u);
		if (str.len)
		{
			ret.buf = buf;
			ret.len = str.len+sign;
			ret.size = bufsize;		
		}
	}

	return ret;
}

struct parse_retval i64_utf8(const utf8 str)
{
	struct parse_retval ret = { .op_result = PARSE_SUCCESS, .i64 = 0 };
	utf8 tmp = str;
	if (tmp.len)
	{
		u64 sign = 0;
		if (tmp.buf[0] == '-')
		{
			tmp.len -= 1;
			tmp.buf += 1;
			sign = 1;
		}

		ret = u64_utf8(tmp);
		if (!ret.op_result)
		{
			if (sign)
			{
				/* signed integer underflow */
				if (ret.u64 > (u64) I64_MAX + 1)
				{
					ret = (struct parse_retval) { .op_result = PARSE_UNDERFLOW, .i64 = 0 };
				}
				else
				{
					ret.u64 = (~ret.u64) + 1;
				}
			}
			/* signed integer overflow */
			else if (ret.u64 > I64_MAX)
			{
				ret = (struct parse_retval) { .op_result = PARSE_OVERFLOW, .i64 = 0 };
			}
		} 
		else if (sign && ret.op_result == PARSE_OVERFLOW)
		{
			ret = (struct parse_retval) { .op_result = PARSE_UNDERFLOW, .i64 = 0 };
		}

	}

	return ret;
}

struct parse_retval u64_utf8(const utf8 str)
{
	u64 overflow = 0;
	u64 ret = 0;
	for (u32 i = 0; i < str.len; ++i)
	{
		const u64 c = str.buf[i];
		if (c < '0' || '9' < c)
		{
			return (struct parse_retval) { .op_result = PARSE_STRING_INVALID, .u64 = 0 };
		}

		overflow |= u64_mul_return_overflow(&ret, ret, 10);
		overflow |= u64_add_return_overflow(&ret, ret, c-'0');
	}

	return (overflow) 
		? (struct parse_retval) { .op_result = PARSE_OVERFLOW, .u64 = 0   }
		: (struct parse_retval) { .op_result = PARSE_SUCCESS,  .u64 = ret };
}

struct parse_retval i64_utf32(const utf32 str)
{
	struct parse_retval ret = { .op_result = PARSE_SUCCESS, .i64 = 0 };
	utf32 tmp = str;
	if (tmp.len)
	{
		u64 sign = 0;
		if (tmp.buf[0] == '-')
		{
			tmp.len -= 1;
			tmp.buf += 1;
			sign = 1;
		}

		ret = u64_utf32(tmp);
		if (!ret.op_result)
		{
			if (sign)
			{
				/* signed integer underflow */
				if (ret.u64 > (u64) I64_MAX + 1)
				{
					ret = (struct parse_retval) { .op_result = PARSE_UNDERFLOW, .i64 = 0 };
				}
				else
				{
					ret.u64 = (~ret.u64) + 1;
				}
			}
			/* signed integer overflow */
			else if (ret.u64 > I64_MAX)
			{
				ret = (struct parse_retval) { .op_result = PARSE_OVERFLOW, .i64 = 0 };
			}
		} 
		else if (sign && ret.op_result == PARSE_OVERFLOW)
		{
			ret = (struct parse_retval) { .op_result = PARSE_UNDERFLOW, .i64 = 0 };
		}

	}

	return ret;
}

struct parse_retval u64_utf32(const utf32 str)
{
	u64 overflow = 0;
	u64 ret = 0;
	for (u32 i = 0; i < str.len; ++i)
	{
		const u64 c = str.buf[i];
		if (c < '0' || '9' < c)
		{
			return (struct parse_retval) { .op_result = PARSE_STRING_INVALID, .u64 = 0 };
		}

		overflow |= u64_mul_return_overflow(&ret, ret, 10);
		overflow |= u64_add_return_overflow(&ret, ret, c-'0');
	}

	return (overflow) 
		? (struct parse_retval) { .op_result = PARSE_OVERFLOW, .u64 = 0   }
		: (struct parse_retval) { .op_result = PARSE_SUCCESS,  .u64 = ret };
}
utf8 utf8_f32(struct arena *mem, const u32 decimals, const f32 val)
{                                                               
	return utf8_f64(mem, decimals, (f64) val);
}                                                               
                                                                
utf8 utf8_f64(struct arena *mem, const u32 decimals, const f64 val)
{
	const u64 bufsize = mem->mem_left;
	u8 *buf = arena_push_packed(mem, bufsize);

	utf8 str = utf8_f64_buffered(buf, bufsize, decimals, val);
	if (str.len)
	{
		str.size = str.len;
		arena_pop_packed(mem, bufsize - str.size);
	}
	else
	{
		arena_pop_packed(mem, bufsize);
	}

	return str;
}

utf8 utf8_u64(struct arena *mem, const u64 val)
{
	const u64 bufsize = mem->mem_left;
	u8 *buf = arena_push_packed(mem, bufsize);

	utf8 str = utf8_u64_buffered(buf, bufsize, val);
	if (str.len)
	{
		str.size = str.len;
		arena_pop_packed(mem, bufsize - str.size);
	}
	else
	{
		arena_pop_packed(mem, bufsize);
	}

	return str;
}

utf8 utf8_i64(struct arena *mem, const i64 val)
{
	const u64 bufsize = mem->mem_left;
	u8 *buf = arena_push_packed(mem, bufsize);

	utf8 str = utf8_i64_buffered(buf, bufsize, val);
	if (str.len)
	{
		str.size = str.len;
		arena_pop_packed(mem, bufsize - str.size);
	}
	else
	{
		arena_pop_packed(mem, bufsize);
	}

	return str;
}

utf32 utf32_f32_buffered(u32 buf[], const u64 buflen, const u32 decimals, const f32 val)
{                                                                                    
	return utf32_f64_buffered(buf, buflen, decimals, (f64) val);
}                                                                                    
                                                                                     
utf32 utf32_f64_buffered(u32 buf[], const u64 buflen, const u32 decimals, const f64 val)
{
	utf32 str = utf32_empty();

	i32 sign;
	i32 decpt;
	char *dmg_str = dmg_dtoa((f64) val, 0, 0, &decpt, &sign, NULL);

	/* INF / Nan */
	if (decpt == 9999)
	{
		str = utf32_cstr_buffered(buf, buflen, dmg_str);
	}
	else
	{
		const u64 dmg_len = strlen(dmg_str);
		u64 integers_in_dmg = 0; 
		u64 integer_zero_leading = 0; 
		u64 integer_zeroes_trailing = 0;
		u64 decimals_in_dmg = 0;
		u64 decimal_zeroes_leading = 0;
		u64 decimal_zeroes_trailing = 0;	
		u64 decimals_in_dmg_wanted = 0;
		u64 decimals_wanted = decimals;

		/* 0.**** */
		if (decpt < 1)
		{
			decimals_in_dmg = dmg_len;
			integer_zero_leading = 1;
			decimal_zeroes_leading = (u64) (-decpt);
		}
		/* ****.0 */
		else if ((u64) decpt >= dmg_len)
		{
			integers_in_dmg = dmg_len;
			integer_zeroes_trailing = (u64) decpt - dmg_len;
		}
		/* ****.**** */
		else
		{
			integers_in_dmg = (u64) decpt;
			decimals_in_dmg = dmg_len - (u64) decpt;
		}

		if (decimals == 0)
		{
			decimals_wanted = decimal_zeroes_leading + decimals_in_dmg;
			decimals_in_dmg_wanted = decimals_in_dmg;
		}
		else if (decimals <= decimal_zeroes_leading)
		{
			decimals_in_dmg_wanted = 0;
			decimal_zeroes_trailing = decimals;
		}
		else if (decimals <= decimal_zeroes_leading + decimals_in_dmg)
		{
			decimals_in_dmg_wanted = decimals - decimal_zeroes_leading;
		}
		else
		{
			decimals_in_dmg_wanted = decimals_in_dmg;
			decimal_zeroes_trailing = decimals - decimal_zeroes_leading - decimals_in_dmg;
		}

		const u64 req_len = sign
			+ integer_zero_leading 
			+ integers_in_dmg
			+ integer_zeroes_trailing
			+ ((decimals_wanted) ? 1 + decimals_wanted : 0);

		if (req_len <= buflen)
		{
			u32 i = 0;
			u32 dmg_i = 0;

			if (sign)
			{
				buf[i++] = '-';
			}

			if (integer_zero_leading)
			{
				buf[i++] = '0';			
			}

			for (u32 k = 0; k < integers_in_dmg; ++k)
			{
				buf[i++] = dmg_str[dmg_i++];			
			}

			for (u32 k = 0; k < integer_zeroes_trailing; ++k)
			{
				buf[i++] = '0';			
			}

			if (decimals_wanted)
			{
				buf[i++] = '.';			

				for (u32 k = 0; k < decimal_zeroes_leading; ++k)
				{
					buf[i++] = '0';			
				}

				for (u32 k = 0; k < decimals_in_dmg_wanted; ++k)
				{
					buf[i++] = dmg_str[dmg_i++];			
				}

				for (u32 k = 0; k < decimal_zeroes_trailing; ++k)
				{
					buf[i++] = '0';			
				}
			}

			kas_assert(i == req_len);
			str.buf = buf;
			str.len = i;
			str.max_len = i;
		}
	}

	freedtoa(dmg_str);

	return str;
}

utf32 utf32_u64_buffered(u32 buf[], const u64 bufsize, const u64 val)
{
	utf32 ret = utf32_empty();

	u8 tmp[64];
	if (bufsize >= 1)
	{
		u64 v = val;
		u32 len = 0;
		do
		{
			tmp[64-1-len++] = '0' + (v % 10);
			v /= 10;
		} while (v && (len < bufsize));

		if (v == 0)
		{
			ret = (utf32) { .buf = buf, .len = len, .max_len = bufsize, };
			for (u32 i = 0; i < len; ++i)
			{
				buf[i] = tmp[64-len+i];
			}
		}
	}

	return ret;
}

utf32 utf32_i64_buffered(u32 buf[], const u64 bufsize, const i64 val)
{
	utf32 ret = utf32_empty();

	b64 b = { .i = val };
	const u64 sign = b.u >> 63;
	b.u = (1-sign)*b.u + sign*((~b.u) + 1);
	if (sign + 2 <= bufsize)
	{
		buf[0] = '-';
		const utf32 str = utf32_u64_buffered(buf+sign, bufsize-sign, b.u);
		if (str.len)
		{
			ret.buf = buf;
			ret.len = str.len+sign;
			ret.max_len = bufsize;		
		}
	}

	return ret;

}

utf32 utf32_f32(struct arena *mem, const u32 decimals, const f32 val)
{                                                                 
	return utf32_f64(mem, decimals, (f64) val);
}                                                                 
                                                                  
utf32 utf32_f64(struct arena *mem, const u32 decimals, const f64 val)
{
	struct allocation_array alloc = arena_push_aligned_all(mem, sizeof(u32), sizeof(u32));

	utf32 str = utf32_f64_buffered(alloc.addr, alloc.len, decimals, val);
	if (str.len)
	{
		str.max_len = str.len;
		arena_pop_packed(mem, (alloc.len-str.len)*sizeof(u32));
	}
	else
	{
		arena_pop_packed(mem, alloc.mem_pushed);
	}
	
	return str;
}

utf32 utf32_u64(struct arena *mem, const u64 val)
{
	struct allocation_array alloc = arena_push_aligned_all(mem, sizeof(u32), sizeof(u32));

	utf32 str = utf32_u64_buffered(alloc.addr, alloc.len, val);
	if (str.len)
	{
		str.max_len = str.len;
		arena_pop_packed(mem, (alloc.len-str.len)*sizeof(u32));
	}
	else
	{
		arena_pop_packed(mem, alloc.mem_pushed);
	}
	
	return str;

}

utf32 utf32_i64(struct arena *mem, const i64 val)
{
	struct allocation_array alloc = arena_push_aligned_all(mem, sizeof(u32), sizeof(u32));

	utf32 str = utf32_i64_buffered(alloc.addr, alloc.len, val);
	if (str.len)
	{
		str.max_len = str.len;
		arena_pop_packed(mem, (alloc.len-str.len)*sizeof(u32));
	}
	else
	{
		arena_pop_packed(mem, alloc.mem_pushed);
	}
	
	return str;

}

utf8 utf8_empty(void)
{
	return empty_utf8;
}


utf32 utf32_empty(void)
{
	return empty_utf32;
}

utf8 utf8_alloc(struct arena *mem, const u64 bufsize)
{
	void *buf = arena_push(mem, bufsize);
	return (buf)
		? (utf8) { .len = 0, .size = bufsize, .buf = buf }
		: utf8_empty();
}

utf32 utf32_alloc(struct arena *mem, const u32 len)
{
	u32 *buf = arena_push(mem, len*sizeof(u32));
	return (buf)
		? (utf32) { .len = 0, .max_len = len, .buf = buf }
		: utf32_empty();
}

void utf8_debug_print(const utf8 str)
{
	u64 offset = 0;
	for (u64 i = 0; i < str.len; ++i)
	{
		fprintf(stderr, "%c", utf8_read_codepoint(&offset, &str, offset));
	}
	fprintf(stderr, "\n");
}

void utf32_debug_print(const utf32 str)
{
	for (u64 i = 0; i < str.len; ++i)
	{
		fprintf(stderr, "%c", str.buf[i]);
	}
	fprintf(stderr, "\n");
}

enum string_token
{
	STRING_TOKEN_INVALID,
	STRING_TOKEN_NULL,
	STRING_TOKEN_CHAR,
	STRING_TOKEN_F32,
	STRING_TOKEN_U32,
	STRING_TOKEN_U64,
	STRING_TOKEN_I32,
	STRING_TOKEN_I64,
	STRING_TOKEN_POINTER,
	STRING_TOKEN_C_STRING,
	STRING_TOKEN_KAS_STRING,
};

/* set extra depending on % parameter, token_length is always valid */
static enum string_token internal_determine_format_parameter(const char *format, u32 *token_length, u32 *extra)
{
	enum string_token type = STRING_TOKEN_INVALID;
	*token_length = 0;
	switch (format[(*token_length)++])
	{
		case '\0':
		{
			type = STRING_TOKEN_NULL;
		} break;

		case '%':
		{
			switch (format[(*token_length)++])
			{
				case 'l':
				{
					switch(format[(*token_length)++])
					{
						case 'u':
						{
							type = STRING_TOKEN_U64;
						} break;

						case 'i':
						{
							type = STRING_TOKEN_I64;
						} break;

						default:
						{
							type = STRING_TOKEN_INVALID;
						} break;
					}
				} break;

				case 'u':
				{
					type = STRING_TOKEN_U32;
				} break;

				case 'i':
				{
					type = STRING_TOKEN_I32;
				} break;

				/* decimal count into f32 (OR f64 in future) */
				case '0': 
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				{
					*extra = format[*token_length-1] - '0';
					for (; '0' <= format[*token_length] && format[*token_length] <= '9'; *token_length += 1)
					{
						*extra = (*extra << 1) + (format[*token_length] - '0');
					}

					 type = (format[(*token_length)++] == 'f')
						 ? STRING_TOKEN_F32
						 : STRING_TOKEN_INVALID;
				} break; 

				case 'f':
				{
					*extra = 7;
					type = STRING_TOKEN_F32;
				} break;

				case 'p':
				{
					type = STRING_TOKEN_POINTER;
				} break;

				case 's':
				{
					type = STRING_TOKEN_C_STRING;
				} break;

				case 'k':
				{
					type =  STRING_TOKEN_KAS_STRING;
				} break;
			}
		} break;

		default:
		{
			type = STRING_TOKEN_CHAR;
		} break;
	}

	return type;
}

utf8 utf8_format_buffered_variadic(u64 *reqsize, u8 *buf, const u64 bufsize, const char *format, va_list args) 
{
	*reqsize = 0;
	if (bufsize == 0)
	{
		return utf8_empty();
	}

	utf8 pstr;

	enum string_token token;
	u32 token_length;
	u32 len = 0;
	u32 offset = 0;
	u32 extra = 0;
	u32 cont = 1;
	while (cont)
	{
		u64 size = 0;
		token = internal_determine_format_parameter(format, &token_length, &extra);
		switch (token)
		{
			case STRING_TOKEN_NULL:
			{
				cont = 0;
			} break;

			case STRING_TOKEN_F32:
			{
				const f64 val = va_arg(args, f64);
				pstr = utf8_f64_buffered(buf + offset, bufsize - offset, extra, val);	
			} break;

			case STRING_TOKEN_U32:
			{
				const u32 val = va_arg(args, u32);
				pstr = utf8_u64_buffered(buf + offset, bufsize - offset, val);	
			} break;

			case STRING_TOKEN_U64:
			{
				const u64 val = va_arg(args, u64);
				pstr = utf8_u64_buffered(buf + offset, bufsize - offset, val);
			} break;

			case STRING_TOKEN_I32:
			{
				const i32 val = va_arg(args, i32);
				pstr = utf8_i64_buffered(buf + offset, bufsize - offset, val);	
			} break;

			case STRING_TOKEN_I64:
			{
				const i64 val = va_arg(args, i64);
				pstr = utf8_i64_buffered(buf + offset, bufsize - offset, val);	
			} break;

			case STRING_TOKEN_POINTER:
			{
				const u64 val = va_arg(args, u64);
				pstr = utf8_u64_buffered(buf + offset, bufsize - offset, val);
			} break;

			case STRING_TOKEN_C_STRING:
			{
				const char *cstr = va_arg(args, char *);
				pstr = utf8_cstr_buffered(buf + offset, bufsize - offset, cstr);
			} break;

			case STRING_TOKEN_KAS_STRING:
			{
				const utf8 *kfstr = va_arg(args, utf8 *);
				pstr = utf8_copy_buffered_and_return_required_size(&size, buf + offset, bufsize - offset, *kfstr);
			} break;

			case STRING_TOKEN_INVALID:
			{
				pstr = utf8_empty();	
			} break;

			case STRING_TOKEN_CHAR:
			{
				char cstr[2];
				cstr[0] = format[0];
				cstr[1] = '\0';
				pstr = utf8_cstr_buffered(buf + offset, bufsize - offset, cstr);
			} break;
		}

		if (pstr.len == 0 || !cont)
		{
			break;
		}

		if (size == 0)
		{
			size = pstr.len;
		}

		len += pstr.len;
		offset += size;
		format += token_length;
	}

	*reqsize = offset;
	utf8 kstr = 
	{ 
		.len = len,
		.size = bufsize,
		.buf = buf,
	};

	return kstr;
}

utf8 utf8_format_variadic(struct arena *mem, const char *format, va_list args)
{
	const u64 mem_left = mem->mem_left;
	void *buf = arena_push_packed(mem, mem->mem_left);
	u64 reqsize;
	utf8 kstr = utf8_format_buffered_variadic(&reqsize, buf, mem_left, format, args);

	if (kstr.len == 0)
       	{
		arena_pop_packed(mem, mem_left);
		return utf8_empty();
	}
	
	kstr.size = reqsize;
	arena_pop_packed(mem, mem_left - reqsize);
	return kstr;
}

utf8 utf8_format(struct arena *mem, const char *format, ...)
{
	const u64 mem_left = mem->mem_left;
	va_list args;
	va_start(args, format);
	void *buf = arena_push_packed(mem, mem->mem_left);
	u64 reqsize;
	utf8 kstr = utf8_format_buffered_variadic(&reqsize, buf, mem_left, format, args);
	va_end(args);

	if (kstr.len == 0)
       	{
		arena_pop_packed(mem, mem_left);
		return utf8_empty();
	}
	
	kstr.size = reqsize;
	arena_pop_packed(mem, mem_left - reqsize);
	return kstr;
}

utf8 utf8_format_buffered(u8 *buf, const u64 bufsize, const char *format, ...)
{
	u64 reqsize; 
	va_list args;
	va_start(args, format);
	utf8 kstr = utf8_format_buffered_variadic(&reqsize, buf, bufsize, format, args);
	va_end(args);

	return kstr;
}

utf8 utf8_cstr_buffered(u8 buf[], const u64 bufsize, const char *cstr)
{
	const u64 bytes = strlen(cstr);	
	utf8 ret = utf8_empty();
	if (bytes <= bufsize)
	{
		ret = (utf8) { .len = 0, .size = (u32) bufsize, .buf = buf };
		memcpy(buf, cstr, bytes);
		for (u64 offset = 0; offset < bytes; ret.len += 1)
		{
			utf8_read_codepoint(&offset, &ret, offset);
		}

	}

	return ret;
}

utf8 utf8_cstr(struct arena *mem, const char *cstr)
{
	const u64 bytes = strlen(cstr);	
	utf8 ret = utf8_alloc(mem, bytes);
	if (ret.size)
	{
		memcpy(ret.buf, cstr, bytes);
		for (u64 offset = 0; offset < bytes; ret.len += 1)
		{
			utf8_read_codepoint(&offset, &ret, offset);
		}
	}

	return ret;
}

utf32 utf32_cstr_buffered(u32 buf[], const u64 buflen, const char *cstr)
{
	const u64 bytes = strlen(cstr);	
	const utf8 str = { .len = 0, .size = bytes, .buf = (u8 *) cstr };
	utf32 ret = (utf32) { .len = 0, .max_len = buflen, .buf = buf };

	for (u64 offset = 0; offset < bytes; ret.len += 1)
	{
		if (ret.len == ret.max_len)
		{
			ret = utf32_empty();
			break;
		}
		ret.buf[ret.len] = utf8_read_codepoint(&offset, &str, offset);
	}

	return ret;
}

utf32 utf32_cstr(struct arena *mem, const char *cstr)
{
	struct allocation_array alloc = arena_push_aligned_all(mem, sizeof(u32), sizeof(u32));	
	utf32 ret = utf32_cstr_buffered(alloc.addr, alloc.len, cstr);
	
	(ret.len)
		? arena_pop_packed(mem, sizeof(u32) * (ret.max_len - ret.len))
		: arena_pop_packed(mem, alloc.mem_pushed);

	ret.max_len = ret.len;
	return ret;
}

utf8 utf8_copy(struct arena *mem, const utf8 str)
{
	u64 bufsize_req = 0;
	for (u32 i = 0; i < str.len; ++i)
	{
		utf8_read_codepoint(&bufsize_req, &str, bufsize_req);
	}

	utf8 copy = utf8_alloc(mem, bufsize_req);
	if (copy.size)
	{
		memcpy(copy.buf, str.buf, bufsize_req);
		copy.len = str.len;
	}

	return copy;
}

utf8 utf8_copy_buffered(u8 buf[], const u64 bufsize, const utf8 str)
{	
	u64 tmp;
	return utf8_copy_buffered_and_return_required_size(&tmp, buf, bufsize, str);
}


utf8 utf8_copy_buffered_and_return_required_size(u64 *reqsize, u8 buf[], const u64 bufsize, const utf8 str)
{
	*reqsize = 0;
	u64 bufsize_req = 0;
	for (u32 i = 0; i < str.len; ++i)
	{
		utf8_read_codepoint(&bufsize_req, &str, bufsize_req);
	}

	utf8 copy = utf8_empty();
	if (bufsize_req <= bufsize)
	{
		*reqsize = bufsize_req;
		memcpy(buf, str.buf, bufsize_req);
		copy = (utf8) { .len = str.len, .size = bufsize, .buf = buf };
	}

	return copy;
}

utf32 utf32_copy(struct arena *mem, const utf32 str)
{
	utf32 copy = utf32_alloc(mem, str.len);
	if (copy.max_len)
	{
		memcpy(copy.buf, str.buf, str.len * sizeof(u32));
		copy.len = str.len;
	}
	
	return copy;
}

utf32 utf32_copy_buffered(u32 buf[], const u64 buflen, const utf32 str)
{
	utf32 copy = utf32_empty();
	if (str.len <= buflen)
	{
		memcpy(buf, str.buf, str.len * sizeof(u32));
		copy = (utf32) { .len = str.len, .max_len = buflen, .buf = buf };
	}
	
	return copy;
}

utf32 utf32_utf8(struct arena *mem, const utf8 str)
{
	utf32 conv = utf32_empty();
	u32 *buf = arena_push(mem, str.len*sizeof(u32));
	if (buf)
	{
		u64 offset = 0;
		for (u32 i = 0; i < str.len; ++i)
		{
			buf[i] = utf8_read_codepoint(&offset, &str, offset);	
		}
		conv = (utf32) { .len = str.len, .max_len = str.len, .buf = buf, };
	}

	return conv;
}

utf32 utf32_utf8_buffered(u32 buf[], const u64 buflen, const utf8 str)
{
	utf32 conv = utf32_empty();
	if (str.len <= buflen)
	{
		u64 offset = 0;
		for (u32 i = 0; i < str.len; ++i)
		{
			conv.buf[i] = utf8_read_codepoint(&offset, &str, offset);	
		}

		conv = (utf32) { .len = str.len, .max_len = buflen, .buf = buf, };
	}

	return conv;
}

u32 utf8_write_codepoint(u8 *buf, const u32 bufsize, const u32 codepoint)
{
	u32 len = 0;

	if (codepoint <= 0x7f && bufsize >= 1)
	{
		len = 1;
		buf[0] = (u8) (codepoint & 0x7f);
	}	
	else if (codepoint <= 0x7ff && bufsize >= 2)
	{
		len = 2;
		buf[0] = 0xc0 | (u8) ((codepoint >> 6) & 0x1f);
		buf[1] = 0x80 | (u8) (codepoint & 0x3f);
	}
	else if (codepoint <= 0xffff && bufsize >= 3)
	{
		len = 3;
		buf[0] = 0xe0 | (u8) ((codepoint >> 12) & 0x0f);
		buf[1] = 0x80 | (u8) ((codepoint >> 6) & 0x3f);
		buf[2] = 0x80 | (u8) (codepoint & 0x3f);
	}
	else if (codepoint <= 0x10ffff && bufsize >= 4)
	{
		len = 4;
		buf[0] = 0xf0 | (u8) ((codepoint >> 18) & 0x07);
		buf[1] = 0x80 | (u8) ((codepoint >> 12) & 0x3f);
		buf[2] = 0x80 | (u8) ((codepoint >> 6) & 0x3f);
		buf[3] = 0x80 | (u8) (codepoint & 0x3f);
	}

	return len;
}

utf8 utf8_utf32_buffered_and_return_required_size(u64 *reqsize, u8 buf[], const u64 bufsize, const utf32 str)
{
	*reqsize = 0;
	utf8 ret = { .buf = buf, .size = bufsize, .len = str.len };

	for (u32 i = 0; i < str.len; ++i)
	{
		const u32 len = utf8_write_codepoint(buf + *reqsize, bufsize - *reqsize, str.buf[i]);
		if (len == 0)
		{
			ret = utf8_empty();
			*reqsize = 0;
			break;
		}

		*reqsize += len;
	}

	return ret;
}

utf8 utf8_utf32_buffered(u8 buf[], const u64 bufsize, const utf32 str)
{
	u64 reqsize;
	return utf8_utf32_buffered_and_return_required_size(&reqsize, buf, bufsize, str);

}

utf8 utf8_utf32_buffered_null_terminated_and_return_required_size(u64 *reqsize, u8 buf[], const u64 bufsize, const utf32 str)
{
	utf8 ret = utf8_utf32_buffered_and_return_required_size(reqsize, buf, bufsize, str);
	if (ret.len && *reqsize < bufsize)
	{
		ret.buf[*reqsize] = '\0';	
		*reqsize += 1;
	}
	else
	{
		ret = utf8_empty();
		*reqsize = 0;
	}
	return ret;
}

utf8 utf8_utf32_buffered_null_terminated(u8 buf[], const u64 bufsize, const utf32 str)
{
	u64 reqsize;
	return utf8_utf32_buffered_null_terminated_and_return_required_size(&reqsize, buf, bufsize, str);
}

utf8 utf8_utf32(struct arena *mem, const utf32 str32)
{
	const u64 bufsize = mem->mem_left;
	u8 *buf = arena_push_packed(mem, bufsize);

	u64 reqsize;
	utf8 str = utf8_utf32_buffered_and_return_required_size(&reqsize, buf, bufsize, str32);
	if (str.len)
	{
		str.size = reqsize;
		arena_pop_packed(mem, bufsize - reqsize);
	}
	else
	{
		str = utf8_empty();
		arena_pop_packed(mem, bufsize);
	}

	return str;
}

utf8 utf8_utf32_null_terminated(struct arena *mem, const utf32 str32)
{
	const u64 bufsize = mem->mem_left;
	u8 *buf = arena_push_packed(mem, bufsize);

	u64 reqsize;
	utf8 str = utf8_utf32_buffered_null_terminated_and_return_required_size(&reqsize, buf, bufsize, str32);
	if (str.len)
	{
		str.size = reqsize;
		arena_pop_packed(mem, bufsize - reqsize);
	}
	else
	{
		str = utf8_empty();
		arena_pop_packed(mem, bufsize);
	}

	return str;
}

u32 cstr_hash(const char *cstr)
{
	const u32 len = strlen(cstr);
	u32 hash = 0;
	for (u32 i = 0; i < len; ++i)
	{
		hash += (u32) cstr[i] * (i+119);
	}
	return hash;
}

u32 utf8_hash(const utf8 str)
{
	u32 hash = 0;
	u64 offset = 0;
	for (u32 i = 0; i < str.len; ++i)
	{
		hash += (u32) utf8_read_codepoint(&offset, &str, offset) * (i+119);
	}
	return hash;
}

u32 utf8_equivalence(const utf8 str1, const utf8 str2)
{
	u32 equivalence = 1;
	u64 offset1 = 0; 
	u64 offset2 = 0;
	if (str1.len == str2.len)
	{
		for (u32 i = 0; i < str1.len; ++i)
		{
			const u32 codepoint1 = utf8_read_codepoint(&offset1, &str1, offset1);
			const u32 codepoint2 = utf8_read_codepoint(&offset2, &str2, offset2);
			if (codepoint1 != codepoint2)
			{
				equivalence = 0;
				break;
			}
		}
	}
	else
	{
		equivalence = 0;
	}

	return equivalence;
}

struct kmp_substring utf8_lookup_substring_init(struct arena *mem, const utf8 str)
{
	struct kmp_substring kmp;
	kmp.substring = utf32_utf8(mem, str);
	kmp.backtrack = arena_push(mem, kmp.substring.len*sizeof(u32));

	if (kmp.substring.len)
	{
		u32 b = U32_MAX;
		kmp.backtrack[0] = U32_MAX;
		for (u32 i = 1; i < kmp.substring.len; ++i)
		{
			while (b != U32_MAX)
			{
				if (kmp.substring.buf[i] == kmp.substring.buf[b+1])
				{
					break;
				}
				b = kmp.backtrack[b];
			}	
			
			if (kmp.substring.buf[i] == kmp.substring.buf[b+1])
			{
				b += 1;
			}

			kmp.backtrack[i] = b;
		}
	}

 	return kmp;	
}

u32 utf8_lookup_substring(struct kmp_substring *kmp_substring, const utf8 str) 
{
	if (kmp_substring->substring.len == 0)
	{
		kmp_substring->start = U32_MAX;
 		return 1;	
	}
	else if (str.len < kmp_substring->substring.len)
	{
		return 0;
	}

	u32 found = 0;
	u64 offset = 0;
	u32 si = U32_MAX;
	for (u32 i = 0; i < str.len; ++i)
	{
		const u32 codepoint = utf8_read_codepoint(&offset, &str, offset);
		while (si != U32_MAX && codepoint != kmp_substring->substring.buf[si+1])
		{
			si = kmp_substring->backtrack[si];
		}

		if (codepoint == kmp_substring->substring.buf[si+1])
		{
			si += 1;
		}

		if (si+1 == kmp_substring->substring.len)
		{
			kmp_substring->start = i - (kmp_substring->substring.len) + 1;	
			found = 1;
			break;
		}
	}

	return found;
}
