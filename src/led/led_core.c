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

#include "led_local.h"

void cmd_led_node_add(void);
void cmd_led_node_remove(void);

u32 cmd_led_node_add_id;
u32 cmd_led_node_remove_id;

void led_core_init_commands(void)
{
	cmd_led_node_add_id = cmd_function_register(utf8_inline("led_node_add"), 1, &cmd_led_node_add).index;
	cmd_led_node_remove_id = cmd_function_register(utf8_inline("led_node_remove"), 1, &cmd_led_node_remove).index;
}

struct slot led_node_add(struct led *led, const utf8 id)
{
	struct slot slot = empty_slot;
	if (!id.len)
	{
		log_string(T_LED, S_WARNING, "Failed to allocate led_node: id must not be empty");
	} 
	else if (id.size > 256)
	{
		log_string(T_LED, S_WARNING, "Failed to allocate led_node: id size must be <= 256B");
	} 
	else if (led_node_lookup(led, id).address == NULL) 
	{ 
		void *buf = thread_alloc_256B();
		const utf8 copy = utf8_copy_buffered(buf, 256, id);	
		const u32 key = utf8_hash(id);

		slot = gpool_add(&led->node_pool);
		hash_map_add(led->node_map, key, slot.index);
		dll_append(&led->node_non_marked_list, led->node_pool.buf, slot.index);

		struct led_node *node = slot.address;
		node->flags = LED_FLAG_NONE;
		node->id = copy;
		node->key = key;
		node->cache = ui_node_cache_null();

		const vec3 axis = { 0.0f, 1.0f, 0.0f };
		vec3_set(node->position, 0.0f, 0.0f, 0.0f);
		axis_angle_to_quaternion(node->rotation, axis, 0.0f);

		node->physics_handle = POOL_NULL;
		node->render_handle = POOL_NULL;
		node->csg_handle = POOL_NULL;
	}

	return slot;
}

struct slot led_node_lookup(struct led *led, const utf8 id)
{
	const u32 key = utf8_hash(id);
	struct slot slot = empty_slot;
	for (u32 i = hash_map_first(led->node_map, key); i != HASH_NULL; i = hash_map_next(led->node_map, i))
	{
		struct led_node *node = gpool_address(&led->node_pool, i);
		if (utf8_equivalence(id, node->id))
		{
			slot.index = i;
			slot.address = node;
			break;
		}
	}

	return slot;
}
static void led_remove_marked_structs(struct led *led)
{
	struct led_node *node = NULL;
	for (u32 i = led->node_marked_list.first; i != DLL_NULL; i = DLL_NEXT(node))
	{
		node = gpool_address(&led->node_pool, i);
		if (node->flags & LED_CONSTANT)
		{
			node->flags &= ~LED_MARKED_FOR_REMOVAL;
			dll_remove(&led->node_marked_list, led->node_pool.buf, i);
			dll_append(&led->node_non_marked_list, led->node_pool.buf, i);
			continue;
		}

		const u32 key = utf8_hash(node->id);
		hash_map_remove(led->node_map, key, i);
		thread_free_256B(node->id.buf);
		gpool_remove(&led->node_pool, i);
	}

	dll_flush(&led->node_marked_list);
}

void led_node_remove(struct led *led, const utf8 id)
{
	struct slot slot = led_node_lookup(led, id);
	struct led_node *node = slot.address;
	if ((node) && (POOL_SLOT_ALLOCATED(node)))
	{
		node->flags |= LED_MARKED_FOR_REMOVAL;
		dll_remove(&led->node_non_marked_list, led->node_pool.buf, slot.index);
		dll_append(&led->node_marked_list, led->node_pool.buf, slot.index);
	}
}

void cmd_led_node_add(void)
{
	led_node_add(g_editor, g_queue->cmd_exec->arg[0].utf8);
}

void cmd_led_node_remove(void)
{
	led_node_remove(g_editor, g_queue->cmd_exec->arg[0].utf8);
}

void led_core(struct led *led)
{
	KAS_TASK(__func__, T_LED);

	led_remove_marked_structs(led);

	KAS_END;
}
