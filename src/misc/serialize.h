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

#ifndef __KAS_SERIALIZE_H__
#define __KAS_SERIALIZE_H__

#include "kas_common.h"
#include "allocator.h"

/*
   ====== SERIALIZE_STREAM PARTIAL BYTE READING ======

 	  When we want to read n < 8 bits from a byte, we read from the "logical high bit downwards, i.e. 
 	  		
    	  					         HEAD
    	 						  V
 	     		      | b7 b6 b5 b4 b3 b2 b1 b0 | b7 b6 b5 b4 b3 b2 b1 b0 |  
 	     		      | 	Byte k  	|  	Byte k+1	  |
 	 
 	  read(4) = 0 0 0 0 b7 b6 b5 b4;
 	 	    =>  			                     HEAD
 	 	             			              	      V
 	 	       	      | b7 b6 b5 b4 b3 b2 b1 b0 | b7 b6 b5 b4 b3 b2 b1 b0 | 
 	 	              | 	Byte k  	|  	Byte k+1          |
 	 
 	 
   ====== SERIALIZE_STREAM WRITING INTERNALS ======
   	
   	(1) FULL ALIGNED BYTES: We read/write a full primitive (b8, b16, b32, b64) to a byte aligned address:
  
   		Implementation Example:
			Write le16:	
				((b16 *) (ss->buf + offset)) = na_to_le_16(val);
        		Read: b16 val = 
				le_to_na_16(*((b16 *) (ss->buf + offset)));
  
		In the write example, from our register's perspective what happens is:
		
					         B1			       B0	
			Register  [ b15 b14 b13 b12 b11 b10 b9 b8 | b7 b6 b5 b4 b3 b2 b1 b0 ]
		
			=>	Register(LE)  [ b15 b14 b13 b12 b11 b10 b9 b8 | b7 b6 b5 b4 b3 b2 b1 b0 ]
			=>	Register(BE)  [ b8 b7 b6 b5 b4 b3 b2 b1 b0 | b15 b14 b13 b12 b11 b10 b9 ]	(manual invert byte order)
		
		 *buf[0,1] = register;
			=>	buf[0|1] = [ b8 b7 b6 b5 b4 b3 b2 b1 b0 | b15 b14 b13 b12 b11 b10 b9 ]	(LE)
			=>	buf[0|1] = [ b8 b7 b6 b5 b4 b3 b2 b1 b0 | b15 b14 b13 b12 b11 b10 b9 ]  (BE)
		
			I.e. LE_write(register) => invert logical byte order
			I.e. BE_write(register) =>        logical byte order

		similarly, in the read example, from our register's perspective what happens is:
		
				buf[0|1] = [ b8 b7 b6 b5 b4 b3 b2 b1 b0 | b15 b14 b13 b12 b11 b10 b9 ]

		 register = *buf[0|1];
			=>	register = [ b15 b14 b13 b12 b11 b10 b9 b8 | b7 b6 b5 b4 b3 b2 b1 b0 ] (LE)
			=>	register = [ b8 b7 b6 b5 b4 b3 b2 b1 b0 | b15 b14 b13 b12 b11 b10 b9 ] (BE)

		manual flipping of the BE case give the correct register.

   	(2) FULL UNALIGNED BITS: Exactly the same as the FULL ALIGNED BYTES case, except each byte (order depending or le/be write/read)
	    		       is possibly straddling the serialize stream bytes.

		Example be16 full write:
			       stream:	[ b7  b6  b5  b4  b3  b2  b1  b0 | b7  b6  b5  b4  b3 b2 b1 b0 | b7 b6 b5 b4 b3 b2 b1 b0 ]
			       b16: 			[ b15 b14 b13 b12  b11 b10 b9 b8 | b7 b6 b5 b4   b3 b2 b1 b0 ]


   	(3) PARTIAL BITS: We write a a number of bits not divisible by 8.  Here is where ambiguity may appear; suppose we write 12 bits
	using write16_partial_le. 
       
				  B1			      B0
		Register [ x x x x b11 b10 b9 b8 | b7 b6 b5 b4 b3 b2 b1 b0 ]

		Should the partial byte be stored at the start or at the end of the stream? i.e. should the state of the stream be 

			
	     Write happened here			Head after write 
		      V					      V
		[ ... b3 b2 b1 b0 | b11 b10 b9 b8 b7 b6 b5 b4 x x x .... ]	(partial byte at the beginning)
		[ ... b7 b6 b5 b4 b3 b2 b1 b0 | b11 b10 b9 b8 x x x .... ]	(partial byte at the end)

	It seems more reasonable to store the partial byte at the end, so we shall go with that convetion.

   ====== SERIALIZE_STREAM SIGNED INTEGER WRITING ======
	
	Suppose we want to write n bits of a signed integer value. It is reasonable to store the sign bit as the most significant bit
	and the rest of the n-1 bits in the usual order. Suppose and we wish to write a signed 16bit integer variable. Furthermore, 
	suppose we know the 10 most significat bits of our variable contains the sign bit(-64 <= variable <= 63). Then we can store the
	integer as follows:

	Integer :  [ S S S S S S S S | S S b5 b4 b3 b2 b1 b0 ]
	Register : [ x x x x x x x x | x S b5 b4 b3 b2 b1 b0 ]

	We can now write it in partial le/be fashion, and when we read it back, we may expand the sign in the register to retrieve
	the signed integer.

	Register : [ S S S S S S S S | S S b5 b4 b3 b2 b1 b0 ]

 */
 struct serialize_stream
{
	u64 bit_index;	
	u64 bit_count;
	u8 *buf;
};

