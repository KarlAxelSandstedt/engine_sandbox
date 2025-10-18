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

#include "sys_public.h"
#include "kas_vector.h"

DEFINE_STACK(u64);
DEFINE_STACK(u32);
DEFINE_STACK(f32);
DEFINE_STACK(ptr);
DEFINE_STACK(intv);

DEFINE_STACK_VEC(vec4);

static const struct vector empty = { 0 };

struct vector vector_alloc(struct arena *mem, const u64 blocksize, const u32 length, const u32 growable)
{
	kas_assert(length && blocksize);

	struct vector v =
	{
		.length = length,
		.blocksize = blocksize,
		.next = 0,
		.growable = growable,
	};

	v.data = (mem)
		? arena_push(mem, blocksize*length)
		: malloc(blocksize*length);

	if (!v.data)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to allocate vector");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}

	ALLOCATOR_DEBUG_INDEX_ALLOC(&v, v.data, v.length, v.blocksize, 0);

	return v;
}

void vector_dealloc(struct vector *v)
{
	ALLOCATOR_DEBUG_INDEX_FREE(v);
	free(v->data);
}

struct allocation_slot vector_push(struct vector *v)
{
	if (v->next >= v->length)
	{
		if (v->growable)
		{
			v->length *= 2;
			v->data = realloc(v->data, v->length*v->blocksize);
			if (!v->data)
			{
				log_string(T_SYSTEM, S_FATAL, "Failed to resize vector");
				fatal_cleanup_and_exit(kas_thread_self_tid());
			}

			ALLOCATOR_DEBUG_INDEX_ALIAS_AND_REPOISON(v, v->data, v->length);
		}
		else
		{
			return (struct allocation_slot) { .index = 0, .address = NULL };
		}
	}

	ALLOCATOR_DEBUG_INDEX_UNPOISON(v, v->next);
	struct allocation_slot slot = { .address = vector_address(v, v->next) };
	slot.index = v->next;
	v->next += 1;
	return slot;
}

void vector_pop(struct vector *v)
{
	kas_assert(v->next);
	v->next -= 1;
	ALLOCATOR_DEBUG_INDEX_POISON(v, v->next);
}

void *vector_address(const struct vector *v, const u32 index)
{
	return v->data + v->blocksize*index;
}

void vector_flush(struct vector *v)
{
	ALLOCATOR_DEBUG_INDEX_FLUSH(v);
	v->next = 0;
}

