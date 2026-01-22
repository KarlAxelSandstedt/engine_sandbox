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

#define	_LARGEFILE64_SOURCE
#include <dirent.h>
#include <unistd.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>

#include "linux_local.h"
#include "sys_public.h"

u32 			(*system_user_is_admin)(void);

u32			(*utf8_path_is_relative)(utf8 path);
u32			(*cstr_path_is_relative)(const char *path);

enum fs_error 		(*file_try_create)(struct arena *mem, struct file *file, const char *filename, const struct file *dir, const u32 truncate);
enum fs_error 		(*file_try_create_at_cwd)(struct arena *mem, struct file *file, const char *filename, const u32 truncate);
enum fs_error 		(*file_try_open)(struct arena *mem, struct file *file, const char *filename, const struct file *dir, const u32 writeable);
enum fs_error 		(*file_try_open_at_cwd)(struct arena *mem, struct file *file, const char *filename, const u32 writeable);
enum fs_error 		(*file_try_open_absolute)(struct arena *mem, struct file *file, const char *filename);

enum fs_error 		(*directory_try_create)(struct arena *mem, struct file *dir, const char *filename, const struct file *parent_dir);
enum fs_error 		(*directory_try_create_at_cwd)(struct arena *mem, struct file *dir, const char *filename);
enum fs_error 		(*directory_try_open)(struct arena *mem, struct file *dir, const char *filename, const struct file *parent_dir);
enum fs_error 		(*directory_try_open_at_cwd)(struct arena *mem, struct file *dir, const char *filename);
enum fs_error		(*directory_push_entries)(struct arena *mem, struct vector *vec, struct file *dir);

u64 			(*file_write_offset)(const struct file *file, const u8 *buf, const u64 bufsize, const u64 offset);
u64 			(*file_write_append)(const struct file *file, const u8 *buf, const u64 size);
void 			(*file_sync)(const struct file *file);
void 			(*file_close)(struct file *file);

void *			(*file_memory_map)(u64 *size, const struct file *file, const u32 prot, const u32 flags);
void *			(*file_memory_map_partial)(const struct file *file, const u64 length, const u64 offset, const u32 prot, const u32 flags);
void 			(*file_memory_unmap)(void *addr, const u64 length);
void 			(*file_memory_sync_unmap)(void *addr, const u64 length);

utf8			(*cwd_get)(struct arena *mem);
enum fs_error		(*cwd_set)(struct arena *mem, const char *path);

struct ds_buffer 	(*file_dump)(struct arena *mem, const char *path, const struct file *dir);
struct ds_buffer 	(*file_dump_at_cwd)(struct arena *mem, const char *path);

enum fs_error		(*file_status_path)(file_status *status, const char *path, const struct file *dir);
enum fs_error		(*file_status_file)(file_status *status, const struct file *file);
enum file_type		(*file_status_type)(const file_status *status);

void			(*file_status_debug_print)(const file_status *stat);

u32			(*file_set_size)(const struct file *file, const u64 size);

u32 linux_system_user_is_admin(void)
{
	return getuid() == 0;
}

u32 linux_utf8_path_is_relative(const utf8 path)
{
	u32 rel = 1;
	if (path.len && path.buf[0] == '/')
	{
		rel = 0;
	}
	return rel;
}

u32 linux_cstr_path_is_relative(const char *path)
{
	return (path[0] == '/') ? 0 : 1;
}

enum fs_error linux_file_try_create(struct arena *mem, struct file *file, const char *filename, const struct file *dir, const u32 truncate)
{
	ds_Assert(file->handle == FILE_HANDLE_INVALID);
	file->handle = FILE_HANDLE_INVALID;
		
	enum fs_error err = FS_SUCCESS;
	if (!cstr_path_is_relative(filename))
	{
		err = FS_PATH_INVALID;
	}
	else
	{
		file->handle = openat(dir->handle, filename, O_CREAT | (O_TRUNC * (!!truncate)) | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP);
		if (file->handle == FILE_HANDLE_INVALID)
		{
			switch (errno)
			{
				case EACCES: { err = FS_PERMISSION_DENIED; } break;
				/* A directory component in pathname does not exist or is a dangling symbolic link */
				case ENOENT: { err = FS_PATH_INVALID; } break;
				case EEXIST: { err = FS_ALREADY_EXISTS; } break;
				/* path is relative but dirfd != FDCWD nor a valid file descriptor */
				case EBADF: 
				case ENOTDIR: { err = FS_FILE_IS_NOT_DIRECTORY; } break;
				default: { err = FS_ERROR_UNSPECIFIED; } break;
			}
		}
	
	}

	if (err == FS_SUCCESS)
	{
		file_status status;
		file_status_file(&status, file);
		file->path = utf8_cstr(mem, filename);
		file->type = file_status_type(&status);
	}

