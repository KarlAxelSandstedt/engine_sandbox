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

#ifndef __PHYSICS_PIPELINE_ISLAND_H__
#define __PHYSICS_PIPELINE_ISLAND_H__

/*
 *
 * ==============================================================================================================
 * |						Persistent Islands						|
 * ==============================================================================================================
 *
 * struct persistent_island: Persistent island over several frames. Justification is that island information
 * may possibly not change much from frame to frame, so storing persistent island data may work as an optimization.
 * It would also be of help in storing cached collision/body data between frames.
 *
 * Operations:
 * 	(1) island_initialize(body)	- Initalize new island from a body (valid for being in an island)
 * 	(2) island_split()		- We must be able to split an island no longer fully connected
 * 	(3) island_merge() 		- We must be able to merge two islands now connected
 *
 * Auxilliary Operation:
 * 	(1)	contact_cache_get_persistent_contacts()	
 * 	(2)	contact_cache_get_new_contacts() * 	(3)	contact_cache_get_deleted_contacts()
 *
 *
 * ----- Island Consistency: Knowing when to split, and when to merge -----
 *
 * In order to know that we should split an island, or merge two islands, we must have ways to reason about
 * the connectivity of islands. The physics pipeline ensure that islands are valid at the start of frames,
 * except perhaps for the first frame. The frame layout should look something like:
 *
 * 	[1] solve island local system
 * 		(1) We may now have broken islands
 * 	[2] finalize bodies, cache contact data  
 * 		(1) Islands contain up-to-date information and caches for bodies (which may no longer be connected) 
 * 		(2) if (cache_map.entry[i] == no_update) => Connection corresponding to entry i no longer exists
 * 	[3] construct new contact_data
 * 		(2) if (cache_map(contact) ==    hit) => Connection remains, (possibly in a new island)
 * 		(3) if (cache_map(contact) == no_hit) => A new connection has been established, 
 * 						         (possibly between two islands)
 *	[4] update/construct islands
 *
 * It follows that if we keep track of 
 *
 *	(1) what contacts were removed from the contact_cache	- deleted links
 *	(2) what contacts were added to the contact cache	- new links
 *	(3) what contacts remain in the contact cache 		- persistent links
 *
 * we have all the sufficient (and necessary) information to re-establish the invariant of correct islands at the
 * next frame.
 *
 *
 * ----- Island Memory: Handling Lifetimes and Memory Layouts (Sanely) -----
 *
 * The issue with persistent islands is that the lifetime of the island is not (generally) shared with to bodies
 * it rules over. It would be possible to limit the islands to using linked lists if we ideally only would have 
 * to iterate each list once. This would greatly simplify the memory management. We consider what data must be
 * delivered to and from the island at what time:
 *
 *
 * FRAME n: 	...
 *	 	...
 * 	==== Contact Cache ====
 * 	[3, 4] construct new contact data + update/construct islands
 * 		list of body indices 		=> island
 * 		list of constraint indices 	=> island
 *
 * FRAME n+1:
 * 	==== Island ====
 * 	[1] solve island local system
 * 		island.constraints.data		=> solver
 * 		island.bodies data		=> solver
 *
 * 	==== Solver ====
 *      [2] finalize bodies, cache contact data
 *		solve.solution			=> contact cache
 *		cache constraints		=> contact cache
 *
 *
 * Assuming that the island only contains linked lists of indices to various data, we wish to fully defer any
 * lookups into that data until the Solver stage. [1] We traverse the lists and retrieve the wanted data. This
 * data (ListData) is kept throughout [2], [3], and discarded at [4] when islands are split/merged.
 *
 */

#include "kas_common.h"
#include "contact_database.h"

#define BODY_NO_ISLAND_INDEX 	U32_MAX

#define ISLAND_AWAKE		(0x1u << 0u)
#define ISLAND_SLEEP_RESET	(0x1u << 1u)	/* reset sleep timers on frame */
#define ISLAND_SPLIT		(0x1u << 2u)	/* flag island for splitting */
#define ISLAND_TRY_SLEEP	(0x1u << 3u)	/* flag island for being put to sleep at next solve iteration 
						 * (if the island is uninterrupted for a frame) This is needed
						 * since if we determine that an updated island should be put
						 * to sleep at end of a frame in island_solve, we must atleast
						 * update all rigid body proxies before butting the bodies to
						 * sleep as well, so keep the island awake for another frame
						 * without solving it at the end if it is uninterrupted.
						 */

#define ISLAND_AWAKE_BIT(is)		(((is)->flags & ISLAND_AWAKE) >> 0u)
#define ISLAND_SLEEP_RESET_BIT(is)	(((is)->flags & ISLAND_SLEEP_RESET) >> 1u)
#define ISLAND_SPLIT_BIT(is)		(((is)->flags & ISLAND_SPLIT) >> 2u)
#define ISLAND_TRY_SLEEP_BIT(is)	(((is)->flags & ISLAND_TRY_SLEEP) >> 3u)