/* allocates a stream on the arena if mem != NULL, otherwise malloced. On failure, we return stream = { 0 } */
struct serialize_stream ss_alloc(struct arena *mem, const u64 bufsize);
/* initiates a stream that aliases the buf */
struct serialize_stream ss_buffered(void *buf, const u64 bufsize);
/* free stream resources (when using malloc) */
void 			ss_free(struct serialize_stream *ss);
/* return number of whole bytes left */
u64			ss_bytes_left(const struct serialize_stream *ss);
/* return number of bits left */
u64			ss_bits_left(const struct serialize_stream *ss);


/*
 * TODO: CHECK / TRUNCATE b8 val upper bits to zero that are not to be written in functions calling ss_write****
 * 	This simplifies logic in write methods
 */

/*	read / write aligned byte(s): 
 *		unaligned read/writes are unhandled ERRORS! 
 *		buffer overruns are unhandled ERRORS! 
 */
b8 	ss_read8(struct serialize_stream *ss);
void 	ss_write8(struct serialize_stream *ss, const b8 val);
b16 	ss_read16_le(struct serialize_stream *ss);
void 	ss_write16_le(struct serialize_stream *ss, const b16 val);
b16 	ss_read16_be(struct serialize_stream *ss);
void 	ss_write16_be(struct serialize_stream *ss, const b16 val);
b32 	ss_read32_le(struct serialize_stream *ss);
void 	ss_write32_le(struct serialize_stream *ss, const b32 val);
b32 	ss_read32_be(struct serialize_stream *ss);
void 	ss_write32_be(struct serialize_stream *ss, const b32 val);
b64 	ss_read64_le(struct serialize_stream *ss);
void 	ss_write64_le(struct serialize_stream *ss, const b64 val);
b64 	ss_read64_be(struct serialize_stream *ss);
void 	ss_write64_be(struct serialize_stream *ss, const b64 val);

