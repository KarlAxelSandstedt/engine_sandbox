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

#ifndef __DS_HASH_MAP_H__
#define __DS_HASH_MAP_H__

#include "ds_common.h"
#include "serialize.h"
#include "allocator.h"

#define HASH_NULL 	U32_MAX
#define HASH_GROWABLE	1
#define HASH_STATIC	0

/*
 * hash map storing mapping a key to a set of possible indices. User dereferences indices to check for equality between identifiers 
 */
struct hash_map
{
	u32 *	hash;
	u32 *	index;
	u32	hash_len;
	u32	index_len;
	u32	hash_mask;
	u32	growable;
};

/* allocate hash map on heap if mem == NULL, otherwise push memory onto arena. On failure, returns NULL  */
struct hash_map	*	hash_map_alloc(struct arena *mem, const u32 hash_len, const u32 index_len, const u32 growable);
/* free hash map memory */
void			hash_map_free(struct hash_map *map);
/* flush / reset the hash map, removing any allocations within it */
void			hash_map_flush(struct hash_map *map);
/* serialize hash map into stream  */
void 			hash_map_serialize(struct serialize_stream *ss, const struct hash_map *map);
/* deserialize and construct hash_map on arena if defined, otherwise alloc on heap. On failure, returns NULL  */
struct hash_map *	hash_map_deserialize(struct arena *mem, struct serialize_stream *ss, const u32 growable);
/* add the key-index pair to the hash map. return 1 on success, 0 on out-of-memory. */
u32			hash_map_add(struct hash_map *map, const u32 key, const u32 index);
/* remove  key-index pair to the hash map. If the pair is not found, do nothing. */
void			hash_map_remove(struct hash_map *map, const u32 key, const u32 index);
/* Get the first (key,index) pair of the map. If HASH_NULL is returned, no more pairs exist */
u32			hash_map_first(const struct hash_map *map, const u32 key);
/* Get the next (key,index) pair of the map. If HASH_NULL is returnsd, no more pairs exist */
u32			hash_map_next(const struct hash_map *map, const u32 index);

/* keygen methods */
u64			key_gen_u32_u32(const u32 k1, const u32 k2);

#endif
