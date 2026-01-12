/*
==========================================================================
    Copyright (C) 2025,2026 Axel Sandstedt 

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "collision.h"

//TODO can play around with these
#define COST_TRAVERSAL  1.0f	/* Overhead of internal node traversal (AABB testing of children) */
#define COST_INTERNAL 	1.5f	/* Overhead of triangle intersection tests */


struct bvh dbvh_alloc(struct arena *mem, const u32 initial_length, const u32 growable)
{
	kas_assert(!mem || !growable);
	struct bvh bvh =
	{
		.tree = bt_alloc(mem, initial_length, struct bvh_node, growable),
		.cost_queue = min_queue_new(NULL, COST_QUEUE_INITIAL_COUNT, growable),
		.tri = NULL,
		.tri_count = 0,
		.heap_allocated = !mem,	
	};

	return bvh;
}

void bvh_free(struct bvh *bvh)
{
	if (bvh->heap_allocated)
	{
		bt_dealloc(&bvh->tree);
		min_queue_free(&bvh->cost_queue);
		if (bvh->tri)
		{
			free(bvh->tri);
		}
	}
}

static f32 bbox_sah(const struct AABB *box)
{
	return box->hw[0]*(box->hw[1] + box->hw[2]) + box->hw[1]*box->hw[2];
}

static f32 bvh_cost_recursive(const struct bvh *bvh, const u32 index)
{
	f32 cost;
	
	const struct bvh_node *node = (struct bvh_node *) bvh->tree.pool.buf;
	if (BT_IS_LEAF(node + index))
	{
		cost = node[index].bt_right * COST_INTERNAL;
	}
	else
	{
		const f32 cost_left = bvh_cost_recursive(bvh, node[index].bt_left);
		const f32 cost_right = bvh_cost_recursive(bvh, node[index].bt_right);

		const f32 probability_left = bbox_sah(&node[node[index].bt_left].bbox) /  bbox_sah(&node[index].bbox);
		const f32 probability_right = bbox_sah(&node[node[index].bt_right].bbox) / bbox_sah(&node[index].bbox);

		cost = COST_TRAVERSAL + probability_left*cost_left + probability_right*cost_right; 
	}

	return cost;
}

f32 bvh_cost(const struct bvh *bvh)
{
	return bvh_cost_recursive(bvh, bvh->tree.root);
}

void dbvh_flush(struct bvh *bvh)
{
	bt_flush(&bvh->tree);
	min_queue_flush(&bvh->cost_queue);
}