#define     ss_read_u8(ss)      		ss_read_u8(ss).u
#define     ss_write_u8(ss, val)      		ss_write_u8(ss, (b8) { .u = val })
#define     ss_read_u16_le(ss)          	ss_read16_le(ss).u
#define     ss_write_u16_le(ss, val)    	ss_write16_le(ss, (b16) { .u = val })
#define     ss_read_u16_be(ss)          	ss_read16_be(ss).u
#define     ss_write_u16_be(ss, val)    	ss_write16_be(ss, (b16) { .u = val })
#define     ss_read_u32_le(ss)          	ss_read32_le(ss).u
#define     ss_write_u32_le(ss, val)    	ss_write32_le(ss, (b32) { .u = val })
#define     ss_read_u32_be(ss)          	ss_read32_be(ss).u
#define     ss_write_u32_be(ss, val)    	ss_write32_be(ss, (b32) { .u = val })
#define     ss_read_u64_le(ss)          	ss_read64_le(ss).u
#define     ss_write_u64_le(ss, val)    	ss_write64_le(ss, (b64) { .u = val })
#define     ss_read_u64_be(ss)          	ss_read64_be(ss).u
#define     ss_write_u64_be(ss, val)    	ss_write64_be(ss, (b64) { .u = val })

#define     ss_read_i8(ss)      		ss_read_i8(ss).i
#define     ss_write_i8(ss, val)      		ss_write_i8(ss, (b8) { .i = val })
#define     ss_read_i16_le(ss)          	ss_read16_le(ss).i
#define     ss_write_i16_le(ss, val)    	ss_write16_le(ss, (b16) { .i = val })
#define     ss_read_i16_be(ss)          	ss_read16_be(ss).i
#define     ss_write_i16_be(ss, val)    	ss_write16_be(ss, (b16) { .i = val })
#define     ss_read_i32_le(ss)          	ss_read32_le(ss).i
#define     ss_write_i32_le(ss, val)    	ss_write32_le(ss, (b32) { .i = val })
#define     ss_read_i32_be(ss)          	ss_read32_be(ss).i
#define     ss_write_i32_be(ss, val)    	ss_write32_be(ss, (b32) { .i = val })
#define     ss_read_i64_le(ss)          	ss_read64_le(ss).i
#define     ss_write_i64_le(ss, val)    	ss_write64_le(ss, (b64) { .i = val })
#define     ss_read_i64_be(ss)          	ss_read64_be(ss).i
#define     ss_write_i64_be(ss, val)    	ss_write64_be(ss, (b64) { .i = val })

#define     ss_read_f32_le(ss)          	ss_read32_le(ss).f
#define     ss_write_f32_le(ss, val)    	ss_write32_le(ss, (b32) { .f = val })
#define     ss_read_f32_be(ss)          	ss_read32_be(ss).f
#define     ss_write_f32_be(ss, val)    	ss_write32_be(ss, (b32) { .f = val })
#define     ss_read_f64_le(ss)          	ss_read64_le(ss).f
#define     ss_write_f64_le(ss, val)    	ss_write64_le(ss, (b64) { .f = val })
#define     ss_read_f64_be(ss)          	ss_read64_be(ss).f
#define     ss_write_f64_be(ss, val)    	ss_write64_be(ss, (b64) { .f = val })

/*	read / write aligned byte(s): 
 *		unaligned read/writes are unhandled ERRORS! 
 *		buffer overruns are unhandled ERRORS! 
 */
void	ss_read8_array(b8 *buf, struct serialize_stream *ss, const u64 len);
void 	ss_write8_array(struct serialize_stream *ss, const b8 *buf, const u64 len);
void 	ss_read16_le_array(b16 *buf, struct serialize_stream *ss, const u64 len);
void 	ss_write16_le_array(struct serialize_stream *ss, const b16 *buf, const u64 len);
void 	ss_read16_be_array(b16 *buf, struct serialize_stream *ss, const u64 len);
void 	ss_write16_be_array(struct serialize_stream *ss, const b16 *buf, const u64 len);
void 	ss_read32_le_array(b32 *buf, struct serialize_stream *ss, const u64 len);
void 	ss_write32_le_array(struct serialize_stream *ss, const b32 *buf, const u64 len);
void 	ss_read32_be_array(b32 *buf, struct serialize_stream *ss, const u64 len);
void 	ss_write32_be_array(struct serialize_stream *ss, const b32 *buf, const u64 len);
void 	ss_read64_le_array(b64 *buf, struct serialize_stream *ss, const u64 len);
void 	ss_write64_le_array(struct serialize_stream *ss, const b64 *buf, const u64 len);
void	ss_read64_be_array(b64 *buf, struct serialize_stream *ss, const u64 len);
void 	ss_write64_be_array(struct serialize_stream *ss, const b64 *buf, const u64 len);

