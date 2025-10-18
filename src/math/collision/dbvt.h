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

#ifndef __DYNAMIC_BOUNDING_VOLUME_TREE_H__
#define __DYNAMIC_BOUNDING_VOLUME_TREE_H__

#include "sys_common.h"
#include "float32.h"
#include "geometry.h"
#include "queue.h"

/**
 * Basic steps for a simple dynamic bounding volume hierarchy:
 *
 * INCREMENTAL ADD
 * 	(1) Alloc leaf node
 * 	(2) Find the best sibling for the new volume
 * 	(3) Add parent to sibling and new node
 * 	(4) Rebase the tree, in order to balance it at keep the performance good
 *
 * INCREMENTAL CHECK OVERLAP
 * 	- several possibilities, we should as a first step try descendLargestVolume.
 * 	  If one goes through a whole tree at a time (depth first) one may get into really bad
 * 	  situations as comparing super small object vs relatively large ones, and check overlaps
 * 	  all the time.
 *
 * INCREMENTAL REMOVE
 * 	(1) Remove leaf
 * 	(2) Set sibling as parent leaf
 *
 * Some useful stuff for debugging purposes and performance monitoring would be:
 * 	- draw line box around AABB_2d volumes
 * 	- average number of overlaps every frame
 * 	- number of nodes
 * 	- deepest leaves
 *
 * Some general optimisations
 * 	- enlarged AABB_2ds somehow (less reinserts)
 * 	- do not recompute cost of child in balance if...
 * 	- clever strategy for how to place parant vs child nodes (left always comes after parent...)
 * 	  We want to make the cache more coherent in traversing the tree
 * 	- remove min_queue queue_indices, aren't they unnecessary?
 * 	- Another strategy for cache coherency while traversing would be double layer nodes (parent, child, child) 
 * 	- Global caching of collisions (Hash table of previous primitive collisions)
 * 	- Local caching (Every single object has a cache of collisions)
 * 	- SIMD AABB_2d operations
 */

#define DBVT_NO_NODE -1
#define COST_QUEUE_MAX 124

struct dbvt_node {
	struct AABB box;
	i32 id;		/* id == -1 <=> end of free chain */
	i32 parent;
	i32 left;
	i32 right;
};

struct dbvt
{
	struct min_queue *cost_queue;
	struct dbvt_node *nodes;	
	i32 cost_index[COST_QUEUE_MAX];
	i32 node_count;		/* not used in operations, only for debug purposes */
	i32 proxy_count; 
	i32 root;
	i32 next;
	i32 len;
};

struct dbvt_overlap
{
	i32 id1;
	i32 id2;	
};

/* If mem == NULL, standard malloc is used */
struct 	dbvt dbvt_alloc(struct arena *mem, const i32 len);
/* free allocated resources */
void 	dbvt_free(struct dbvt *tree);
/* flush / reset the hierarchy  */
void 	dbvt_flush(struct dbvt *tree);
/* id is an integer identifier from the outside, return index of added value */
i32 	dbvt_insert(struct dbvt *tree, const i32 id, const struct AABB *box);
/* remove leaf corresponding to index from tree */
void 	dbvt_remove(struct dbvt *tree, const i32 index);
/* Return overlapping ids ptr, set to NULL if no overlap. if overlap, count is set */
struct dbvt_overlap *dbvt_push_overlap_pairs(struct arena *mem, u32 *count, struct dbvt *tree);
/* push	id:s of leaves hit by raycast. returns number of hits. -1 == out of memory */
u32	dbvt_raycast(struct arena *mem, const struct dbvt *tree, const struct ray *ray);
/* validate tree construction */
void	dbvt_validate(struct dbvt *tree);
/* push heirarchy node box lines into draw buffer */
//void	dbvt_push_lines(struct drawbuffer *buf, struct dbvt *tree, const vec4 color);
/* TODO: Calculate tree cost */
f32	dbvt_cost(struct dbvt *tree);
/* TODO: Calculate tree maximal depth */
i32 	dbvt_depth(struct dbvt *tree);
/* TODO: Calculate memory size  */
u64 	dbvt_memory_usage(struct dbvt *tree);


#endif
