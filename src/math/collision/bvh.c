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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "collision.h"

static struct slot dbvh_internal_alloc_node(struct dbvh *tree, const u32 id, const struct AABB *box)
{
	struct slot slot = pool_add(&tree->node_pool);

	struct dbvh_node *node = slot.address;
	node->id = id;
	node->parent = BVH_NO_NODE;
	node->left = BVH_NO_NODE;
	node->right = BVH_NO_NODE;
	node->box = *box;

	return slot;	
}

static void dbvh_internal_free_node(struct dbvh *tree, const u32 index)
{
	pool_remove(&tree->node_pool, index);
}

struct dbvh dbvh_alloc(const u32 initial_length)
{
	struct dbvh tree =
	{
		.proxy_count = 0,
		.root = BVH_NO_NODE,
	};

	tree.node_pool = pool_alloc(NULL, initial_length, struct dbvh_node, GROWABLE);
	tree.cost_queue = min_queue_new(NULL, COST_QUEUE_INITIAL_COUNT, GROWABLE);

	return tree;
}

void dbvh_free(struct dbvh *tree)
{
	pool_dealloc(&tree->node_pool);
	min_queue_free(&tree->cost_queue);
}

void dbvh_flush(struct dbvh *tree)
{
	tree->proxy_count = 0;
	tree->root = BVH_NO_NODE;
	pool_flush(&tree->node_pool);
	min_queue_flush(&tree->cost_queue);
}

static f32 bbox_sah(const struct AABB *box)
{
	return box->hw[0]*(box->hw[1] + box->hw[2]) + box->hw[1]*box->hw[2];
}

static void dbvh_internal_balance_node(struct dbvh *tree, const u32 node)
{
	struct dbvh_node *nodes = (struct dbvh_node *) tree->node_pool.buf;
	/* (1) find best rotation */
	u32 left = nodes[node].left;
	u32 right = nodes[node].right;
	struct AABB box_union;
	f32 cost_rotation, cost_original, cost_best = F32_INFINITY;
			
	u32 upper_rotation; /* child to rotate */
	u32 best_rotation = BVH_NO_NODE; /* best grandchild to rotate */
	if (nodes[left].left != BVH_NO_NODE)
	{
		AABB_union(&box_union, &nodes[nodes[left].left].box, &nodes[right].box);
		cost_original = bbox_sah(&nodes[left].box);	
		cost_rotation = bbox_sah(&box_union);
		if (cost_rotation < cost_original)
		{
			upper_rotation = right;
			best_rotation = nodes[left].right;
			cost_best = cost_rotation;
		}

		AABB_union(&box_union, &nodes[nodes[left].right].box, &nodes[right].box);
		cost_rotation = bbox_sah(&box_union);
		if (cost_rotation < cost_best && cost_rotation < cost_original)
		{
			upper_rotation = right;
			best_rotation = nodes[left].left;
			cost_best = cost_rotation;
		}
	}

	if (nodes[right].left != BVH_NO_NODE)
	{
		AABB_union(&box_union, &nodes[nodes[right].left].box, &nodes[left].box);
		cost_original = bbox_sah(&nodes[right].box);	
		cost_rotation = bbox_sah(&box_union);
		if (cost_rotation < cost_best && cost_rotation < cost_original)
		{
			upper_rotation = left;
			best_rotation = nodes[right].right;
			cost_best = cost_rotation;
		}

		AABB_union(&box_union, &nodes[nodes[right].right].box, &nodes[left].box);
		cost_rotation = bbox_sah(&box_union);
		if (cost_rotation < cost_best && cost_rotation < cost_original)
		{
			upper_rotation = left;
			best_rotation = nodes[right].left;
			cost_best = cost_rotation;
		}
	}

	/* (2) apply rotation */
	if (best_rotation != BVH_NO_NODE)
	{
		nodes[best_rotation].parent = node;
		if (upper_rotation == left)
		{
			nodes[upper_rotation].parent = right;
			nodes[node].left = best_rotation;
			if (best_rotation == nodes[right].left)
			{
				AABB_union(&nodes[right].box, 
						&nodes[nodes[right].right].box,
					       	&nodes[upper_rotation].box);
				nodes[right].left = upper_rotation;
			}
			else
			{
				AABB_union(&nodes[right].box, 
						&nodes[nodes[right].left].box,
					       	&nodes[upper_rotation].box);
				nodes[right].right = upper_rotation;
			}
			left = best_rotation;
		}
		else
		{
			nodes[upper_rotation].parent = left;
			nodes[node].right = best_rotation;
			if (best_rotation == nodes[left].left)
			{
				AABB_union(&nodes[left].box, 
						&nodes[nodes[left].right].box,
					       	&nodes[upper_rotation].box);
				nodes[left].left = upper_rotation;
			}
			else
			{
				AABB_union(&nodes[left].box, 
						&nodes[nodes[left].left].box,
					       	&nodes[upper_rotation].box);
				nodes[left].right = upper_rotation;
			}
			right = best_rotation;
		}
	}

	/* (3) refit node's box */
	AABB_union(&nodes[node].box, &nodes[left].box, &nodes[right].box);
}