	return err;
}

enum fs_error linux_file_try_open(struct arena *mem, struct file *file, const char *filename, const struct file *dir, const u32 writeable)
{
	ds_Assert(file->handle == FILE_HANDLE_INVALID);
	file->handle = FILE_HANDLE_INVALID;
		
	enum fs_error err = FS_SUCCESS;
	if (!cstr_path_is_relative(filename))
	{
		err = FS_PATH_INVALID;
	}
	else
	{
		file->handle = openat(dir->handle, filename, (writeable) ? O_RDWR : O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP);
		if (file->handle == FILE_HANDLE_INVALID)
		{
			switch (errno)
			{
				case EACCES: { err = FS_PERMISSION_DENIED; } break;
				/* A directory component in pathname does not exist or is a dangling symbolic link */
				case ENOENT: { err = FS_PATH_INVALID; } break;
				/* path is relative but dirfd != FDCWD nor a valid file descriptor */
				case EBADF: 
				case ENOTDIR: { err = FS_FILE_IS_NOT_DIRECTORY; } break;
				default: { err = FS_ERROR_UNSPECIFIED; } break;
			}
		}
	
	}

	if (err == FS_SUCCESS)
	{
		file->path = utf8_cstr(mem, filename);
		file->type = FILE_REGULAR;
	}

	return err;
}

enum fs_error linux_directory_try_create(struct arena *mem, struct file *dir, const char *filename, const struct file *parent_dir)
{	
	ds_Assert(dir->handle == FILE_HANDLE_INVALID);
	dir->handle = FILE_HANDLE_INVALID;
		
	enum fs_error err = FS_SUCCESS;
	if (!cstr_path_is_relative(filename))
	{
		err = FS_PATH_INVALID;
	}
	else
	{
		const mode_t mode = S_IRWXU | S_IRGRP | S_IROTH;
		if (mkdirat(parent_dir->handle, filename, mode) == 0)
		{
			err = file_try_open(mem, dir, filename, parent_dir, FILE_READ);
		}
		else
		{
			switch (errno)
			{
				case EACCES: { err = FS_PERMISSION_DENIED; } break;
				/* A directory component in pathname does not exist or is a dangling symbolic link */
				case ENOENT: { err = FS_PATH_INVALID; } break;
				case EEXIST: { err = FS_ALREADY_EXISTS; } break;
				/* path is relative but dirfd != FDCWD nor a valid file descriptor */
				case EBADF: 
				case ENOTDIR: { err = FS_FILE_IS_NOT_DIRECTORY; } break;
				default: { err = FS_ERROR_UNSPECIFIED; } break;
					   
			}
		}
	
	}

	return err;
}

enum fs_error linux_file_try_create_at_cwd(struct arena *mem, struct file *file, const char *filename, const u32 truncate)
{
	const struct file cwd = 
	{
		.handle = AT_FDCWD,
		.type = FILE_DIRECTORY,
		.path = utf8_empty(),
	};

	return linux_file_try_create(mem, file, filename, &cwd, truncate);
}

enum fs_error linux_file_try_open_at_cwd(struct arena *mem, struct file *file, const char *filename, const u32 writeable)
{
	const struct file cwd = 
	{
		.handle = AT_FDCWD,
		.type = FILE_DIRECTORY,
		.path = utf8_empty(),
	};

	return linux_file_try_open(mem, file, filename, &cwd, writeable);
}

enum fs_error linux_directory_try_create_at_cwd(struct arena *mem, struct file *dir, const char *filename)
{
	const struct file cwd = 
	{
		.handle = AT_FDCWD,
		.type = FILE_DIRECTORY,
		.path = utf8_empty(),
	};

	return linux_directory_try_create(mem, dir, filename, &cwd);
}

enum fs_error linux_directory_try_open(struct arena *mem, struct file *dir, const char *filename, const struct file *parent_dir)
{
	return linux_file_try_open(mem, dir, filename, parent_dir, 0);
}

enum fs_error linux_directory_try_open_at_cwd(struct arena *mem, struct file *dir, const char *filename)
{
	return linux_file_try_open_at_cwd(mem, dir, filename, 0);
}