static void dbvh_internal_balance_node(struct bvh *bvh, const u32 node)
{
	struct bvh_node *nodes = (struct bvh_node *) bvh->tree.pool.buf;
	/* (1) find best rotation */
	u32 left = nodes[node].bt_left;
	u32 right = nodes[node].bt_right;
	struct AABB box_union;
	f32 cost_rotation, cost_original, cost_best = F32_INFINITY;
			
	u32 upper_rotation; /* child to rotate */
	u32 best_rotation = POOL_NULL; /* best grandchild to rotate */
	if (!BT_IS_LEAF(nodes + left))
	{
		box_union = bbox_union(nodes[nodes[left].bt_left].bbox, nodes[right].bbox);
		cost_original = bbox_sah(&nodes[left].bbox);	
		cost_rotation = bbox_sah(&box_union);
		if (cost_rotation < cost_original)
		{
			upper_rotation = right;
			best_rotation = nodes[left].bt_right;
			cost_best = cost_rotation;
		}

		box_union = bbox_union(nodes[nodes[left].bt_right].bbox, nodes[right].bbox);
		cost_rotation = bbox_sah(&box_union);
		if (cost_rotation < cost_original && cost_rotation < cost_best)
		{
			upper_rotation = right;
			best_rotation = nodes[left].bt_left;
			cost_best = cost_rotation;
		}
	}

	if (!BT_IS_LEAF(nodes + right))
	{
		box_union = bbox_union(nodes[nodes[right].bt_left].bbox, nodes[left].bbox);
		cost_original = bbox_sah(&nodes[right].bbox);
		cost_rotation = bbox_sah(&box_union);
		if (cost_rotation < cost_best && cost_rotation < cost_original)
		{
			upper_rotation = left;
			best_rotation = nodes[right].bt_right;
			cost_best = cost_rotation;
		}

		box_union = bbox_union(nodes[nodes[right].bt_right].bbox, nodes[left].bbox);
		cost_rotation = bbox_sah(&box_union);
		if (cost_rotation < cost_best && cost_rotation < cost_original)
		{
			upper_rotation = left;
			best_rotation = nodes[right].bt_left;
			cost_best = cost_rotation;
		}
	}

	/* (2) apply rotation */
	if (best_rotation != POOL_NULL)
	{
		nodes[best_rotation].bt_parent = (nodes[best_rotation].bt_parent & BT_PARENT_LEAF_MASK) | node;
		if (upper_rotation == left)
		{
			nodes[upper_rotation].bt_parent = (nodes[upper_rotation].bt_parent & BT_PARENT_LEAF_MASK) | right;
			nodes[node].bt_left = best_rotation;
			if (best_rotation == nodes[right].bt_left)
			{
				nodes[right].bbox = bbox_union(nodes[nodes[right].bt_right].bbox, nodes[upper_rotation].bbox);
				nodes[right].bt_left = upper_rotation;
			}
			else
			{
				nodes[right].bbox = bbox_union(nodes[nodes[right].bt_left].bbox, nodes[upper_rotation].bbox);
				nodes[right].bt_right = upper_rotation;
			}
			left = best_rotation;
		}
		else
		{
			nodes[upper_rotation].bt_parent = (nodes[upper_rotation].bt_parent & BT_PARENT_LEAF_MASK) | left;
			nodes[node].bt_right = best_rotation;
			if (best_rotation == nodes[left].bt_left)
			{
				nodes[left].bbox = bbox_union(nodes[nodes[left].bt_right].bbox, nodes[upper_rotation].bbox);
				nodes[left].bt_left = upper_rotation;
			}
			else
			{
				nodes[left].bbox = bbox_union(nodes[nodes[left].bt_left].bbox, nodes[upper_rotation].bbox);
				nodes[left].bt_right = upper_rotation;
			}
			right = best_rotation;
		}
	}

	/* (3) refit node's box */
	nodes[node].bbox = bbox_union(nodes[left].bbox, nodes[right].bbox);
}

u32 dbvh_insert(struct bvh *bvh, const u32 id, const struct AABB *bbox)
{
	struct bvh_node *nodes = (struct bvh_node *) bvh->tree.pool.buf;
	struct slot leaf;
	if (bvh->tree.root == POOL_NULL)
	{
		leaf = bt_node_add_root(&bvh->tree);
		BT_SET_LEAF(nodes + leaf.index);
		/* Store external id's in bt_left of leaves */
		nodes[leaf.index].bt_left = id;
		nodes[leaf.index].bbox = *bbox;
	}
	else
	{
		struct slot internal = bt_node_add(&bvh->tree);
		leaf = bt_node_add(&bvh->tree);
		nodes[leaf.index].bbox = *bbox;
		nodes[leaf.index].bt_parent = BT_PARENT_LEAF_MASK | internal.index;
		nodes[leaf.index].bt_left = id;

		/**
		 * (1) Find best sibling using the minimum surface area hueristic + branch and bound algorithm.
		 * The idea is that every node in the hierarchy is a potential sibling to the new node, and we find
		 * the best suitable one by continuously delve deeper into the hierarchy as long as the some 
		 * new potential node gives a better cost than previous ones. We keep track of the best score and 
		 * the node achieving it. When no node achieves a better score, we are done and set the best scoring
		 * one as the sibling.
		 */
		u32 best_index = bvh->tree.root;
		f32 best_cost = F32_INFINITY;
		f32 node_cost = 0.0f; 
	
		min_queue_insert(&bvh->cost_queue, node_cost, bvh->tree.root);

		u32 node;
		f32 inherited_cost, cost;
	
		while (bvh->cost_queue.object_pool.count > 0)
		{
			/* (i) Get cost of node */
			inherited_cost = bvh->cost_queue.elements[0].priority; 
			node = min_queue_extract_min(&bvh->cost_queue);
			const struct AABB box_union = bbox_union(nodes[leaf.index].bbox, nodes[node].bbox);
			/* Inherited area cost + expanded node area cost */
			cost = inherited_cost + bbox_sah(&box_union);

			if (cost < best_cost)
			{
				best_cost = cost;
				best_index = node;
			}

			/**
			 * The current difference in area produced by the node's path + the new box's area
			 * is a lower bound on the node's descendants' cost. If the lower bound is not less
			 * than the best cost, we can prune the children's trees. Otherwise, we must still
			 * consider them as viable siblings. Their priorities become the increase in cost 
			 * to node's path when adding the new box (the inherited cost).
			 */
			cost -= bbox_sah(&nodes[node].bbox);

			if (!BT_IS_LEAF(nodes + node) && cost + bbox_sah(&nodes[leaf.index].bbox) < best_cost)
			{
				min_queue_insert(&bvh->cost_queue, cost, nodes[node].bt_left);
				min_queue_insert(&bvh->cost_queue, cost, nodes[node].bt_right);
			}
		}

		/* (2) Setup a new parent node for the new node and its sibling */
		const u32 best_parent = nodes[best_index].bt_parent & BT_PARENT_INDEX_MASK;
		if (BT_IS_ROOT(nodes + best_index))
		{
			bvh->tree.root = internal.index;
		}
		else
		{
			if (nodes[best_parent].bt_left == best_index)
			{
				nodes[best_parent].bt_left = internal.index;
			}
			else
			{
				nodes[best_parent].bt_right = internal.index;
			}
		}

		nodes[internal.index].bt_parent = best_parent;
		nodes[internal.index].bt_left = best_index;
		nodes[internal.index].bt_right = leaf.index;
		nodes[internal.index].bbox = bbox_union(nodes[leaf.index].bbox, nodes[best_index].bbox);
		nodes[best_index].bt_parent = (nodes[best_index].bt_parent & BT_PARENT_LEAF_MASK) | internal.index;

		node = nodes[internal.index].bt_parent;
		/* (3) Traverse from grandparent of leaf, refitting and rotating node up to the root */
		while (node != POOL_NULL)
		{
			dbvh_internal_balance_node(bvh, node);
			node = nodes[node].bt_parent;
		}
	}

	//struct arena tmp = arena_alloc_1MB();
	//bvh_validate(&tmp, bvh);
	//arena_free_1MB(&tmp);

	return leaf.index;
}