#define 	ss_read_u8_array(buf, ss, len)			ss_read8_array((b8 *) (buf), ss, len)
#define 	ss_write_u8_array(ss, buf, len)			ss_write8_array(ss, (b8 *) (buf), len)
#define 	ss_read_u16_le_array(buf, ss, len) 		ss_read16_le_array((b16 *) (buf), ss, len)
#define 	ss_write_u16_le_array(ss, buf, len) 		ss_write16_le_array((ss, b16 *) (buf), len)
#define 	ss_read_u16_be_array(buf, ss, len) 		ss_read16_be_array((b16 *) (buf), ss, len)
#define 	ss_write_u16_be_array(ss, buf, len) 		ss_write16_be_array(ss, (b16 *) (buf), len)
#define 	ss_read_u32_le_array(buf, ss, len) 		ss_read32_le_array((b32 *) (buf), ss, len)
#define 	ss_write_u32_le_array(ss, buf, len) 		ss_write32_le_array(ss, (b32 *) (buf), len)
#define 	ss_read_u32_be_array(buf, ss, len) 		ss_read32_be_array((b32 *) (buf), ss, len)
#define 	ss_write_u32_be_array(ss, buf, len) 		ss_write32_be_array(ss, (b32 *) (buf), len)
#define 	ss_read_u64_le_array(buf, ss, len) 		ss_read64_le_array((b64 *) (buf), ss, len)
#define 	ss_write_u64_le_array(ss, buf, len) 		ss_write64_le_array(ss, (b64 *) (buf), len)
#define 	ss_read_u64_be_array(buf, ss, len)		ss_read64_be_array((b64 *) (buf), ss, len)
#define 	ss_write_u64_be_array(ss, buf, len) 		ss_write64_be_array(ss, (b64 *) (buf), len)

#define 	ss_read_i8_array(buf, ss, len)			ss_read8_array((b8 *) (buf), ss, len)
#define 	ss_write_i8_array(ss, buf, len)			ss_write8_array(ss, (b8 *) (buf), len)
#define 	ss_read_i16_le_array(buf, ss, len) 		ss_read16_le_array((b16 *) (buf), ss, len)
#define 	ss_write_i16_le_array(ss, buf, len)		ss_write16_le_array(ss, (b16 *) (buf), len)
#define 	ss_read_i16_be_array(buf, ss, len) 		ss_read16_be_array((b16 *) (buf), ss, len)
#define 	ss_write_i16_be_array(ss, buf, len)		ss_write16_be_array(ss, (b16 *) (buf), len)
#define 	ss_read_i32_le_array(buf, ss, len) 		ss_read32_le_array((b32 *) (buf), ss, len)
#define 	ss_write_i32_le_array(ss, buf, len)		ss_write32_le_array(ss, (b32 *) (buf), len)
#define 	ss_read_i32_be_array(buf, ss, len) 		ss_read32_be_array((b32 *) (buf), ss, len)
#define 	ss_write_i32_be_array(ss, buf, len)		ss_write32_be_array(ss, (b32 *) (buf), len)
#define 	ss_read_i64_le_array(buf, ss, len) 		ss_read64_le_array((b64 *) (buf), ss, len)
#define 	ss_write_i64_le_array(ss, buf, len)		ss_write64_le_array(ss, (b64 *) (buf), len)
#define 	ss_read_i64_be_array(buf, ss, len)		ss_read64_be_array((b64 *) (buf), ss, len)
#define 	ss_write_i64_be_array(ss, buf, len)		ss_write64_be_array(ss, (b64 *) (buf), len)

