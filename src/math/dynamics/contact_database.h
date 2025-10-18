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

#ifndef __KAS_CONTACT_DB_H__
#define __KAS_CONTACT_DB_H__

#include "kas_common.h"
#include "allocator.h"
#include "array_list.h"
#include "hash_map.h"
#include "bit_vector.h"
#include "collision.h"
#include "net_list.h"

/*
 * ==============================================================================================================
 * |						Contact Database						|
 * ==============================================================================================================
 *
 * struct contact_database: database for last and current frame contacts. Any rigid body can lookup its cached
 * and current contacts, and if necessary, invalidate any contact data.
 *
 * Frame layout:
 * 	
 * 	1. generate_contacts
 * 	2. c_db_new_frame(contact_count)	// alloc memory for frame contacts
 * 	3. c_db_add_contact(i1, i2, contact)	// add all new contacts 
 * 	4. solve
 * 	5. invalidate any contacts before caching them.
 * 	6. switch frame and cache 
 * 	7. reset frame
 */

#define C_DB_NULL 	NET_LIST_NODE_NULL_INDEX

struct contact
{
	struct net_list_node header;	/* net list intrusive header, MAY NOT BE MOVED FROM TOP */
	struct contact_manifold cm;
	u64 key;

	vec3 normal_cache;
	vec3 tangent_cache[2];
	vec3 v_cache[4];	/* previous contact manifold vertices, or { F32_MAX, F32_MAX, F32_MAX }  */
	f32 tangent_impulse_cache[4][2];
	f32 normal_impulse_cache[4];	/* contact_solver solution to contact constraint, or 0.0f */
	u32 cached_count;		/* number of vertices in cache */
};

struct contact_database
{
	/*
	 * contact net list nodes are owned as follows:
	 *
	 * contact->key & (0xffffffff00000000) >> 32 identifier owns slot 0
	 * contact->key & (0x=0000000ffffffff) >>  0 identifier owns slot 1
	 *
	 * i.e. the smaller index owns slot 0 and the larger index owns slot 1.
	 */
	struct net_list *contacts;
	struct hash_map *contact_map;		/* growable */

	/* PERSISTENT DATA, GROWABLE, keeps track of which slots in contacts are currently being used. */
	struct bit_vec contacts_persistent_usage; /* At end of frame, is set to contacts_frame_usage + any 
						     new appended contacts resulting in appending the 
						     contacts array.  */

	/* FRAME DATA, NOT GROWABLE, keeps track of which slots in contacts are currently being used. */
	struct bit_vec contacts_frame_usage;	/* bit-array showing which of the previous frame link indices
						   are reused. Thus, all links in the current frame are the
						   ones in the bit array + any appended contacts which resu-
						   -lted in growing the array. */

	/* FRAME DATA */
	u32 *broken_list;
	u32 *new_list;
	u32 broken_count;
	u32 new_count;
};

#define CONTACT_KEY_TO_BODY_0(key) 	((key) >> 32)
#define CONTACT_KEY_TO_BODY_1(key) 	((key) & U32_MAX)

struct physics_pipeline;

struct contact_database c_db_alloc(struct arena *mem_persistent, const u32 initial_size);
void 			c_db_free(struct contact_database *c_db);
void			c_db_flush(struct contact_database *c_db);
void			c_db_validate(const struct physics_pipeline *pipeline);
void			c_db_clear_frame(struct contact_database *c_db);
/* Update or add new contact depending on if the contact persisted from prevous frame. */
struct contact *	c_db_add_contact(struct physics_pipeline *pipeline, const struct contact_manifold *cm, const u32 i1, const u32 i2);
void 			c_db_remove_contact(struct physics_pipeline *pipeline, const u64 key, const u32 index);
/* Remove all contacts associated with the given body */
void			c_db_remove_body_contacts(struct physics_pipeline *pipeline, const u32 body_index);
/* Remove all contacts associated with the given static body and store affected islands */
u32 *			c_db_remove_static_contacts_and_store_affected_islands(struct arena *mem, u32 *count, struct physics_pipeline *pipeline, const u32 static_index);
struct contact *	c_db_lookup_contact(const struct contact_database *c_db, const u32 b1, const u32 b2);
u32 			c_db_lookup_contact_index(const struct contact_database *c_db, const u32 i1, const u32 i2);
void 			c_db_update_persistent_contacts_usage(struct contact_database *c_db);

#endif