void dbvh_remove(struct bvh *bvh, const u32 index)
{
	struct bvh_node *nodes = (struct bvh_node *) bvh->tree.pool.buf;
	kas_assert(BT_IS_LEAF(nodes + index));

	u32 parent = nodes[index].bt_parent & BT_PARENT_INDEX_MASK;
	if (parent == POOL_NULL)
	{
		bvh->tree.root = POOL_NULL;
		bt_node_remove(&bvh->tree, parent);
	}
	else
	{
		const u32 sibling = (nodes[parent].bt_left == index)
			? nodes[parent].bt_right
			: nodes[parent].bt_left;

		const u32 grand_parent = nodes[parent].bt_parent;
		nodes[sibling].bt_parent = (nodes[sibling].bt_parent & BT_PARENT_LEAF_MASK) | grand_parent;

		bt_node_remove(&bvh->tree, parent);
		bt_node_remove(&bvh->tree, index);

		/* set new root */
		if (grand_parent == POOL_NULL)
		{
			bvh->tree.root = sibling;
		}
		else
		{
			if (nodes[grand_parent].bt_left == parent)
			{
				nodes[grand_parent].bt_left = sibling;
			}
			else
			{
				nodes[grand_parent].bt_right = sibling;
			}

			nodes[grand_parent].bbox = bbox_union(nodes[nodes[grand_parent].bt_left].bbox, nodes[nodes[grand_parent].bt_right].bbox);
			parent = nodes[grand_parent].bt_parent;
			while (parent != POOL_NULL)
			{
				dbvh_internal_balance_node(bvh, parent);
				parent = nodes[parent].bt_parent;
			}
		}
	}

	//struct arena tmp = arena_alloc_1MB();
	//bvh_validate(&tmp, bvh);
	//arena_free_1MB(&tmp);
}

