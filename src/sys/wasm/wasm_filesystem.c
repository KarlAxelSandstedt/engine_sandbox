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

#include "wasm_local.h"

#include <dirent.h>
#include <unistd.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>

#include "wasm_public.h"
#include "wasm_local.h"

u32 			(*system_user_is_admin)(void);

u32			(*path_is_relative)(const kas_string *path);

struct kas_buffer 	(*file_dump)(struct arena *mem, const kas_string *path);
file_handle 		(*file_open_for_reading)(const kas_string *path);
u32 			(*file_try_create_or_truncate)(file_handle *handle, const kas_string *filename);
void 			(*file_close)(file_handle handle);
u32 			(*file_write_offset)(file_handle handle, const u8 *buf, const u32 bufsize, const u64 offset);
u32 			(*file_write_append)(file_handle handle, const u8 *buf, const u32 size);
void 			(*file_sync)(file_handle handle);
void *			(*file_memory_map)(u64 *size, const file_handle handle, const u32 prot, const u32 flags);
void *			(*file_memory_map_partial)(const file_handle handle, const u64 length, const u64 offset, const u32 prot, const u32 flags);
void 			(*file_memory_unmap)(void *addr, const u64 length);
void 			(*file_memory_sync_unmap)(void *addr, const u64 length);

void 			(*directory_print_tree)(struct arena *tmp, const struct kas_directory *dir);
kas_string 		(*directory_current_path)(struct arena *mem);
kas_string		(*directory_current_path_buffered)(u8 *buf, const u32 bufsize);
enum kas_fs_error_type  (*directory_get_current)(struct kas_directory *dir, struct arena *mem);
enum kas_fs_error_type  (*directory_get_current_buffered)(struct kas_directory *dir, u8 *buf, const u32 bufsize);
enum kas_fs_error_type 	(*directory_try_create_relative_to)(struct kas_directory *dir, const struct kas_directory *relative, const kas_string *path);
enum kas_fs_error_type 	(*directory_try_remove)(const struct kas_directory *dir);

void			(*file_status_print)(const file_status *stat);
enum kas_fs_error_type	(*file_status_from_directory)(file_status *status, const struct kas_directory *dir);
enum kas_fs_error_type	(*file_status_from_handle)(file_status *status, const file_handle handle);
enum kas_fs_error_type	(*file_status_from_path)(file_status *status, const kas_string *path);

u32 wasm_system_user_is_admin(void)
{
	return getuid() == 0;
}

u32 wasm_path_is_relative(const kas_string *path)
{
	kas_assert_string(path, "kas_strings should never be invalid!, use kas_string_empty().");

	return (path->buf[0] == '/') ? 0 : 1;
}

struct kas_buffer wasm_file_dump(struct arena *mem, const kas_string *path)
{
	const file_handle handle = open((char *) path->buf, O_RDONLY);
	if (handle == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR, 0);
		return kas_buffer_empty();
	}

	
	struct stat stat;
	if (file_status_from_handle(&stat, handle) != KAS_FS_SUCCESS)
	{
		close(handle);
		return kas_buffer_empty();	
	}

	struct kas_buffer buf =
	{
			.size = (u64) stat.st_size,
			.mem_left = (u64) stat.st_size,
	};

	struct arena record;
	if (mem)
	{
		record = *mem;
		buf.data = arena_push(mem, NULL, (u64) stat.st_size);
	}
	else
	{
		buf.data = malloc((u64) stat.st_size);
	}

	if (!buf.data)
	{
		close(handle);
		return kas_buffer_empty();	
	}

	u64 bytes_left = buf.size;
	i64 bytes_read_in_call;
	while (0 < bytes_left)
	{
		bytes_read_in_call = read(handle, buf.data + (buf.size - bytes_left), bytes_left);
		if (bytes_read_in_call == -1)
		{
			LOG_SYSTEM_ERROR(S_ERROR, 0);
			buf = kas_buffer_empty();
			if (mem)
			{
				*mem = record;
			}
			else
			{
 				free(buf.data);
			}
			break;
		}
		bytes_left -= (u64) bytes_read_in_call;	
	}
		
	close(handle);
	return buf;

}

