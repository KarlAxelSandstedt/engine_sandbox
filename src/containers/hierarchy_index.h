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

#ifndef __HIERARCHY_INDEX_H__
#define __HIERARCHY_INDEX_H__

#include "kas_common.h"
#include "array_list.h"
#include "allocator.h"

/* root stub is an internal node of the hierarchy; using this we can simplify logic and have a nice "NULL" index to use */
#define HI_ROOT_STUB_INDEX	0
#define	HI_NULL_INDEX		0	
#define HI_STATIC		0
#define HI_ORPHAN_STUB_INDEX	1
#define HI_GROWABLE		1
/*
 * hierarchy_index_node: Intrusive node to be placed at the top of any data structure in the hierarchy.
 */
struct hierarchy_index_node
{
	u32 	parent;		/* Index of parent 		*/
	u32 	next;		/* Index of next sibling 	*/
	u32 	prev;		/* Index of previous sibling 	*/
	u32 	first;		/* Index of first child 	*/ 
	u32 	last;		/* Index of last child 		*/
	u32	child_count;	/* Number of direct children 	*/
};

struct hierarchy_index
{
	struct array_list *	list;
};

/* alloc array list (on arena if non-NULL and non-growable), returns NULL on failure */
struct hierarchy_index *hierarchy_index_alloc(struct arena *mem, const u32 length, const u64 data_size, const u32 growable);
/* free hierarchy allocated on the heap */
void			hierarchy_index_free(struct hierarchy_index *hi);
/* flush or reset hierarchy */
void			hierarchy_index_flush(struct hierarchy_index *hi);
/* Allocate a hierarchy node and return the allocation slot on success, RETURNS (0, NULL) on failure */
struct slot		hierarchy_index_add(struct hierarchy_index *hi, const u32 parent_index);
/* Deallocate a hierarchy node and its whole sub-hierarchy */
void 			hierarchy_index_remove(struct arena *tmp, struct hierarchy_index *hi, const u32 node_index);
/* node's children (and their subtrees) are adopted by node's parent, and node's new parent becomes parent_index */
void 			hierarchy_index_adopt_node_exclusive(struct hierarchy_index *hi, const u32 node_index, const u32 new_parent_index);
/* node's subtree is removed from current parent and added to new parent*/
void 			hierarchy_index_adopt_node(struct hierarchy_index *hi, const u32 node_index, const u32 new_parent_index);
/* apply a custom free to and deallocate a hierarchy node and its whole sub-hierarchy; the custom free takes in the index to remove */
void			hierarchy_index_apply_custom_free_and_remove(struct arena *tmp, struct hierarchy_index *hi, const u32 node_index, void (*custom_free)(const struct hierarchy_index *hi, const u32, void *data), void *data);
/* Return node address corresponding to index */
void *			hierarchy_index_address(const struct hierarchy_index *hi, const u32 node_index);

/*
 * hierarchy_index_iterator: iterator for traversing a supplied node and it's entire sub-hierarchy in the given
 * hierarchy. SHOULD always be supplied an arena large enough to store the sub-hierarchy. To check if memory
 * ran out, and forced heap allocations took place, simply run the following check after you are done iterating.
 *
 * if (iterator.forced_malloc)
 * {
 *	LOG_MESSAGE()
 * }
 */
struct hierarchy_index_iterator
{
	struct hierarchy_index *hi; 	/* hierarchy index */
	struct arena *mem;		/* iterator memory */
	u64 stack_len;  		/* max stack size  */
	u32 *stack;			/* index stack 	   */
	u64 count;			/* stack count 	   */
	//u32 next;			/* next index	   */
	u32 forced_malloc;		/* internal state to use heap allocation if arena runs out of memory */
};

/* Setup hierarchy iterator at the given node root */
struct hierarchy_index_iterator	hierarchy_index_iterator_init(struct arena *ar_alias, struct hierarchy_index *hi, const u32 root);
/* release / pop any used memory by iterator */
void 				hierarchy_index_iterator_release(struct hierarchy_index_iterator *it);
/* Given it->count > 0, return the next index in the iterator */
u32 				hierarchy_index_iterator_peek(struct hierarchy_index_iterator *it);
/* Given it->count > 0, return the next index in the iterator, and push any new links (depth-first) related to the index */
u32 				hierarchy_index_iterator_next_df(struct hierarchy_index_iterator *it);
/* Given it->count > 0, skip the whole subtree corresponding to the next index in the iterator, and push the subtree's next sibling, if it exists. */
void				hierarchy_index_iterator_skip(struct hierarchy_index_iterator *it);

#endif