u32 dbvh_internal_push_subtree_overlap_pairs(struct arena *mem, struct dbvh_overlap *stack, const u64 stack_len, const struct bvh *bvh, u32 subA, u32 subB)
{
	struct bvh_node *nodes = (struct bvh_node *) bvh->tree.pool.buf;
	u32 overlap_count = 0;
	struct dbvh_overlap overlap;
	u32 q = U32_MAX;

	while (1)
	{
		if (AABB_test(&nodes[subA].bbox, &nodes[subB].bbox))
		{
			if (BT_IS_LEAF(nodes + subA) && BT_IS_LEAF(nodes + subB))
			{
				overlap_count += 1;
				/* id's */
				if (nodes[subA].bt_left < nodes[subB].bt_left)
				{
					overlap.id1 = nodes[subA].bt_left;	
					overlap.id2 = nodes[subB].bt_left;	
				}
				else
				{
					overlap.id1 = nodes[subB].bt_left;	
					overlap.id2 = nodes[subA].bt_left;	
				}
				arena_push_packed_memcpy(mem, &overlap, sizeof(overlap));
			}
			else
			{
				/* if a is larger than b, descend into a first  */
				if (BT_IS_LEAF(nodes + subB) || (!BT_IS_LEAF(nodes + subA) && bbox_sah(&nodes[subB].bbox) < bbox_sah(&nodes[subA].bbox)))
				{
					stack[++q].id1 = nodes[subA].bt_left;
					stack[q].id2 = subB;
					subA = nodes[subA].bt_right;
				}
				else
				{
					stack[++q].id1 = nodes[subB].bt_left;
					stack[q].id2 = subA;
					subB = nodes[subB].bt_right;
				}

				if (q+1 >= stack_len)
				{
					log_string(T_PHYSICS, S_FATAL, "out-of-memory in arena based stack, increase arena size!");		
					fatal_cleanup_and_exit(kas_thread_self_tid());
				}
				continue;
			}
		}

		if (q != U32_MAX)
		{
			subA = stack[q].id1;
			subB = stack[q--].id2;
		}
		else
		{
			break;
		}
	}

	return overlap_count;
}

struct dbvh_overlap *dbvh_push_overlap_pairs(struct arena *mem, u32 *count, const struct bvh *bvh)
{
	if (bt_leaf_count(&bvh->tree) < 2) { return 0; }
	const struct bvh_node *nodes = (struct bvh_node *) bvh->tree.pool.buf;

	*count = 0;
	u32 a = nodes[bvh->tree.root].bt_left;
	u32 b = nodes[bvh->tree.root].bt_right;
	u32 q = U32_MAX;

	struct arena tmp1 = arena_alloc_1MB();
	struct arena tmp2 = arena_alloc_1MB();

	struct allocation_array arr1 = arena_push_aligned_all(&tmp1, sizeof(struct dbvh_overlap), 4); 
	struct allocation_array arr2 = arena_push_aligned_all(&tmp2, sizeof(struct dbvh_overlap), 4); 

	struct dbvh_overlap *stack1 = arr1.addr;
	struct dbvh_overlap *stack2 = arr2.addr;
	struct dbvh_overlap *overlaps = (struct dbvh_overlap *) mem->stack_ptr; 

	while (1)
	{
		*count += dbvh_internal_push_subtree_overlap_pairs(mem, stack2, arr2.len, bvh, a, b);

		if (!BT_IS_LEAF(nodes + a))
		{
			stack1[++q].id1 = nodes[a].bt_left;
			stack1[q].id2 = nodes[a].bt_right;	
			if (q >= arr1.len)
			{
				log_string(T_PHYSICS, S_FATAL, "out-of-memory in arena based stack, increase arena size!");		
				fatal_cleanup_and_exit(kas_thread_self_tid());
			}
		}

		if (!BT_IS_LEAF(nodes + b))
		{
			 a = nodes[b].bt_left;	
			 b = nodes[b].bt_right;	
			 continue;
		}

		if (q != U32_MAX)
		{
			a = stack1[q].id1;
			b = stack1[q--].id2;
		}
		else
		{
			break;
		}
	}

	arena_free_1MB(&tmp1);
	arena_free_1MB(&tmp2);

	return (*count) ? overlaps : NULL;
}