file_handle wasm_file_open_for_reading(const kas_string *path)
{
	const file_handle fd = open((char *) path->buf, O_RDONLY);
	if (fd == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR, 0);
		return FILE_HANDLE_INVALID;
	}

	return fd;
}

u32 wasm_file_try_create_or_truncate(file_handle *handle, const kas_string *filename)
{
	kas_assert_string(filename, "kas_strings should never be invalid!, use kas_string_empty().");

	*handle = openat(AT_FDCWD, (char *) filename->buf, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP);

	if (*handle == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR, 0);
		return 0;
	}
	else
	{
		return 1;
	}
}

void wasm_file_close(file_handle handle)
{
	if (close(handle) == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR, 0);
	}
}

u32 wasm_file_write_offset(file_handle handle, const u8 *buf, const u32 bufsize, const u64 offset)
{
	if (!buf || bufsize == 0) { return 0; }

	const off_t ret = lseek64(handle, (off_t) offset, SEEK_SET);
	if (ret == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR, 0);
		return 0;
	}

	ssize_t left = (ssize_t) bufsize;
	ssize_t count = 0;
	ssize_t total = 0;
	while (left)
	{
		count = write(handle, buf + total, left);
		if (count == -1)
		{
			LOG_SYSTEM_ERROR(S_ERROR, 0);
			break;
		}

		total += count;
		left -= count;
	}

	return total;

}

u32 wasm_file_write_append(file_handle handle, const u8 *buf, const u32 bufsize)
{
	if (!buf || bufsize == 0) { return 0; }

	const off_t ret = lseek64(handle, 0, SEEK_END);
	if (ret == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR, 0);
		return 0;
	}

	ssize_t left = (ssize_t) bufsize;
	ssize_t count = 0;
	ssize_t total = 0;
	while (left)
	{
		count = write(handle, buf + total, left);
		if (count == -1)
		{
			LOG_SYSTEM_ERROR(S_ERROR, 0);
			break;
		}

		total += count;
		left -= count;
	}

	return total;
}

void wasm_file_sync(file_handle handle)
{
	fsync(handle);
}

void *wasm_file_memory_map(u64 *size, const file_handle handle, const u32 prot, const u32 flags)
{
	*size = 0;
	void *map = NULL;
	struct stat stat;
	if (file_status_from_handle(&stat, handle) == KAS_FS_SUCCESS)
	{
		*size = stat.st_size;
		map = file_memory_map_partial(handle, stat.st_size, 0, prot, flags);
	}
	return map;
}

void *wasm_file_memory_map_partial(const file_handle handle, const u64 length, const u64 offset, const u32 prot, const u32 flags)
{
	void *addr = mmap(NULL, length, prot, flags, handle, (off_t) offset);
	if (addr == MAP_FAILED)
	{
		LOG_SYSTEM_ERROR(S_ERROR, 0);
		addr = NULL;
	}

	return addr;
}

void wasm_file_memory_unmap(void *addr, const u64 length)
{
	if (munmap(addr, length) == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR, 0);
	}
}

void wasm_file_memory_sync_unmap(void *addr, const u64 length)
{
	if (msync(addr, length, MS_SYNC) == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR, 0);
	}

	if (munmap(addr, length) == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR, 0);
	}
}

