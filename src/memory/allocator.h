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

#ifndef __KAS_ALLOCATOR_H__
#define __KAS_ALLOCATOR_H__

#include "kas_common.h"

/************************************* arena allocator  *************************************/

#define DEFAULT_MEMORY_ALIGNMENT	((u64) 8)

struct allocation_array
{
	void *	addr;
	u64	len;
	u64	mem_pushed;	/* NOTE: recorded pushed number of bytes to be used in arena_pop_packed(*) */
};

struct arena_record
{
	struct arena_record *	prev;
	u64 			rec_mem_left;
};

/* arena - Contiguous memory aligned to MEMORY_ALIGNMENT. Any allocation, unless specifically packed, 
 * 	   is aligned to DEFAULT_MEMORY_ALIGNMENT */
struct arena 
{
	u8 * 			stack_ptr;
	u64 			mem_size;
	u64 			mem_left;
	struct arena_record *	record;		/* NULL == no record */
};

/* setup arena using global block allocator */
struct arena	arena_alloc_1MB(void);
/*  global block allocator free wrapper */
void		arena_free_1MB(struct arena *mem);

/* Record arena memory position */
void		arena_push_record(struct arena *ar);
/* Return to last recorded memory position, given that recorded mem_left >= current mem_left. */
void		arena_pop_record(struct arena *ar);
/* Remove last recorded memory position */
void 		arena_remove_record(struct arena *ar);

/* If allocation failed, return arena = { 0 } */
struct arena	arena_alloc(const u64 size);
/* free heap memory and set *ar = empty_arena */
void		arena_free(struct arena *ar);
/* flush contents, reset stack to start of stack */
void		arena_flush(struct arena* ar);	
/* pop arena memory */
void 		arena_pop_packed(struct arena *ar, const u64 mem_to_pop);

/* Return address to aligned data of given size on success, otherwise return NULL. */
void *		arena_push_aligned(struct arena *ar, const u64 size, const u64 alignment);
/* Return address to zeroed-out aligned data of given size on success, otherwise return NULL. */
void *		arena_push_aligned_zero(struct arena *ar, const u64 size, const u64 alignment);
/* Return address to aligned data of given size and copy [size] bytes of copy's content into it on success, 
 * otherwise return NULL. */
void *		arena_push_aligned_memcpy(struct arena *ar, const void *copy, const u64 size, const u64 alignment);
/* arena_push_alignement but we push the whole arena given a slot size, and return the number of slots aquired.  */
struct allocation_array	arena_push_aligned_all(struct arena *ar, const u64 slot_size, const u64 alignment);

#define		arena_push_packed(ar, size)			arena_push_aligned(ar, size, 1)
#define		arena_push_packed_zero(ar, size)		arena_push_aligned_zero(ar, size, 1)
#define		arena_push_packed_memcpy(ar, copy, size)	arena_push_aligned_memcpy(ar, copy, size, 1)

#define		arena_push(ar, size)				arena_push_aligned(ar, size, DEFAULT_MEMORY_ALIGNMENT)
#define		arena_push_zero(ar, size)			arena_push_aligned_zero(ar, size, DEFAULT_MEMORY_ALIGNMENT)
#define		arena_push_memcpy(ar, copy, size)		arena_push_aligned_memcpy(ar, copy, size, DEFAULT_MEMORY_ALIGNMENT)

/************************************* Thread-Safe block allocator  *************************************/

struct thread_block_allocator
{
	/* pads for 64 and 128 cachelines */
	u64	a_next;
	u8	pad2[120];
	u8 *	block;
	u64	block_size;
	u64	max_count;
	
	//TODO parallel 
	//ALLOCATOR_DEBUG_PARALLEL_INDEX_STRUCT
};

/*==== User defined global block allocators ====*/

/* allocate global block allocators */
void	global_thread_block_allocators_alloc(const u32 count_256B, const u32 count_1MB);
/* free global block allocators */
void	global_thread_block_allocators_free(void);
/* returns a 256B cache aligned block on success, NULL on out-of-memory */
void *	thread_alloc_256B(void);
/* returns a 1MB cache aligned block on success, NULL on out-of-memory */
void *	thread_alloc_1MB(void);
/* free a 256B block */
void 	thread_free_256B(void *addr);
/* free a 1MB block */
void 	thread_free_1MB(void *addr);

/*==== block allocater ====*/

/* reserve virtual memory pages and initiate allocator */
struct thread_block_allocator *	thread_block_allocator_alloc(const u64 block_count, const u64 block_size);
/* reserve virtual memory pages and initiate allocator */
void				thread_block_allocator_free(struct thread_block_allocator *allocator);
/* Returns pointer to requested block, or NULL on out of memory */
void *				thread_block_alloc(struct thread_block_allocator *allocator);
/* Free block */
void  				thread_block_free(struct thread_block_allocator *allocator, void *addr);

/************************************* ring allocator  *************************************/


/* virtual memory wrapped ring buffer. */
struct ring
{
	u64 mem_total;
	u64 mem_left;
	u64 offset;	/* offset to write from base pointer buf */
	u8 *buf;	
};

/* return an empty ring */
struct ring 		ring_empty(void);	
/* Allocated virtual memory wrapped ring buffer using mem_hint as a minimum memsize.
 * The final size depends on the page size of the underlying system. Returns the
 * allocated ring allocator on SUCCESS, or an empty allocator on FAILURE.*/
struct ring 		ring_alloc(const u64 mem_hint);	
/* free ring allocator resources */
void 			ring_free(struct ring *ring);			
/* flush ring memory and set offset to 0 */
void			ring_flush(struct ring *ring);
/* return allocaction[size], and do not advance the ring write offset on success; empty buffer on FAILURE. */
struct kas_buffer 	ring_push_start(struct ring *ring, const u64 size);
/* return allocaction[size], and advance the ring write offset on success; empty buffer on FAILURE. */
struct kas_buffer 	ring_push_end(struct ring *ring, const u64 size);
/* release bytes in ring in fifo order. */
void 			ring_pop_start(struct ring *ring, const u64 size); 
/* release bytes in ring in lifo order. */
void 			ring_pop_end(struct ring *ring, const u64 size); 


#endif