void bvh_validate(struct arena *tmp, const struct bvh *bvh)
{
	arena_push_record(tmp);
	bt_validate(tmp, &bvh->tree);
	if (bvh->tree.root == POOL_NULL) { return; }

	const struct bvh_node *node = (struct bvh_node *) bvh->tree.pool.buf;
	struct allocation_array arr = arena_push_aligned_all(tmp, sizeof(u32), 4);
	u32 *stack = arr.addr;
	stack[0] = bvh->tree.root;
	u32 sc = 1;
	while (sc--)
	{
		const u32 i = stack[sc];
		if (!BT_IS_ROOT(node + i))
		{
			const u32 parent = node[i].bt_parent & BT_PARENT_INDEX_MASK;
			kas_assert(AABB_contains_margin(&node[parent].bbox, &node[i].bbox, 0.001f));
		}

		if (!BT_IS_LEAF(node + stack[sc]))
		{
			stack[sc + 0] = node[i].bt_left;
			stack[sc + 1] = node[i].bt_right;
			sc += 2;
		}
	}
	arena_pop_record(tmp);
}

struct bvh sbvh_from_tri_mesh(struct arena *mem, const struct tri_mesh *mesh, const u32 bin_count)
{
	kas_assert(bin_count);
	if (!mesh->tri_count)
	{
		return (struct bvh) { 0 };
	}

	PROF_ZONE;

	arena_push_record(mem);
	const u32 max_node_count_required = 2*mesh->tri_count - 1;
	struct bvh sbvh =
	{
		.tree = bt_alloc(mem, max_node_count_required, struct bvh_node, NOT_GROWABLE),
		.mesh = mesh,
		.tri = arena_push(mem, mesh->tri_count*sizeof(u32)),
		.tri_count = mesh->tri_count,
		.heap_allocated = 0,
	};

	arena_push_record(mem);
	struct AABB *axis_bin_bbox[3];
	u32 *axis_bin_tri_count[3];
	u8 *centroid_bin_map[3];
	centroid_bin_map[0] = arena_push(mem, mesh->tri_count*sizeof(u8));
	centroid_bin_map[1] = arena_push(mem, mesh->tri_count*sizeof(u8));
	centroid_bin_map[2] = arena_push(mem, mesh->tri_count*sizeof(u8));
	axis_bin_bbox[0] = arena_push(mem, bin_count*sizeof(struct AABB));
	axis_bin_bbox[1] = arena_push(mem, bin_count*sizeof(struct AABB));
	axis_bin_bbox[2] = arena_push(mem, bin_count*sizeof(struct AABB));
	axis_bin_tri_count[0] = arena_push(mem, bin_count*sizeof(u32));
	axis_bin_tri_count[1] = arena_push(mem, bin_count*sizeof(u32));
	axis_bin_tri_count[2] = arena_push(mem, bin_count*sizeof(u32));
	struct AABB *bbox_tri = arena_push(mem, mesh->tri_count*sizeof(struct AABB));
	struct allocation_array arr = arena_push_aligned_all(mem, sizeof(u32), 4);

	u32 success = 1;
	if (!sbvh.tree.pool.length 
			|| !sbvh.tri 
			|| !centroid_bin_map[2] 
			|| !axis_bin_tri_count[2] 
			|| !axis_bin_bbox[2] 
			|| !arr.len 
			|| !bbox_tri)
	{
		success = 0;
		goto end;
	}

	u32 *node_stack = arr.addr;	
	u32 node_stack_size = arr.len;
	u32 sc = 1;
	struct slot root = bt_node_add_root(&sbvh.tree);
	struct bvh_node *node = root.address;
	/* bt_left = tri_first,
	 * bt_right = tri_count */
	node->bt_left = 0;
	node->bt_right = mesh->tri_count;
	node->bbox = bbox_triangle(
				mesh->v[mesh->tri[0][0]],
				mesh->v[mesh->tri[0][1]],
				mesh->v[mesh->tri[0][2]]);
	node_stack[0] = root.index;

	for (u32 i = 0; i < mesh->tri_count; ++i)
	{
		sbvh.tri[i] = i;
		bbox_tri[i] = bbox_triangle(
				mesh->v[mesh->tri[i][0]],
				mesh->v[mesh->tri[i][1]],
				mesh->v[mesh->tri[i][2]]);
		node->bbox = bbox_union(node->bbox, bbox_tri[i]);
	}
	