void wasm_directory_print_tree(struct arena *tmp, const struct kas_directory *dir)
{
	struct arena record = *tmp;

	u32 s_len = 0;
	u64 r_size = 32;
	u64 s_size = 256;
	struct kas_string *dir_name_stack = arena_push(tmp, NULL, s_size * sizeof(struct kas_string));
	kas_assert(dir_name_stack);
	
	DIR *root = fdopendir(dir->handle);
	if (!root)
	{
		*tmp = record;
		LOG_SYSTEM_ERROR(S_WARNING, 0);
		return;
	}

	const kas_string dot = KAS_COMPILE_TIME_STRING(".");
	const kas_string dotdot = KAS_COMPILE_TIME_STRING("..");

	kas_string cur_path = dir->relative_path;
	DIR *cur_dir = root;
	fprintf(stderr, "root of tree:\t%s\n", (char *) dir->relative_path.buf);
	for (;;)
	{
		errno = 0;
		struct stat stat_buf;
		struct dirent *ent;
		/* thread-safe as long no threads read the same DIR *ptr. */
		while ((ent = readdir(cur_dir)) != NULL)
		{
			dir_name_stack[s_len] = kas_string_format(tmp, "%k/%s", &cur_path,  ent->d_name);
			if (stat((char *) dir_name_stack[s_len].buf, &stat_buf) == -1)
			{
				fprintf(stderr, "%s\n", (char *) dir_name_stack[s_len].buf);
				break;
			}
				
			if (S_ISDIR(stat_buf.st_mode) 
					&& !(ent->d_name[0] == '.' && ent->d_name[1] == '\0')
					&& !(ent->d_name[0] == '.' && ent->d_name[1] == '.' && ent->d_name[2] == '\0')
					)
			{
				/* Must keep one slot as tmp storage */
				s_len += 1;
				if (s_len == s_size)
				{
					errno = ENOMEM;
					break;
				}
			}
			else
			{
				fprintf(stderr, "%s\n", (char *) dir_name_stack[s_len].buf);
			}
		}
		
		if (errno != 0)
		{
			LOG_SYSTEM_ERROR_CODE(S_ERROR, 0, errno);
			closedir(cur_dir);
			break;
		}

		closedir(cur_dir);
		if (s_len)
		{
			s_len -= 1;
			cur_path = dir_name_stack[s_len];
			cur_dir = opendir((char *) cur_path.buf);
			if (cur_dir)
			{
				fprintf(stderr, "%s\n", cur_path.buf);
				continue;
			}

			LOG_SYSTEM_ERROR(S_ERROR, 0);
		}

		break;
	}

	*tmp = record;
}

kas_string wasm_directory_current_path(struct arena *mem)
{
	struct arena record = *mem;
	u32 tmp_size = 256;
	u8 *buf = arena_push_packed(mem, NULL, tmp_size);

	while (!getcwd((char *) buf, tmp_size))
	{
		if (errno == ERANGE)
		{
			LOG_MESSAGE(T_SYSTEM, S_WARNING, 0, "small buffer in %s, dubbling size to %u\n", __func__, 2*tmp_size);
			arena_push_packed(mem, NULL, tmp_size);
			tmp_size *= 2;
			continue;
		}
		else
		{
			LOG_SYSTEM_ERROR(S_ERROR, 0);
			*mem = record;
			return kas_string_empty();
		}
	}

	kas_string path;
	path.buf = buf;
	path.len = strlen((char *) buf);
	path.size = path.len + 1;
	arena_pop_packed(mem, tmp_size - path.size); 

	return path;
}

kas_string wasm_directory_current_path_buffered(u8 *buf, const u32 bufsize)
{
	if (!getcwd((char *) buf, bufsize))
	{
		LOG_SYSTEM_ERROR(S_WARNING, 0);
		return kas_string_empty();
	}

	kas_string path =
	{
		.buf = buf,
		.len = strlen((char *) buf),
		.size = bufsize,
	};

	return path;
}

enum kas_fs_error_type wasm_directory_get_current(struct kas_directory *dir, struct arena *mem)
{
	struct arena record = *mem;

	dir->relative_path = directory_current_path(mem);
	dir->handle = file_open_for_reading(&dir->relative_path);
	if (dir->handle == FILE_HANDLE_INVALID)
	{
		*mem = record;
		return KAS_FS_ERROR_UNSPECIFIED;
	}

	return KAS_FS_SUCCESS;
}

enum kas_fs_error_type wasm_directory_get_current_buffered(struct kas_directory *dir, u8 *buf, const u32 bufsize)
{
	dir->relative_path = directory_current_path_buffered(buf, bufsize);
	if (dir->relative_path.len == 0)
	{
		dir->handle = FILE_HANDLE_INVALID;
		return KAS_FS_BUFFER_TO_SMALL;
	}

	dir->handle = file_open_for_reading(&dir->relative_path);
	return (dir->handle != FILE_HANDLE_INVALID)
		? KAS_FS_SUCCESS
		: KAS_FS_ERROR_UNSPECIFIED;
}