#define ISLAND_NULL	U32_MAX 
#define ISLAND_STATIC	U32_MAX-1	/* static bodies are mapped to "island" ISLAND_STATIC */

struct island
{
	struct rigid_body **	bodies;	
	struct contact 	**	contacts;
	u32 *			body_index_map; /* body_index -> local indices of bodies in island:
						 * is->bodies[i] = pipeline->bodies[b] => 
						 * is->body_index_map[b] = i 
						 */

	//TODO REMOVE
	u32 cm_count;

	/* Persistent Island */
	u32 flags;

	u32 body_first;			/* index into first node in island_body_lists 		*/
	u32 contact_first;		/* index into first node in island_contact_lists 	*/

	u32 body_last;			/* index into last node in island_body_lists 		*/
	u32 contact_last;		/* index into last node in island_contact_lists 	*/

	u32 body_count;
	u32 contact_count;

#ifdef	KAS_PHYSICS_DEBUG
	vec4 	color;
#endif
};

struct is_index_entry
{
	u32 index;
	u32 next;
};

struct island_database
{
	/* PERSISTENT DATA */
	struct bit_vec island_usage;			/* NOT GROWABLE, bit vector for islands in use */
	struct array_list *islands;			/* NOT GROWABLE, set to max_body_count 	*/
	struct array_list *island_contact_lists;	/* GROWABLE, list nodes to contacts 	*/
	struct array_list *island_body_lists;		/* NOT GROWABLE, list nodes to bodies   */

	/* FRAME DATA */
	u32 *possible_splits;				/* Islands in which a contact has been broken during frame */
	u32 possible_splits_count;
};

#ifdef KAS_DEBUG
#define IS_DB_VALIDATE(pipeline)	is_db_validate((pipeline)
#else
#define IS_DB_VALIDATE(pipeline)	
#endif

struct physics_pipeline;

/* Setup and allocate memory for new database */
struct island_database 	is_db_alloc(struct arena *mem_persistent, const u32 initial_size);
/* Free any heap memory */
void		       	is_db_free(struct island_database *is_db);
/* Flush / reset the island database */
void			is_db_flush(struct island_database *is_db);
/* Clear any frame related data */
void			is_db_clear_frame(struct island_database *is_db);
/* remove island resources from database */
void 			is_db_island_remove(struct physics_pipeline *pipeline, struct island *is);
/* remove island resources related to body, and possibly the whole island, from database */
void 			is_db_island_remove_body_resources(struct physics_pipeline *pipeline, const u32 island_index, const u32 body);
/* Debug printing of island */
void is_db_print_island(FILE *file, const struct island_database *is_db, const struct contact_database *c_db, const u32 island, const char *desc);
/* Check if the database appears to be valid */
void 			is_db_validate(const struct physics_pipeline *pipeline);
/* Setup new island from single body */
struct island *		is_db_init_island_from_body(struct physics_pipeline *pipeline, const u32 body);
/* Add contact to island */
void 			is_db_add_contact_to_island(struct island_database *is_db, const u32 island, const u32 contact);
/* Return island that body is assigned to */
struct island *		is_db_body_to_island(struct physics_pipeline *pipeline, const u32 body);
/* Reserve enough memory to fit all possible split */
void			is_db_reserve_splits_memory(struct arena *mem_frame, struct island_database *is_db);
/* Release any unused reserved possible split memory */
void			is_db_release_unused_splits_memory(struct arena *mem_frame, struct island_database *is_db);
/* Tag the island that the body is in for splitting and push it onto split memory (if we havn't already) */
void 			is_db_tag_for_splitting(struct physics_pipeline *pipeline, const u32 body);
/* Merge islands (Or simply update if new local contact) using new contact */
void 			is_db_merge_islands(struct physics_pipeline *pipeline, const u32 ci, const u32 b1, const u32 b2);
/* Split island, or remake if no split happens: TODO: Make thread-safe  */
void 			is_db_split_island(struct arena *mem_tmp, struct physics_pipeline *pipeline, const u32 island_to_split);
/* Solve island constraints, and update bodies in pipeline */
//void 			island_solve(struct arena *mem_frame, struct physics_pipeline *pipeline, struct island *is, const f32 timestep);

/********* Threaded Island API *********/

struct island_solve_output
{
	u32 island;
	u32 island_asleep;
	u32 body_count;
	u32 *bodies;		/* bodies simulated in island */ 
	struct island_solve_output *next;
};

struct island_solve_input
{
	struct island *is;
	struct physics_pipeline *pipeline;
	struct island_solve_output *out;
	f32 timestep;
};

/*
 * Input: struct island_solve_in 
 * Output: struct island_solve_output
 *
 * Solves the given island using the global solver config. Since no island shares any contacts or bodies, and every
 * island is a unique task, no shared variables are being written to.
 *
 * - reads pipeline, solver config, c_db, is_db (basically everything)
 * - writes to island,		(unique to thread, memory in c_db)
 * - writes to island->contacts (unique to thread, memory in c_db)
 * - writes to island->bodies	(unique to thread, memory in pipeline)
 */
void	thread_island_solve(void *task_input);

#endif