u32 dbvh_insert(struct dbvh *tree, const u32 id, const struct AABB *box)
{
	tree->proxy_count += 1;
	const u32 index = dbvh_internal_alloc_node(tree, id, box).index;
	if (tree->root == BVH_NO_NODE)
	{
		tree->root = index;
	}
	else
	{
		/**
		 * (1) Find best sibling using the minimum surface area hueristic + branch and bound algorithm.
		 * The idea is that every node in the hierarchy is a potential sibling to the new node, and we find
		 * the best suitable one by continuously delve deeper into the hierarchy as long as the some 
		 * new potential node gives a better cost than previous ones. We keep track of the best score and 
		 * the node achieving it. When no node achieves a better score, we are done and set the best scoring
		 * one as the sibling.
		 */
		u32 best_index = tree->root;
		f32 best_cost = F32_INFINITY;
		f32 node_cost = 0.0f; 
	
		min_queue_insert(&tree->cost_queue, node_cost, tree->root);
		kas_assert(tree->cost_queue.elements[0].priority == 0.0f);

		u32 node;
		f32 inherited_cost, cost;
		struct AABB box_union;
	
		while (tree->cost_queue.object_pool.count > 0)
		{
			struct dbvh_node *nodes = (struct dbvh_node *) tree->node_pool.buf;
			/* (i) Get cost of node */
			inherited_cost = tree->cost_queue.elements[0].priority; 
			node = min_queue_extract_min(&tree->cost_queue);
			AABB_union(&box_union, &nodes[index].box, &nodes[node].box);
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
			cost -= bbox_sah(&nodes[node].box);

			if (nodes[node].left != BVH_NO_NODE && cost + bbox_sah(&nodes[index].box) < best_cost)
			{
				min_queue_insert(&tree->cost_queue, cost, nodes[node].left);
				min_queue_insert(&tree->cost_queue, cost, nodes[node].right);
			}
		}

		/* (2) Setup a new parent node for the new node and its sibling */
		const u32 parent = dbvh_internal_alloc_node(tree, BVH_NO_NODE, box).index; 
		struct dbvh_node *nodes = (struct dbvh_node *) tree->node_pool.buf;
		if (nodes[best_index].parent != BVH_NO_NODE)
		{
			if (nodes[nodes[best_index].parent].left == best_index)
			{
				nodes[nodes[best_index].parent].left = parent;
			}
			else
			{
				nodes[nodes[best_index].parent].right = parent;
			}
		}

		nodes[parent].parent = nodes[best_index].parent;
		nodes[parent].left = best_index;
		nodes[parent].right = index;
		nodes[best_index].parent = parent;
		nodes[index].parent = parent;
		AABB_union(&nodes[parent].box, &nodes[index].box, &nodes[best_index].box);
		if (best_index == tree->root)
		{
			tree->root = parent;
		} 

		//printf("parent: %i,%i,%i,%i\n", tree->nodes[parent].parent, tree->nodes[parent].id, tree->nodes[parent].left, tree->nodes[parent].right);
		node = nodes[parent].parent;
		/* (3) Traverse from grandparent of leaf, refitting and rotating node up to the root */
		while (node != BVH_NO_NODE)
		{
			dbvh_internal_balance_node(tree, node);
			node = nodes[node].parent;
		}
	}

	//dbvh_validate(tree);

	return index;
}