enum kas_fs_error_type wasm_directory_try_create_relative_to(struct kas_directory *dir, const struct kas_directory *relative, const kas_string *path)
{
	if (!wasm_path_is_relative(path))
	{
		LOG_MESSAGE(T_SYSTEM, S_WARNING, 0, "In function %s: path is not relative.\n", __func__);
		return KAS_FS_PATH_INVALID;
	}

	const mode_t mode = S_IRWXU | S_IRGRP | S_IROTH;
	const int fd = mkdirat(relative->handle, (char *) path->buf, mode);
	if (fd == -1)
	{
		LOG_SYSTEM_ERROR(S_WARNING, 0);
		if (errno == EEXIST)
		{
			return KAS_FS_ALREADY_EXISTS;
		}	
		else
		{
			return KAS_FS_ERROR_UNSPECIFIED;
		}
	}

	dir->handle = fd;
	dir->relative_path = *path;

	return KAS_FS_SUCCESS;
}

enum kas_fs_error_type wasm_directory_try_remove(const struct kas_directory *dir)
{
	if (rmdir((char *) dir->relative_path.buf) == -1)
	{
		LOG_SYSTEM_ERROR(S_WARNING, 0);
		if (errno == ENOTEMPTY || errno == EEXIST)
		{
			return KAS_FS_DIRECTORY_NOT_EMPTY;
		}
		else
		{
			return KAS_FS_ERROR_UNSPECIFIED;
		}
	}

	return KAS_FS_SUCCESS;
}

enum kas_fs_error_type wasm_file_status_from_directory(file_status *status, const struct kas_directory *dir)
{
	if (fstat(dir->handle, status) == -1)

	{
		LOG_SYSTEM_ERROR(S_ERROR, 0);	
		return KAS_FS_ERROR_UNSPECIFIED;
	}

	return KAS_FS_SUCCESS;
}

enum kas_fs_error_type wasm_file_status_from_handle(file_status *status, const file_handle handle)
{
	if (fstat(handle, status) == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR, 0);	
		return KAS_FS_ERROR_UNSPECIFIED;
	}

	return KAS_FS_SUCCESS;
}

enum kas_fs_error_type wasm_file_status_from_path(file_status *status, const kas_string *path)
{
	if (stat((char *) path->buf, status) == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR, 0);	
		return KAS_FS_ERROR_UNSPECIFIED;
	}

	return KAS_FS_SUCCESS;
}

