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

#ifndef __KAS_VECTOR_H__
#define __KAS_VECTOR_H__

#include "kas_common.h"
#include "allocator.h"
#include "allocator_debug.h"

/****************************************** general vector ******************************************/

#define VECTOR_STATIC		0
#define VECTOR_GROWABLE		1

/*
 * struct vector: Simple stack-based array, i.e. all of its contiguous memory up until data[next] is valid.
 */
struct vector
{
	u64	blocksize;	/* size of individual block 	*/
	u8 *	data;		/* memory address base 		*/
	u32 	length;		/* memory length (in blocks)	*/
	u32 	next;		/* next index to be pushed 	*/ 
	u32	growable;	/* Boolean: is memory growable  */

	ALLOCATOR_DEBUG_INDEX_STRUCT;
};

/* allocate and initalize vector: If mem is defined, use arena allocator; given that growable == VECTOR_STATIC */
struct vector		vector_alloc(struct arena *mem, const u64 blocksize, const u32 length, const u32 growable);
/* deallocate vector heap memory */
void			vector_dealloc(struct vector *v);
/* push block, return index and address on success, otherwise return (0, NULL) */
struct allocation_slot	vector_push(struct vector *v);
/* pop block  */
void			vector_pop(struct vector *v);
/* return address of indexed block */
void *			vector_address(const struct vector *v, const u32 index);
/* pop all allocated blocks */
void			vector_flush(struct vector *v);

/****************************************** FIXED TYPE STACK GENERATION ******************************************/

#define STACK_GROWABLE	1

#define DECLARE_STACK_STRUCT(type)	\
typedef struct				\
{					\
	u32 	length;			\
	u32 	next;			\
	u32	growable;		\
	type *	arr;			\
					\
	/*ALLOCATOR_DEBUG_INDEX_STRUCT*/	\
} stack_ ## type

#define DECLARE_STACK_ALLOC(type)	stack_ ## type stack_ ## type ## _alloc(struct arena *arena, const u32 length, const u32 growable)
#define DECLARE_STACK_FREE(type)	void stack_ ## type ## _free(stack_ ## type *stack)
#define DECLARE_STACK_PUSH(type)	void stack_ ## type ## _push(stack_ ## type *stack, const type val)
#define DECLARE_STACK_SET(type)		void stack_ ## type ## _set(stack_ ## type *stack, const type val)
#define DECLARE_STACK_POP(type)		type stack_ ## type ## _pop(stack_ ## type *stack)
#define DECLARE_STACK_TOP(type)		type stack_ ## type ## _top(stack_ ## type *stack)

#define DECLARE_STACK(type)		\
	DECLARE_STACK_STRUCT(type);	\
	DECLARE_STACK_ALLOC(type);	\
	DECLARE_STACK_PUSH(type);	\
	DECLARE_STACK_POP(type);	\
	DECLARE_STACK_TOP(type);	\
	DECLARE_STACK_SET(type);	\
	DECLARE_STACK_FREE(type)

#define DEFINE_STACK_ALLOC(type)									\
DECLARE_STACK_ALLOC(type)										\
{													\
	stack_ ## type stack =										\
	{												\
		.length = length,									\
		.next = 0,										\
		.growable = growable,									\
	};												\
	stack.arr = (arena)										\
		? arena_push(arena, sizeof(type)*length)						\
		: malloc(sizeof(type)*length);								\
	if (length > 0 && !stack.arr)									\
	{												\
		fatal_cleanup_and_exit(kas_thread_self_tid());						\
	}												\
	/*ALLOCATOR_DEBUG_INDEX_ALLOC(&stack, (u8 *) stack.arr, stack.length, sizeof(type), 0);*/	\
	return stack;											\
}

#define DEFINE_STACK_FREE(type)			\
DECLARE_STACK_FREE(type)			\
{						\
	/*ALLOCATOR_DEBUG_INDEX_FREE(stack);*/	\
	if (stack->arr)				\
	{					\
		free(stack->arr);		\
	}					\
}

#define DEFINE_STACK_PUSH(type)											\
DECLARE_STACK_PUSH(type)											\
{														\
	if (stack->next >= stack->length)									\
	{													\
		if (stack->growable)										\
		{												\
			stack->length *= 2;									\
			stack->arr = realloc(stack->arr, stack->length*sizeof(stack->arr[0]));			\
			if (!stack->arr)									\
			{											\
				fatal_cleanup_and_exit(kas_thread_self_tid());					\
			}											\
			/*ALLOCATOR_DEBUG_INDEX_ALIAS_AND_REPOISON(stack, (u8 *) stack->arr, stack->length);*/	\
		}												\
		else												\
		{												\
			fatal_cleanup_and_exit(kas_thread_self_tid());						\
		}												\
	}													\
	/*ALLOCATOR_DEBUG_INDEX_UNPOISON(stack, stack->next);*/							\
	stack->arr[stack->next] = val;										\
	stack->next += 1;											\
}

#define DEFINE_STACK_SET(type)			\
DECLARE_STACK_SET(type)				\
{						\
	kas_assert(stack->next);		\
	stack->arr[stack->next-1] = val;	\
}

#define DEFINE_STACK_POP(type)					\
DECLARE_STACK_POP(type)						\
{								\
	kas_assert(stack->next);				\
	stack->next -= 1;					\
	const type val = stack->arr[stack->next];		\
	/*ALLOCATOR_DEBUG_INDEX_POISON(stack, stack->next);*/	\
	return val;						\
}

#define DEFINE_STACK_TOP(type)			\
DECLARE_STACK_TOP(type)				\
{						\
	kas_assert(stack->next);		\
	return stack->arr[stack->next-1];	\
}

