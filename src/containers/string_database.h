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

#ifndef __STRING_DATABASE_H__
#define __STRING_DATABASE_H__

#include "kas_common.h"
#include "kas_string.h"
#include "array_list.h"
#include "hash_map.h"

#define STRING_DATABASE_GROWABLE	1
#define STRING_DATABASE_STUB_INDEX	0

/*
 * string_database_node - header to be placed at the top of any structure to be stored within the database.
 */
struct string_database_node
{
	struct array_list_intrusive_node list_header;		/* header for internal use: MAY NOT BE MOVED */
	utf8 				id;			/* identifier of database object */
	u32				reference_count;	/* Number of references to the current data base object*/
};

/*
 *	1. id aliasing: on deallocation, do nothing with identifier, 
 *		(up to user to free, BUT WE MUST ENSURE LIFETIME ATLEAST AS LONG AS DATABASE)
 *	2. malloc copy: on deallocation, free identifier 
 *	3. arena copy:  on deallocation, do nothing with identifier, (up to user to free)
 */
struct string_database
{
	struct hash_map *		hash;
	struct array_list_intrusive *	list;
	u32 				growable; /* Boolean: increases the size of the database if more memory is required */
};

/* allocate and return database with entries of data_size (simply sizeof(struct)). 
 * If growable, allows the database to increase size when required. */
struct string_database *string_database_init(struct arena *mem, const u32 hash_size, const u32 index_size, const u64 data_size, const u32 growable);
/* free the database. NOTE that none of the database id strings are freed as they are either aliases or arena memory. */
void			string_database_free(struct string_database *db);
/* flush or reset the string database */
void			string_database_flush(struct string_database *db);
/* allocate a new database node with the given identifier and return its index (handle). 
   The id will be copied onto the arena. On failure, the stub slot (0, NULL) is returned. */
struct allocation_slot	string_database_add(struct arena *mem_db_lifetime, struct string_database *db, const utf8 id);
/* allocate a new database node with the given identifier and return its index. 
 * The id will alias the given string's content. On failure, the stub slot (0, NULL) is returned.  */
struct allocation_slot	string_database_add_and_alias(struct string_database *db, const utf8 id);
/* remove the identifier's corresponding database node if found and update database state, otherwise do nothing. */
void			string_database_remove(struct string_database *db, const utf8 id);
/* Lookup the identifer in the database. If it exist, return its slot. Otherwise return (0, NULL). */
struct allocation_slot	string_database_lookup(const struct string_database *db, const utf8 id);
/* Return the corresponding address of the index. */
void *			string_database_address(const struct string_database *db, const u32 handle);
/* Return the result of the lookup operation. furthermore, if the returned slot is not (0, NULL), increment the corresponding node's reference count.  */
struct allocation_slot 	string_database_reference(struct string_database *db, const utf8 id);
/* Lookup the handle in the database. If it exist, decrement the corresponding node's reference count. */
void			string_database_dereference(struct string_database *db, const u32 handle);

#endif