void dbvh_remove(struct dbvh *tree, const u32 index)
{
	tree->proxy_count -= 1;
	struct dbvh_node *nodes = (struct dbvh_node *) tree->node_pool.buf;

	kas_assert(nodes[index].left  == BVH_NO_NODE);
	kas_assert(nodes[index].right == BVH_NO_NODE);

	u32 parent = nodes[index].parent;
	if (parent == BVH_NO_NODE)
	{
		tree->root = BVH_NO_NODE;
		dbvh_internal_free_node(tree, index);
	}
	else
	{
		u32 sibling = (nodes[parent].left == index)
			? nodes[parent].right
			: nodes[parent].left;

		const u32 grand_parent = nodes[parent].parent;
		nodes[sibling].parent = grand_parent;

		dbvh_internal_free_node(tree, parent);
		dbvh_internal_free_node(tree, index);

		/* set new root */
		if (grand_parent == BVH_NO_NODE)
		{
			tree->root = sibling;
		}
		else
		{
			if (nodes[grand_parent].left == parent)
			{
				nodes[grand_parent].left = sibling;
			}
			else
			{
				nodes[grand_parent].right = sibling;
			}

			AABB_union(&nodes[grand_parent].box, 
					&nodes[nodes[grand_parent].left].box,
					&nodes[nodes[grand_parent].right].box);
			parent = nodes[grand_parent].parent;
			while (parent != BVH_NO_NODE)
			{
				dbvh_internal_balance_node(tree, parent);
				parent = nodes[parent].parent;
			}
		}
	}

	//dbvh_validate(tree);
}

u32 dbvh_internal_descend_a(const struct dbvh_node *a, const struct dbvh_node *b)
{
	return (b->left == BVH_NO_NODE || (a->left != BVH_NO_NODE && bbox_sah(&b->box) < bbox_sah(&a->box))) ? 1 : 0;
}