void wasm_file_status_print(const file_status *stat)
{
       //struct stat {
       //    dev_t      st_dev;      /* ID of device containing file */
       //    ino_t      st_ino;      /* Inode number */
       //    mode_t     st_mode;     /* File type and mode */
       //    nlink_t    st_nlink;    /* Number of hard links */
       //    uid_t      st_uid;      /* User ID of owner */
       //    gid_t      st_gid;      /* Group ID of owner */
       //    dev_t      st_rdev;     /* Device ID (if special file) */
       //    off_t      st_size;     /* Total size, in bytes */
       //    blksize_t  st_blksize;  /* Block size for filesystem I/O */
       //    blkcnt_t   st_blocks;   /* Number of 512 B blocks allocated */

       //    /* Since POSIX.1-2008, this structure supports nanosecond
       //       precision for the following timestamp fields.
       //       For the details before POSIX.1-2008, see VERSIONS. */

       //    struct timespec  st_atim;  /* Time of last access */
       //    struct timespec  st_mtim;  /* Time of last modification */
       //    struct timespec  st_ctim;  /* Time of last status change */

       //#define st_atime  st_atim.tv_sec  /* Backward compatibility */
       //#define st_mtine  st_mtim.tv_sec
       //#define st_ctime  st_ctim.tv_sec
       //};
       
	switch (stat->st_mode & S_IFMT)
	{
		case S_IFREG: { fprintf(stderr, 	"regular file\n"); 	} break;
		case S_IFDIR: { fprintf(stderr, 	"drectory\n"); 		} break;
		case S_IFCHR: { fprintf(stderr, 	"character device\n"); 	} break;
		case S_IFBLK: { fprintf(stderr, 	"block device\n"); 	} break;
		case S_IFIFO: { fprintf(stderr, 	"fifo or pipe\n"); 	} break;
		case S_IFSOCK:{ fprintf(stderr, 	"socket\n"); 		} break;
		case S_IFLNK: { fprintf(stderr, 	"symbolic link\n"); 	} break;
	}
       
	fprintf(stderr, "file inode (%li) on device (major:minor) - %li : %li\n", 
			(long) stat->st_ino,
			(long) major(stat->st_dev), 
			(long) minor(stat->st_dev));

	fprintf(stderr, "st_mode %lo:\n", (long unsigned) stat->st_mode);
	fprintf(stderr, "\tspecial bits: (set-user-ID, set-group-ID, sticky-bit) = %u%u%u\n",
			(stat->st_mode & S_ISUID) ? 1 : 0,
			(stat->st_mode & S_ISGID) ? 1 : 0,
			(stat->st_mode & S_ISVTX) ? 1 : 0);
	fprintf(stderr, "\t      us gp ot\n");
	fprintf(stderr, "\tmask: %c%c%c%c%c%c%c%c%c\n",
			(stat->st_mode & S_IRUSR) ? 'r' : '-',
			(stat->st_mode & S_IWUSR) ? 'w' : '-',
			(stat->st_mode & S_IXUSR) ? 'x' : '-',
			(stat->st_mode & S_IRGRP) ? 'r' : '-',
			(stat->st_mode & S_IWGRP) ? 'w' : '-',
			(stat->st_mode & S_IXGRP) ? 'x' : '-',
			(stat->st_mode & S_IROTH) ? 'r' : '-',
			(stat->st_mode & S_IWOTH) ? 'w' : '-',
			(stat->st_mode & S_IXOTH) ? 'x' : '-');

	fprintf(stderr, "\thard link count: %li\n", (long) stat->st_nlink);
	fprintf(stderr, "\townership (uid, gid): (%li, %li)\n", (long) stat->st_uid, (long) stat->st_gid);

	if (S_ISCHR(stat->st_mode) || S_ISBLK(stat->st_mode))
	{
		fprintf(stderr, "\tspecial file device (major:minor) - %li : %li\n",
				(long) major(stat->st_rdev),
				(long) minor(stat->st_rdev));
	}
      
	fprintf(stderr, "\tsize: %lli\n", (long long) stat->st_size);
	fprintf(stderr, "\toptimation I/O block size: %li\n", (long) stat->st_blksize);
	fprintf(stderr, "\t512B blocks allocated: %lli\n", (long long) stat->st_blocks);

	fprintf(stderr, "\tlast file access:        %s", ctime(&stat->st_atime));
	fprintf(stderr, "\tlast file modification:  %s", ctime(&stat->st_mtime));
	fprintf(stderr, "\tlast file status change: %s", ctime(&stat->st_ctime));
}

void filesystem_init_func_ptrs(void)
{
	system_user_is_admin = &wasm_system_user_is_admin;

	path_is_relative = &wasm_path_is_relative;

	file_dump = &wasm_file_dump;
	file_open_for_reading = &wasm_file_open_for_reading;
	file_try_create_or_truncate = &wasm_file_try_create_or_truncate;
	file_close = &wasm_file_close;
	file_write_offset = &wasm_file_write_offset;
	file_write_append = &wasm_file_write_append;
	file_sync = &wasm_file_sync;
	file_memory_map = &wasm_file_memory_map;
	file_memory_map_partial = &wasm_file_memory_map_partial;
	file_memory_unmap = &wasm_file_memory_unmap;
	file_memory_sync_unmap = &wasm_file_memory_sync_unmap;

	directory_print_tree = &wasm_directory_print_tree;
	directory_current_path = &wasm_directory_current_path;
	directory_current_path_buffered = &wasm_directory_current_path_buffered;
	directory_get_current = &wasm_directory_get_current;
	directory_get_current_buffered = &wasm_directory_get_current_buffered;
	directory_try_create_relative_to = &wasm_directory_try_create_relative_to;
	directory_try_remove = &wasm_directory_try_remove;

	file_status_from_directory = &wasm_file_status_from_directory;
	file_status_from_handle = &wasm_file_status_from_handle;
        file_status_from_path = &wasm_file_status_from_path;
	file_status_print = &wasm_file_status_print;
}