struct ds_buffer linux_file_dump(struct arena *mem, const char *path, const struct file *dir)
{
	const file_handle handle = openat(dir->handle, path, O_RDONLY);
	if (handle == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR);
		return ds_buffer_empty;
	}

	
	struct stat stat;
	if (file_status_file(&stat, &(struct file ) { .handle = handle }) != FS_SUCCESS)
	{
		close(handle);
		return ds_buffer_empty;	
	}

	struct ds_buffer buf =
	{
		.size = (u64) stat.st_size,
		.mem_left = (u64) stat.st_size,
	};

	struct arena record;
	if (mem)
	{
		record = *mem;
		buf.data = arena_push(mem, (u64) stat.st_size);
	}
	else
	{
		buf.data = malloc((u64) stat.st_size);
	}

	if (!buf.data)
	{
		close(handle);
		return ds_buffer_empty;	
	}

	u64 bytes_left = buf.size;
	i64 bytes_read_in_call;
	while (0 < bytes_left)
	{
		bytes_read_in_call = read(handle, buf.data + (buf.size - bytes_left), bytes_left);
		if (bytes_read_in_call == -1)
		{
			LOG_SYSTEM_ERROR(S_ERROR);
			buf = ds_buffer_empty;
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

struct ds_buffer linux_file_dump_at_cwd(struct arena *mem, const char *path)
{
	const struct file dir = { .handle = AT_FDCWD };
	return linux_file_dump(mem, path, &dir);
}

u32 linux_file_set_size(const struct file *file, const u64 size)
{
	u32 success = 1;
	if (ftruncate(file->handle, size) == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR);
		success = 0;
	}
	return success;
}

void linux_file_close(struct file *file)
{
	if (close(file->handle) == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR);
	}

	*file = file_null();
}

u64 linux_file_write_offset(const struct file *file, const u8 *buf, const u64 bufsize, const u64 offset)
{
	if (!buf || bufsize == 0) { return 0; }

	const off_t ret = lseek64(file->handle, (off_t) offset, SEEK_SET);
	if (ret == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR);
		return 0;
	}

	ssize_t left = (ssize_t) bufsize;
	ssize_t count = 0;
	ssize_t total = 0;
	while (left)
	{
		count = write(file->handle, buf + total, left);
		if (count == -1)
		{
			LOG_SYSTEM_ERROR(S_ERROR);
			break;
		}

		total += count;
		left -= count;
	}

	return total;
}

u64 linux_file_write_append(const struct file *file, const u8 *buf, const u64 bufsize)
{
	if (!buf || bufsize == 0) { return 0; }

	const off_t ret = lseek64(file->handle, 0, SEEK_END);
	if (ret == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR);
		return 0;
	}

	ssize_t left = (ssize_t) bufsize;
	ssize_t count = 0;
	ssize_t total = 0;
	while (left)
	{
		count = write(file->handle, buf + total, left);
		if (count == -1)
		{
			LOG_SYSTEM_ERROR(S_ERROR);
			break;
		}

		total += count;
		left -= count;
	}

	return total;
}

void linux_file_sync(const struct file *file)
{
	fsync(file->handle);
}

void *linux_file_memory_map(u64 *size, const struct file *file, const u32 prot, const u32 flags)
{
	*size = 0;
	void *map = NULL;
	struct stat stat;
	if (file_status_file(&stat, file) == FS_SUCCESS)
	{
		*size = stat.st_size;
		map = file_memory_map_partial(file, stat.st_size, 0, prot, flags);
	}
	return map;
}

void *linux_file_memory_map_partial(const struct file *file, const u64 length, const u64 offset, const u32 prot, const u32 flags)
{
	struct stat stat;
	if (file_status_file(&stat, file) != FS_SUCCESS)
	{
		LOG_SYSTEM_ERROR(S_ERROR);
		return NULL;
	}

	if ((u64) stat.st_size < length + offset && !linux_file_set_size(file, offset + length))
	{
		LOG_SYSTEM_ERROR(S_ERROR);
		return NULL;
	}

	void *addr = mmap(NULL, length, prot, flags, file->handle, (off_t) offset);
	if (addr == MAP_FAILED)
	{
		LOG_SYSTEM_ERROR(S_ERROR);
		addr = NULL;
	}

	return addr;
}

void linux_file_memory_unmap(void *addr, const u64 length)
{
	if (munmap(addr, length) == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR);
	}
}

void linux_file_memory_sync_unmap(void *addr, const u64 length)
{
	if (msync(addr, length, MS_SYNC) == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR);
	}

	if (munmap(addr, length) == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR);
	}
}

utf8 linux_cwd_get(struct arena *mem)
{
	utf8 cwd =
	{
		.size = 256,
	};

	const u64 record = mem->mem_left;
	cwd.buf = arena_push(mem, cwd.size);
	while ((cwd.buf = (u8*) getcwd((char*) cwd.buf, cwd.size)) == NULL)
	{
		arena_pop_packed(mem, record - mem->mem_left);
		cwd.size *= 2;	
		if (errno != ENOMEM || cwd.size > mem->mem_left)
		{
			return utf8_empty();
		}
		cwd.buf = arena_push(mem, cwd.size);
	}

	u64 offset = 0;
	while (1)
	{
		const u32 codepoint = utf8_read_codepoint(&offset, &cwd, offset);
		if (codepoint == '\0')
		{
			break;
		}
		cwd.len += 1;
	}
	return cwd;
}

