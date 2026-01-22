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

#include <stdlib.h>

#include "hash_map.h"
#include "sys_public.h"
#include "ds_math.h"

struct hash_map *hash_map_alloc(struct arena *mem, const u32 hash_len, const u32 index_len, const u32 growable)
{
	ds_Assert(hash_len && index_len && (hash_len >> 31) == 0);
	const u32 actual_hash_len = (u32) power_of_two_ceil(hash_len);

	struct hash_map *map = NULL;
	u32 *hash = NULL;
	u32 *index = NULL;
	if (mem)
	{

		map = arena_push(mem, sizeof(struct hash_map));
		hash = arena_push(mem, actual_hash_len * sizeof(u32)); 
		index = arena_push(mem, index_len * sizeof(u32)); 
	}
	else
	{
		map = malloc(sizeof(struct hash_map));
		hash = malloc(actual_hash_len * sizeof(u32));
		index = malloc(index_len * sizeof(u32));
	}

	if (!hash || !index || !map)
	{
		return NULL;
	}

	map->hash = hash;
	map->index = index;
	map->hash_len = actual_hash_len;
	map->index_len = index_len;
	map->growable = growable;
	map->hash_mask = map->hash_len-1;

	for (u32 i = 0; i < map->hash_len; ++i)
	{
		map->hash[i] = HASH_NULL;
	}

	return map;
}

void hash_map_free(struct hash_map *map)
{
	if (map)
	{
		free(map->hash);
		free(map->index);
		free(map);
	}	
}

void hash_map_flush(struct hash_map *map)
{
	for (u32 i = 0; i < map->hash_len; ++i)
	{
		map->hash[i] = HASH_NULL;
	}
}

void hash_map_serialize(struct serialize_stream *ss, const struct hash_map *map)
{
	if ((2 + map->hash_len + map->index_len) * sizeof(u32) <= ss_bytes_left(ss))
	{
		ss_write_u32_be(ss, map->hash_len);
		ss_write_u32_be(ss, map->index_len);
		ss_write_u32_be_array(ss, map->hash, map->hash_len);
		ss_write_u32_be_array(ss, map->index, map->index_len);
	}
}

struct hash_map *hash_map_deserialize(struct arena *mem, struct serialize_stream *ss, const u32 growable)
{
	ds_Assert(!(mem && growable));
	if (2 * sizeof(u32) > ss_bytes_left(ss))
	{
		return NULL;
	}

	const u32 hash_len = ss_read_u32_be(ss);
	const u32 index_len = ss_read_u32_be(ss);

	u32 *hash;
	u32 *index;
	struct hash_map *map;

	if (mem)
	{
		arena_push_record(mem);
		hash = arena_push(mem, hash_len * sizeof(u32));
		index = arena_push(mem, index_len * sizeof(u32));
		map = arena_push(mem, sizeof(struct hash_map));
		if (!map || !index || !hash)
		{
			arena_pop_record(mem);
			return NULL;
		}
	}
	else
	{
		hash = malloc(hash_len * sizeof(u32));
		if (!hash)
		{
			return NULL;
		}

		index = malloc(index_len * sizeof(u32));
		if (!index)
		{
			free(hash);
			return NULL;
		}

		map = malloc(sizeof(struct hash_map));
		if (!map)
		{
			free(hash);
			free(index);
			return NULL;
		}
	}

	map->hash_len = hash_len;
	map->hash = hash;
	map->index_len = index_len;
	map->index = index;
	map->growable = growable;
	map->hash_mask = map->hash_len-1;

	if (map->hash_len + map->index_len * sizeof(u32) <= ss_bytes_left(ss))
	{
		ss_read_u32_be_array(map->hash, ss, map->hash_len);
		ss_read_u32_be_array(map->index, ss, map->index_len);
	}

	return map;
}

u32 hash_map_add(struct hash_map *map, const u32 key, const u32 index)
{
	ds_Assert(index >> 31 == 0);

	if (map->index_len <= index)
	{
		if (map->growable)
		{
			map->index_len = (u32) power_of_two_ceil(index+1);
			map->index = realloc(map->index, map->index_len * sizeof(u32));
		}
		else
		{
			return 0;	
		}
	}

	const u32 h = key & map->hash_mask;
	
	map->index[index] = map->hash[h];
	map->hash[h] = index;
	return 1;
}

void hash_map_remove(struct hash_map *map, const u32 key, const u32 index)
{
	ds_Assert(index < map->index_len);

	const u32 h = key & map->hash_mask;
	if (map->hash[h] == index)
	{
		map->hash[h] = map->index[index];
	}
	else
	{
		for (u32 i = map->hash[h]; i != HASH_NULL; i = hash_map_next(map, i))
		{
			if (map->index[i] == index)
			{
				map->index[i] = map->index[index];	
				break;
			}
		}
	}

	/* Only for debug purposes  */
	map->index[index] = HASH_NULL;
}

u32 hash_map_first(const struct hash_map *map, const u32 key)
{
	return map->hash[key & map->hash_mask];
}

u32 hash_map_next(const struct hash_map *map, const u32 index)
{
	return (index < map->index_len)
		? map->index[index]
		: HASH_NULL;
}

u64 key_gen_u32_u32(const u32 k1, const u32 k2)
{
       return ((u64) k1 << 32) | (u64) k2;
}