	/* Process triangles from left to right, depth-first. */
	while (sc--)
	{
		node = pool_address(&sbvh.tree.pool, node_stack[sc]);
		const u32 tri_first = node->bt_left;
		const u32 tri_count = node->bt_right;
		//fprintf(stderr, "(I,T) = (%u,%u)\n", tri_first, tri_first + tri_count - 1);
		if (tri_count == 1)
		{
			continue;
		}

		PROF_ZONE_NAMED("sbvh construction iteration");
		vec3 bbox_min, bbox_max;
		vec3_add(bbox_max, node->bbox.center, node->bbox.hw);
		vec3_sub(bbox_min, node->bbox.center, node->bbox.hw);

		u32 best_axis = U32_MAX;
		u32 best_split = U32_MAX;
		u32 best_left_count = 0;
		u32 best_right_count = 0;
		struct AABB best_bbox_left = { 0 };
		struct AABB best_bbox_right = { 0 };
		const f32 parent_sah = bbox_sah(&node->bbox);
		f32 best_score = F32_INFINITY;
		for (u32 axis = 0; axis < 3; axis++)
		{
			for (u32 bi = 0; bi < bin_count; ++bi)
			{
				axis_bin_tri_count[axis][bi] = 0;
			}

			for (u32 i = tri_first; i < tri_first + tri_count; ++i)
			{
				const u32 tri = sbvh.tri[i];
				const f32 val = bin_count * (bbox_tri[tri].center[axis] - bbox_min[axis]) / (bbox_max[axis] - bbox_min[axis]);
				const u8 bi = (u8) f32_clamp(val, 0.0f, bin_count - 0.01f);
				centroid_bin_map[axis][tri] = bi;
				axis_bin_bbox[axis][bi] = (axis_bin_tri_count[axis][bi] > 0)
					? bbox_union(axis_bin_bbox[axis][bi], bbox_tri[tri])
					: bbox_tri[tri];
				axis_bin_tri_count[axis][bi] += 1;
			}

			//TODO simplify bbox constructing by creating bbox array before loop so we can easily just bbox_left = [], bbox_right = [] 
			struct AABB bbox_left;
			u32 left_count = 0;
			for (u32 split = 0; split < bin_count-1; ++split)
			{
				if (axis_bin_tri_count[axis][split] == 0)
				{
					continue;
				}

				bbox_left = (left_count == 0)
					? axis_bin_bbox[axis][split]
					: bbox_union(bbox_left, axis_bin_bbox[axis][split]);
				left_count += axis_bin_tri_count[axis][split];

				const u32 right_count = node->bt_right - left_count;
				if (right_count == 0)
				{
					break;
				}

				u32 bi = split + 1;
				for (; axis_bin_tri_count[axis][bi] == 0; ++bi);

				struct AABB bbox_right = axis_bin_bbox[axis][bi++];
				for (; bi < bin_count; bi++)
				{
					if (axis_bin_tri_count[axis][bi])
					{
						bbox_right = bbox_union(bbox_right, axis_bin_bbox[axis][bi]);
					}
				}

				const f32 cost_traversal = COST_TRAVERSAL;
				const f32 cost_internal = COST_INTERNAL;
				const f32 left_cost = left_count*bbox_sah(&bbox_left)/parent_sah;
				const f32 right_cost = right_count*bbox_sah(&bbox_right)/parent_sah;
				const f32 score = cost_traversal + cost_internal*(left_cost + right_cost);
				if (score < best_score)
				{
					best_score = score;
					best_axis = axis;
					best_split = split;
					best_bbox_left = bbox_left;
					best_bbox_right = bbox_right;
					best_left_count = left_count;
					best_right_count = right_count;
				}
			}
		}

		if (best_left_count && best_right_count)
		{
			if (sc + 2 <= node_stack_size)
			{
				u32 left = tri_first;
				u32 right = tri_first + tri_count - 1;
				while (left < right)
				{
					const u32 tri = sbvh.tri[left];
					if (centroid_bin_map[best_axis][tri] <= best_split)
					{
						left += 1;
					}
					else
					{
						sbvh.tri[left] = sbvh.tri[right];
						sbvh.tri[right] = tri;
						right -= 1;
					}
				}


				struct slot slot_left, slot_right;
				bt_node_add_children(&sbvh.tree, &slot_left, &slot_right, node_stack[sc]);
				kas_assert(slot_left.address && slot_right.address);

				struct bvh_node *child_left = slot_left.address;
				struct bvh_node *child_right = slot_right.address;

				child_left->bbox = best_bbox_left;
				child_left->bt_left = tri_first;
				child_left->bt_right = best_left_count;

				child_right->bbox = best_bbox_right;
				child_right->bt_left = tri_first + best_left_count;
				child_right->bt_right = best_right_count;

				node_stack[sc] = slot_right.index;
				node_stack[sc+1] = slot_left.index;
				sc += 2;
			}
			else
			{
				sc = 0;
				success = 0;	
			}
		}
		
		PROF_ZONE_END;
	}
	
end:
	arena_pop_record(mem);
	if (success)
	{
		arena_remove_record(mem);
	}
	else
	{
		arena_pop_record(mem);
		const u64 size_required = max_node_count_required*sizeof(struct bvh_node) 
			+ mesh->tri_count*sizeof(u32) 
			+ mesh->tri_count*sizeof(struct AABB)
			+ 3*mesh->tri_count*sizeof(u8) 
			+ 3*bin_count*(sizeof(struct AABB) + sizeof(u32));
		log(T_SYSTEM, S_ERROR, "Failed to allocate bvh from triangle mesh, minimum size required: %lu\n", size_required);
		sbvh = (struct bvh) { 0 };
	}

