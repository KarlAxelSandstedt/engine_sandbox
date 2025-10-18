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

#include "sys_local.h"

struct file file_null(void)
{
	return (struct file) { .handle = FILE_HANDLE_INVALID, .path = utf8_empty(), .type = FILE_NONE };
}

struct directory_navigator directory_navigator_alloc(const u32 initial_memory_string_size, const u32 hash_size, const u32 initial_hash_index_size)
{
	struct directory_navigator dn =
	{
		.path = utf8_empty(),
		.relative_path_to_file_map = hash_map_alloc(NULL, hash_size, initial_hash_index_size, HASH_GROWABLE),
		.mem_string = arena_alloc(initial_memory_string_size),
		.files = vector_alloc(NULL, sizeof(struct file), initial_hash_index_size, VECTOR_GROWABLE),
	};

	return dn;
}

void directory_navigator_dealloc(struct directory_navigator *dn)
{
	arena_free(&dn->mem_string);
	hash_map_free(dn->relative_path_to_file_map);
	vector_dealloc(&dn->files);
}

void directory_navigator_flush(struct directory_navigator *dn)
{
	arena_flush(&dn->mem_string);
	hash_map_flush(dn->relative_path_to_file_map);
	vector_flush(&dn->files);
}

u32 directory_navigator_lookup_substring(struct arena *mem, u32 **index, struct directory_navigator *dn, const utf8 substring)
{
	arena_push_record(&dn->mem_string);

	struct kmp_substring kmp_substring = utf8_lookup_substring_init(&dn->mem_string, substring);
	*index = (u32 *) mem->stack_ptr;
	u32 count = 0;

	for (u32 i = 0; i < dn->files.next; ++i)
	{
		const struct file *file = vector_address(&dn->files, i);
		if (utf8_lookup_substring(&kmp_substring, file->path))
		{
			arena_push_packed_memcpy(mem, &i, sizeof(i));
			count += 1;
		}
	}

	arena_pop_record(&dn->mem_string);
	return count;
}

u32 directory_navigator_lookup(const struct directory_navigator *dn, const utf8 filename)
{
	const u32 key = utf8_hash(filename);
	u32 index = HASH_NULL;
	for (u32 i = hash_map_first(dn->relative_path_to_file_map, key); i != HASH_NULL; i = hash_map_next(dn->relative_path_to_file_map, i))
	{
		const struct file *file = vector_address(&dn->files, i);
		if (utf8_equivalence(filename, file->path))
		{
			index = i;
			break;
		}
	}

	return index;
}

enum fs_error directory_navigator_enter_and_alias_path(struct directory_navigator *dn, const utf8 path)
{
	directory_navigator_flush(dn);

	struct file dir = file_null();
	const enum fs_error ret = directory_try_open_at_cwd(&dn->mem_string, &dir, cstr_utf8(&dn->mem_string, path));
	if (ret == FS_SUCCESS)
	{
		dn->path = path;
		directory_push_entries(&dn->mem_string, &dn->files, &dir);
		for (u32 i = 0; i < dn->files.next; ++i)
		{
			const struct file *entry = vector_address(&dn->files, i);
			const u32 key = utf8_hash(entry->path);
			hash_map_add(dn->relative_path_to_file_map, key, i);
		}
	}

	return ret;
}