#define 	ss_read_f32_le_array(buf, ss, len) 		ss_read32_le_array((b32 *) (buf), ss, len)
#define 	ss_write_f32_le_array(ss, buf, len)		ss_write32_le_array(ss, (b32 *) (buf), len)
#define 	ss_read_f32_be_array(buf, ss, len) 		ss_read32_be_array((b32 *) (buf), ss, len)
#define 	ss_write_f32_be_array(ss, buf, len)		ss_write32_be_array(ss, (b32 *) (buf), len)
#define 	ss_read_f64_le_array(buf, ss, len) 		ss_read64_le_array((b64 *) (buf), ss, len)
#define 	ss_write_f64_le_array(ss, buf, len)		ss_write64_le_array(ss, (b64 *) (buf), len)
#define 	ss_read_f64_be_array(buf, ss, len)		ss_read64_be_array((b64 *) (buf), ss, len)
#define 	ss_write_f64_be_array(ss, buf, len)		ss_write64_be_array(ss, (b64 *) (buf), len)

/*	read / write bit(s): 
 *		buffer overruns are unhandled ERRORS! 
 */
void ss_write_u64_le_partial(struct serialize_stream *ss, const u64 val, const u64 bit_count);
#define ss_write_u32_le_partial(ss, val, bit_count)	ss_write_u64_le_partial(ss, val, bit_count)
#define ss_write_u16_le_partial(ss, val, bit_count)	ss_write_u64_le_partial(ss, val, bit_count)
void ss_write_u64_be_partial(struct serialize_stream *ss, const u64 val, const u64 bit_count);
#define ss_write_u32_be_partial(ss, val, bit_count)	ss_write_u64_be_partial(ss, val, bit_count)
#define ss_write_u16_be_partial(ss, val, bit_count)	ss_write_u64_be_partial(ss, val, bit_count)
#define ss_write_u8_partial(ss, val, bit_count)		ss_write_u64_be_partial(ss, val, bit_count)

u64 ss_read_u64_le_partial(struct serialize_stream *ss, const u64 bit_count);
#define ss_read_u32_le_partial(ss, bit_count)		((u32) ss_read_u64_le_partial(ss, bit_count));
#define ss_read_u16_le_partial(ss, bit_count)		((u16) ss_read_u64_le_partial(ss, bit_count));
u64 ss_read_u64_be_partial(struct serialize_stream *ss, const u64 bit_count);
#define ss_read_u32_be_partial(ss, bit_count)		((u32) ss_read_u64_be_partial(ss, bit_count));
#define ss_read_u16_be_partial(ss, bit_count)		((u16) ss_read_u64_be_partial(ss, bit_count));
#define ss_read_u8_partial(ss, bit_count)		((u8) ss_read_u64_be_partial(ss, bit_count));

void ss_write_i64_le_partial(struct serialize_stream *ss, const i64 val, const u64 bit_count);
#define ss_write_i32_le_partial(ss, val, bit_count)	ss_write_i64_le_partial(ss, val, bit_count)
#define ss_write_i16_le_partial(ss, val, bit_count)	ss_write_i64_le_partial(ss, val, bit_count)
void ss_write_i64_be_partial(struct serialize_stream *ss, const i64 val, const u64 bit_count);
#define ss_write_i32_be_partial(ss, val, bit_count)	ss_write_i64_be_partial(ss, val, bit_count)
#define ss_write_i16_be_partial(ss, val, bit_count)	ss_write_i64_be_partial(ss, val, bit_count)
#define ss_write_i8_partial(ss, val, bit_count)		ss_write_i64_be_partial(ss, val, bit_count)

i64 ss_read_i64_le_partial(struct serialize_stream *ss, const u64 bit_count);
#define ss_read_i32_le_partial(ss, bit_count)		((i32) ss_read_i64_le_partial(ss, bit_count));
#define ss_read_i16_le_partial(ss, bit_count)		((i16) ss_read_i64_le_partial(ss, bit_count));
i64 ss_read_i64_be_partial(struct serialize_stream *ss, const u64 bit_count);
#define ss_read_i32_be_partial(ss, bit_count)		((i32) ss_read_i64_be_partial(ss, bit_count));
#define ss_read_i16_be_partial(ss, bit_count)		((i16) ss_read_i64_be_partial(ss, bit_count));
#define ss_read_i8_partial(ss, bit_count)		((i8) ss_read_i64_be_partial(ss, bit_count));

#endif