#define DEFINE_STACK(type)		\
	DEFINE_STACK_ALLOC(type)	\
	DEFINE_STACK_PUSH(type)		\
	DEFINE_STACK_POP(type)		\
	DEFINE_STACK_TOP(type)		\
	DEFINE_STACK_SET(type)		\
	DEFINE_STACK_FREE(type)

#define DECLARE_STACK_VEC_STRUCT(vectype)\
typedef struct				\
{					\
	u32 		length;		\
	u32 		next;		\
	u32		growable;	\
	vectype ## ptr	arr;		\
					\
	/*ALLOCATOR_DEBUG_INDEX_STRUCT*/	\
} stack_ ## vectype

#define DECLARE_STACK_VEC_ALLOC(vectype)	stack_ ## vectype stack_ ## vectype ## _alloc(struct arena *arena, const u32 length, const u32 growable)
#define DECLARE_STACK_VEC_FREE(vectype)		void stack_ ## vectype ## _free(stack_ ## vectype *stack)
#define DECLARE_STACK_VEC_PUSH(vectype)		void stack_ ## vectype ## _push(stack_ ## vectype *stack, const vectype val)
#define DECLARE_STACK_VEC_SET(vectype)		void stack_ ## vectype ## _set(stack_ ## vectype *stack, const vectype val)
#define DECLARE_STACK_VEC_POP(vectype)		void stack_ ## vectype ## _pop(stack_ ## vectype *stack)
#define DECLARE_STACK_VEC_TOP(vectype)		void stack_ ## vectype ## _top(vectype ret_val, stack_ ## vectype *stack)

#define DECLARE_STACK_VEC(vectype)		\
	DECLARE_STACK_VEC_STRUCT(vectype);	\
	DECLARE_STACK_VEC_ALLOC(vectype);	\
	DECLARE_STACK_VEC_PUSH(vectype);	\
	DECLARE_STACK_VEC_POP(vectype);		\
	DECLARE_STACK_VEC_TOP(vectype);		\
	DECLARE_STACK_VEC_SET(vectype);		\
	DECLARE_STACK_VEC_FREE(vectype)

#define DEFINE_STACK_VEC_ALLOC(vectype)									\
DECLARE_STACK_VEC_ALLOC(vectype)									\
{													\
	stack_ ## vectype stack =									\
	{												\
		.length = length,									\
		.next = 0,										\
		.growable = growable,									\
	};												\
	stack.arr = (arena)										\
		? arena_push(arena, sizeof(vectype)*length)					\
		: malloc(sizeof(vectype)*length);							\
	if (length > 0 && !stack.arr)									\
	{												\
		fatal_cleanup_and_exit(kas_thread_self_tid());						\
	}												\
	/*ALLOCATOR_DEBUG_INDEX_ALLOC(&stack, (u8 *) stack.arr, stack.length, sizeof(vectype), 0);*/	\
	return stack;											\
}

#define DEFINE_STACK_VEC_FREE(vectype)		\
DECLARE_STACK_VEC_FREE(vectype)			\
{						\
	/*ALLOCATOR_DEBUG_INDEX_FREE(stack);*/	\
	if (stack->arr)				\
	{					\
		free(stack->arr);		\
	}					\
}

#define DEFINE_STACK_VEC_PUSH(vectype)										\
DECLARE_STACK_VEC_PUSH(vectype)											\
{														\
	if (stack->next >= stack->length)									\
	{													\
		if (stack->growable)										\
		{												\
			stack->length *= 2;									\
			stack->arr = realloc(stack->arr, stack->length*sizeof(stack->arr[0]));			\
			if (!stack->arr)									\
			{											\
				fatal_cleanup_and_exit(kas_thread_self_tid());					\
			}											\
			/*ALLOCATOR_DEBUG_INDEX_ALIAS_AND_REPOISON(stack, (u8 *) stack->arr, stack->length);*/	\
		}												\
		else												\
		{												\
			fatal_cleanup_and_exit(kas_thread_self_tid());						\
		}												\
	}													\
	/*ALLOCATOR_DEBUG_INDEX_UNPOISON(stack, stack->next);*/							\
	vectype ## _copy(stack->arr[stack->next], val);								\
	stack->next += 1;											\
}

#define DEFINE_STACK_VEC_SET(vectype)				\
DECLARE_STACK_VEC_SET(vectype)					\
{								\
	kas_assert(stack->next);				\
	vectype ## _copy(stack->arr[stack->next-1], val);	\
}

#define DEFINE_STACK_VEC_POP(vectype)				\
DECLARE_STACK_VEC_POP(vectype)					\
{								\
	kas_assert(stack->next);				\
	stack->next -= 1;					\
	/*ALLOCATOR_DEBUG_INDEX_POISON(stack, stack->next);*/	\
}

#define DEFINE_STACK_VEC_TOP(vectype)				\
DECLARE_STACK_VEC_TOP(vectype)					\
{								\
	kas_assert(stack->next);				\
	vectype ## _copy(ret_val, stack->arr[stack->next-1]);	\
}

#define DEFINE_STACK_VEC(vectype)		\
	DEFINE_STACK_VEC_ALLOC(vectype)		\
	DEFINE_STACK_VEC_PUSH(vectype)		\
	DEFINE_STACK_VEC_POP(vectype)		\
	DEFINE_STACK_VEC_TOP(vectype)		\
	DEFINE_STACK_VEC_SET(vectype)		\
	DEFINE_STACK_VEC_FREE(vectype)

typedef void * ptr;

DECLARE_STACK(u64);
DECLARE_STACK(u32);
DECLARE_STACK(f32);
DECLARE_STACK(ptr);
DECLARE_STACK(intv);
DECLARE_STACK_VEC(vec4);

#endif