u32 dbvh_internal_push_subtree_overlap_pairs(struct arena *mem, const struct dbvh *tree, u32 subA, u32 subB, struct dbvh_overlap *stack, const u64 stack_len)
{
	kas_assert(subA != BVH_NO_NODE && subB != BVH_NO_NODE);

	struct dbvh_node *nodes = (struct dbvh_node *) tree->node_pool.buf;
	u32 overlap_count = 0;
	struct dbvh_overlap overlap;
	u32 q = U32_MAX;

	while (1)
	{
		if (AABB_test(&nodes[subA].box, &nodes[subB].box))
		{
			if (nodes[subA].left == BVH_NO_NODE && nodes[subB].left == BVH_NO_NODE)
			{
				overlap_count += 1;
				if (nodes[subA].id < nodes[subB].id)
				{
					overlap.id1 = nodes[subA].id;	
					overlap.id2 = nodes[subB].id;	
				}
				else
				{
					overlap.id1 = nodes[subB].id;	
					overlap.id2 = nodes[subA].id;	
				}
				arena_push_packed_memcpy(mem, &overlap, sizeof(overlap));
			}
			else
			{
				/* if a is larger than b, descend into a first  */
				if (dbvh_internal_descend_a(nodes + subA, nodes + subB))
				{
					stack[++q].id1 = nodes[subA].left;
					stack[q].id2 = subB;
					subA = nodes[subA].right;
				}
				else
				{
					stack[++q].id1 = nodes[subB].left;
					stack[q].id2 = subA;
					subB = nodes[subB].right;
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

struct dbvh_overlap *dbvh_push_overlap_pairs(struct arena *mem, u32 *count, struct dbvh *tree)
{
	if (tree->proxy_count < 2) { return 0; }
	struct dbvh_node *nodes = (struct dbvh_node *) tree->node_pool.buf;

	*count = 0;
	u32 a = nodes[tree->root].left;
	u32 b = nodes[tree->root].right;
	u32 q = U32_MAX;

	struct arena tmp1 = arena_alloc_1MB();
	struct arena tmp2 = arena_alloc_1MB();

	struct allocation_array arr1 = arena_push_aligned_all(&tmp1, sizeof(struct dbvh_overlap), 8); 
	struct allocation_array arr2 = arena_push_aligned_all(&tmp2, sizeof(struct dbvh_overlap), 8); 

	struct dbvh_overlap *stack1 = arr1.addr;
	struct dbvh_overlap *stack2 = arr2.addr;
	struct dbvh_overlap *overlaps = (struct dbvh_overlap *) mem->stack_ptr; 

	while (1)
	{
		*count += dbvh_internal_push_subtree_overlap_pairs(mem, tree, a, b, stack2, arr2.len);

		if (nodes[a].left != BVH_NO_NODE)
		{
			stack1[++q].id1 = nodes[a].left;
			stack1[q].id2 = nodes[a].right;	
			if (q >= arr1.len)
			{
				log_string(T_PHYSICS, S_FATAL, "out-of-memory in arena based stack, increase arena size!");		
				fatal_cleanup_and_exit(kas_thread_self_tid());
			}
		}

		if (nodes[b].left != BVH_NO_NODE)
		{
			 a = nodes[b].left;	
			 b = nodes[b].right;	
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

u32 dbvh_raycast(struct arena *mem, const struct dbvh *tree, const struct ray *ray)
{
	if (tree->proxy_count == 0) { return 0; }

	u32 *hits = arena_push_packed(mem, tree->proxy_count * sizeof(u32));
	struct arena record = *mem;
	u32 *stack = arena_push_packed(mem, tree->proxy_count * sizeof(u32));

	vec3 tmp;
	u32 i = tree->root;
	u32 s_len = 0;
	u32 h_len = 0;
	const struct dbvh_node *node;

	while (1)
	{
		node = pool_address(&tree->node_pool, i);
		kas_assert (i < tree->node_pool.count);

		if (AABB_raycast(tmp, &node->box, ray))
		{
			if (node->left != BVH_NO_NODE)
			{
				i = node->left;
				stack[s_len++] = node->right;
				continue;
			}
			else
			{
				
				hits[h_len++] = node->id;
			}
		}


		if (s_len == 0)
		{
			break;
		}

		i = stack[--s_len];
	}

	*mem = record;
	return h_len;
}

void dbvh_validate(struct dbvh *tree)
{
	u32 i = tree->root;
	u32 q = BVH_NO_NODE;
	struct dbvh_node *nodes = (struct dbvh_node *) tree->node_pool.buf;
	
	u32 cost_index[COST_QUEUE_INITIAL_COUNT];

	u32 node_count = 0;
	while (i != BVH_NO_NODE)
	{
		node_count++;
		const u32 parent = nodes[i].parent;
		if (parent != BVH_NO_NODE)
		{
			const u32 parent_left = nodes[parent].left;
			const u32 parent_right = nodes[parent].right;
			kas_assert(parent_left != parent_right);
			kas_assert(parent_left == i || parent_right == i);
		}
	
		kas_assert((nodes[i].left == BVH_NO_NODE && nodes[i].right == BVH_NO_NODE) 
				|| (nodes[i].left != BVH_NO_NODE && nodes[i].right != BVH_NO_NODE));

		if (nodes[i].left != BVH_NO_NODE)
		{
			cost_index[++q] = nodes[i].right;
			i = nodes[i].left;
			kas_assert(q < COST_QUEUE_INITIAL_COUNT);
		}
		else if (q != BVH_NO_NODE)
		{
			i = cost_index[q--];
			kas_assert(i != BVH_NO_NODE);
		}
		else
		{
			i = BVH_NO_NODE;
		}
	}

	kas_assert(node_count == 2*tree->proxy_count - 1);
}

struct sbvh_iteration
{
	struct AABB 	bbox;
	u32		tri_count;
};

struct sbvh sbvh_from_tri_mesh(struct arena *mem, const struct tri_mesh *mesh, const u32 bin_count)
{
	kas_assert(bin_count);
	if (!mesh->tri_count)
	{
		return (struct sbvh) { 0 };
	}

	TracyCZone(ctx, 1);

	arena_push_record(mem);
	const u32 node_count_required = 2*mesh->tri_count - 1;
	struct sbvh sbvh = 
	{ 
		.tree = bt_alloc(mem, node_count_required, struct sbvh_node, 0),
	};

	u32 success = 1;
	if (!sbvh.tree.pool.length)
	{
		success = 0;
		goto end;
	}
	arena_remove_record(mem);

	arena_push_record(mem);
	struct AABB *axis_bin_bbox[3];
	u32 *axis_bin_tri_count[3];
	u8 *centroid_bin_map = arena_push(mem, mesh->tri_count*sizeof(u8));
	axis_bin_bbox[0] = arena_push(mem, bin_count*sizeof(struct AABB));
	axis_bin_bbox[1] = arena_push(mem, bin_count*sizeof(struct AABB));
	axis_bin_bbox[2] = arena_push(mem, bin_count*sizeof(struct AABB));
	axis_bin_tri_count[0] = arena_push(mem, bin_count*sizeof(u32));
	axis_bin_tri_count[1] = arena_push(mem, bin_count*sizeof(u32));
	axis_bin_tri_count[2] = arena_push(mem, bin_count*sizeof(u32));
	struct allocation_array arr = arena_push_aligned_all(mem, sizeof(struct sbvh_iteration), 8);

	if (!centroid_bin_map || !axis_bin_tri_count[2] || axis_bin_bbox[2] || !arr.len)
	{
		success = 0;
		goto end;
	}

	struct AABB bbox_root = tri_mesh_bbox(mesh);
	struct sbvh_node *leaf = (struct sbvh_node *) sbvh.tree.pool.buf;
	for (u32 i = 0; i < mesh->tri_count; ++i)
	{
		struct slot slot = bt_node_add(&sbvh.tree);
		struct sbvh_node *node = slot.address;
		node->id = slot.index;
		node->bbox = bbox_triangle(
				mesh->v[mesh->tri[i][0]],
				mesh->v[mesh->tri[i][1]],
				mesh->v[mesh->tri[i][2]]);
	}

	u32 sc = 1;
	const u32 stack_size = arr.len;
	struct sbvh_iteration *stack = arr.addr;
	stack[0].bbox = bbox_root;
	stack[0].tri_count = mesh->tri_count;

	/* Process triangles from left to right, depth-first. */
	u32 processed = 0;
	u32 remaining = mesh->tri_count;
	while (sc--)
	{
		if (stack[sc].tri_count == 1)
		{
			processed += 1;
			remaining -= 1;
			continue;
		}

		TracyCZoneN(ctx, "sbvh construction iteration", 1);
		struct AABB bbox = stack[sc].bbox;
		vec3 bbox_min, bbox_max;
		vec3_add(bbox_max, bbox.center, bbox.hw);
		vec3_sub(bbox_min, bbox.center, bbox.hw);

		u32 best_axis = U32_MAX;
		u32 best_split = U32_MAX;
		u32 best_left_count = 0;
		u32 best_right_count = 0;
		struct AABB best_bbox_left = { 0 };
		struct AABB best_bbox_right = { 0 };
		const f32 parent_sah = bbox_sah(&bbox);
		f32 best_score = F32_INFINITY;
		for (u32 axis = 0; axis < 3; axis++)
		{
			for (u32 bi = 0; bi < bin_count; ++bi)
			{
				axis_bin_bbox[axis][bi] = (struct AABB) { 0 };
				axis_bin_tri_count[axis][bi] = 0;
			}

			for (u32 i = processed; i < stack[sc].tri_count; ++i)
			{
				const f32 val = bbox_min[axis] - mesh->v[i][axis] / (bbox_max[axis] - bbox_min[axis]);
				const u8 bi = (u8) f32_clamp(val, 0.0f, bin_count - 0.01f);
				centroid_bin_map[i] = bi;
				axis_bin_tri_count[axis][bi] += 1;
				axis_bin_bbox[axis][bi] = bbox_union(axis_bin_bbox[axis][bi], leaf[i].bbox);
			}

			struct AABB bbox_left = { 0 };
			u32 left_count = 0;
			u32 right_count = stack[sc].tri_count;
			for (u32 split = 0; split < bin_count-1; ++split)
			{
				bbox_left = bbox_union(bbox_left, axis_bin_bbox[axis][split]);
				left_count += axis_bin_tri_count[axis][split];
				struct AABB bbox_right = axis_bin_bbox[axis][split+1];
				for (u32 bi = split + 2; bi < bin_count; bi++)
				{
					bbox_right = bbox_union(bbox_right, axis_bin_bbox[axis][bi]);
					right_count -= axis_bin_tri_count[axis][bi];
				}

				const f32 c_trav = 1.0f;
				const f32 c_int = 1.5f;
				const f32 left_cost = left_count*bbox_sah(&bbox_left)/parent_sah;
				const f32 right_cost = right_count*bbox_sah(&bbox_right)/parent_sah;
				const f32 score = c_trav + c_int*(left_cost + right_cost);
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

		if (sc + 2 <= stack_size)
		{
			//TODO sort left-right ordering
			u32 left = 0;
			u32 right = stack[sc].tri_count-1;
			while (left < right)
			{

			}

			stack[sc].bbox = best_bbox_right;
			stack[sc].tri_count = best_right_count;
			stack[sc + 1].bbox = best_bbox_left;
			stack[sc + 1].tri_count = best_left_count;
			sc += 2;
		
			/* (8) TODO: Create parent and leaf and update tree. */
		}
		else
		{
			success = 0;	
			break;
		}

		TracyCZoneEnd(ctx);
	}
	
end:
	if (!success)
	{
		const u64 size_required = node_count_required*sizeof(struct sbvh_node) + mesh->tri_count*sizeof(u8) + 3*bin_count*(sizeof(struct AABB) + sizeof(u32));
		log(T_SYSTEM, S_ERROR, "Failed to allocate bvh from triangle mesh, minimum size required: %lu\n", size_required);
		sbvh = (struct sbvh) { 0 };
	}
	arena_pop_record(mem);
	TracyCZoneEnd(ctx);
	return sbvh;
}