	bvh_validate(mem, &sbvh);

	PROF_ZONE_END;
	return sbvh;
}

struct bvh_raycast_info bvh_raycast_init(struct arena *mem, const struct bvh *bvh, const struct ray *ray)
{
	struct bvh_raycast_info info =
	{
		.hit = u32f32_inline(U32_MAX, F32_INFINITY),
		.node = (struct bvh_node *) bvh->tree.pool.buf,
		.ray = ray,
		.bvh = bvh,
	};

	if (bt_node_count(&bvh->tree)) 
	{
		AABB_raycast_parameter_ex_setup(info.multiplier, info.dir_sign_bit, info.ray);
		const f32 root_hit_param = AABB_raycast_parameter_ex(&info.node[info.bvh->tree.root].bbox, info.ray, info.multiplier, info.dir_sign_bit);
		if (root_hit_param < F32_INFINITY) 
		{
			info.hit_queue = min_queue_fixed_alloc_all(mem);
			min_queue_fixed_push(&info.hit_queue, bvh->tree.root, root_hit_param);
		}
	}

	return info;
}

void bvh_raycast_test_and_push_children(struct bvh_raycast_info *info, const u32f32 popped_tuple)
{
	const struct bvh_node *node = info->node;
	const f32 distance_left = AABB_raycast_parameter_ex(&node[node[popped_tuple.u].bt_left].bbox, info->ray, info->multiplier, info->dir_sign_bit);
	const f32 distance_right = AABB_raycast_parameter_ex(&node[node[popped_tuple.u].bt_right].bbox, info->ray, info->multiplier, info->dir_sign_bit);

	if (distance_left < F32_INFINITY)
	{
		min_queue_fixed_push(&info->hit_queue, info->node[popped_tuple.u].bt_left, distance_left);
	}

	if (distance_right < F32_INFINITY)
	{
		if (info->hit_queue.count == info->hit_queue.length)
		{
			log_string(T_SYSTEM, S_FATAL, "distance queue in bvh_raycast OOM, aborting");
			fatal_cleanup_and_exit(kas_thread_self_tid());
		}
		min_queue_fixed_push(&info->hit_queue, info->node[popped_tuple.u].bt_right, distance_right);
	}
}

u32f32 sbvh_raycast(struct arena *tmp, const struct bvh *bvh, const struct ray *ray)
{
	PROF_ZONE;
	arena_push_record(tmp);

	struct bvh_raycast_info info = bvh_raycast_init(tmp, bvh, ray);
	while (info.hit_queue.count)
	{
		const u32f32 tuple = min_queue_fixed_pop(&info.hit_queue);
		if (info.hit.f < tuple.f)
		{
			break;	
		}

		if (BT_IS_LEAF(info.node + tuple.u))
		{
			const u32 tri_first = info.node[tuple.u].bt_left;
			const u32 tri_last = tri_first + info.node[tuple.u].bt_right - 1;
			for (u32 i = tri_first; i <= tri_last; ++i)
			{
				const f32 distance = triangle_raycast_parameter(bvh->mesh, i, ray);
				if (distance < info.hit.f)
				{
					info.hit = u32f32_inline(i, distance);
				}
			}
		}
		else
		{
			bvh_raycast_test_and_push_children(&info, tuple);
		}
	}

	arena_pop_record(tmp);

	PROF_ZONE_END;
	return info.hit;
}