enum fs_error linux_cwd_set(struct arena *mem, const char *path)
{
	u32 ret = FS_SUCCESS;
	if (chdir(path) == -1)
	{
		switch (errno)
		{
			case EACCES:  { ret = FS_PERMISSION_DENIED; } break;
			case ENOENT:  { ret = FS_PATH_INVALID; } break;
			case ENOTDIR: { ret = FS_PATH_INVALID; } break;
			default:      { ret = FS_ERROR_UNSPECIFIED; } break;
		}
	}
	else
	{
		g_sys_env->cwd.path = linux_cwd_get(mem);
		g_sys_env->cwd.type = FILE_DIRECTORY;
		g_sys_env->cwd.handle = AT_FDCWD;
	}

	return ret;
}

enum fs_error linux_directory_push_entries(struct arena *mem, struct vector *vec, struct file *dir)
{
	u32 ret = FS_SUCCESS;
	DIR *dir_stream = fdopendir(dir->handle);
	if (dir_stream == NULL)
	{
		return FS_ERROR_UNSPECIFIED;
	}

	arena_push_record(mem);
	const u32 vec_record = vec->next;
	
	file_status status;
	struct dirent *ent;
	while ((ent = readdir(dir_stream)) != NULL)
	{
		struct file *file = vector_push(vec).address;
		file->path = utf8_cstr(mem, ent->d_name);
		if (file->path.len == 0)
		{
			ret = FS_BUFFER_TO_SMALL;
			break;
		}

		if (file_status_path(&status, ent->d_name, dir) != FS_SUCCESS)
		{
			ret = FS_ERROR_UNSPECIFIED;
			break;
		}

		file->type = file_status_type(&status);
	}

	if (ret != FS_SUCCESS)
	{
		arena_pop_record(mem);
		vec->next = vec_record;
	}
	closedir(dir_stream);
	*dir = file_null();
	return ret;
}

enum fs_error linux_file_status_file(file_status *status, const struct file *file)
{
	if (fstat(file->handle, status) == -1)
	{
		return FS_ERROR_UNSPECIFIED;
	}

	return FS_SUCCESS;
}

enum fs_error linux_file_status_path(file_status *status, const char *path, const struct file *dir)
{
	enum fs_error err = FS_SUCCESS;
	if (!cstr_path_is_relative(path))
	{
		err = FS_PATH_INVALID;
	}
	else
	{
		if (fstatat(dir->handle, path, status, 0) == -1)
		{
			LOG_SYSTEM_ERROR(S_ERROR);	
			err = FS_ERROR_UNSPECIFIED;
		}
	}

	return err;
}

void linux_file_status_debug_print(const file_status *stat)
{
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

enum file_type linux_file_status_type(const file_status *status)
{
	enum file_type type;
	switch (status->st_mode & S_IFMT)
	{
		case S_IFREG: { type = FILE_REGULAR; 		} break;
		case S_IFDIR: { type = FILE_DIRECTORY; 		} break;
		default:      { type = FILE_UNRECOGNIZED;	} break;
	}

	return type;
}

void filesystem_init_func_ptrs(void)
{
	system_user_is_admin = &linux_system_user_is_admin;

	utf8_path_is_relative = &linux_utf8_path_is_relative;
	cstr_path_is_relative = &linux_cstr_path_is_relative;

	file_try_create = &linux_file_try_create;
	file_try_create_at_cwd = &linux_file_try_create_at_cwd;
	file_try_open = &linux_file_try_open;
	file_try_open_at_cwd = &linux_file_try_open_at_cwd;

	directory_try_create = &linux_directory_try_create;
	directory_try_create_at_cwd = &linux_directory_try_create_at_cwd;
	directory_try_open = &linux_directory_try_open;
	directory_try_open_at_cwd = &linux_directory_try_open_at_cwd;
	directory_push_entries = &linux_directory_push_entries;

	cwd_get = &linux_cwd_get;
	cwd_set = &linux_cwd_set;

	file_dump = &linux_file_dump;
	file_dump_at_cwd = &linux_file_dump_at_cwd;

	file_status_file = &linux_file_status_file;
        file_status_path = &linux_file_status_path;
	file_status_debug_print = &linux_file_status_debug_print;
	file_status_type = linux_file_status_type;

	file_close = &linux_file_close;

	file_write_offset = &linux_file_write_offset;
	file_write_append = &linux_file_write_append;
	file_sync = &linux_file_sync;

	file_memory_map = &linux_file_memory_map;
	file_memory_map_partial = &linux_file_memory_map_partial;
	file_memory_unmap = &linux_file_memory_unmap;
	file_memory_sync_unmap = &linux_file_memory_sync_unmap;

	file_set_size = &linux_file_set_size;
}
