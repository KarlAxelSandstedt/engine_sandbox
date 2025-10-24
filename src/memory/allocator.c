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

#include "allocator.h"
#include "sys_public.h"
#include "allocator_debug.h"
#include "kas_math.h"
#include "log.h"

void arena_push_record(struct arena *ar)
{
	const u64 rec_mem_left = ar->mem_left;
	struct arena_record *record = arena_push(ar, sizeof(struct arena_record));
	if (record)
	{
		record->prev = ar->record;
		record->rec_mem_left = rec_mem_left;
		ar->record = record;
	}
}

void arena_pop_record(struct arena *ar)
{
	if (ar->record)
	{
		kas_assert((u64) ar->record <= (u64) ar->stack_ptr);
		kas_assert(ar->mem_left <= ar->record->rec_mem_left);
		const u64 rec_mem_left = ar->record->rec_mem_left;
		ar->record = ar->record->prev;
		arena_pop_packed(ar, rec_mem_left - ar->mem_left);
	}
}

void arena_remove_record(struct arena *ar)
{
	if (ar->record)
	{
		ar->record = ar->record->prev;
	}
}

struct arena arena_record_and_unpoison(struct arena *arena_addr)
{
	UNPOISON_ADDRESS(arena_addr->stack_ptr, arena_addr->mem_left);
	return *arena_addr;	
}

struct arena arena_record_release_and_poison(struct arena *record_addr)
{
	POISON_ADDRESS(record_addr->stack_ptr, record_addr->mem_left);
	return *record_addr;
}

struct arena arena_alloc(const u64 size)
{
	struct arena ar =
	{
		.mem_size = 0,
		.mem_left = 0,
		.record = NULL,
	};

	ar.stack_ptr = virtual_memory_reserve(size);

	if (ar.stack_ptr)
	{
		ar.mem_size = size;
		ar.mem_left = size;
		POISON_ADDRESS(ar.stack_ptr, ar.mem_left);
	}
	
	return ar;
}

void arena_free(struct arena *ar)
{
	ar->stack_ptr -= ar->mem_size - ar->mem_left;
	UNPOISON_ADDRESS(ar->stack_ptr, ar->mem_size);
	virtual_memory_release(ar->stack_ptr, ar->mem_size);
	ar->mem_size = 0;
	ar->mem_left = 0;
	ar->stack_ptr = NULL;
	ar->record = NULL;
}

struct arena arena_alloc_1MB(void)
{
	struct arena ar =
	{
		.mem_size = 0,
		.mem_left = 0,
		.record = NULL,
	};

	ar.stack_ptr = thread_alloc_1MB();
	if (ar.stack_ptr)
	{
		ar.mem_size = 1024*1024;
		ar.mem_left = 1024*1024;
		POISON_ADDRESS(ar.stack_ptr, ar.mem_left);
	}

	return ar;
}

void arena_free_1MB(struct arena *ar)
{
	ar->stack_ptr -= ar->mem_size - ar->mem_left;
	UNPOISON_ADDRESS(ar->stack_ptr, ar->mem_size);
	thread_free_1MB(ar->stack_ptr);
}

void arena_flush(struct arena* ar)
{
	if (ar)
	{
		ar->stack_ptr -= ar->mem_size - ar->mem_left;
		ar->mem_left = ar->mem_size;
		ar->record = NULL;
		POISON_ADDRESS(ar->stack_ptr, ar->mem_left);
	}
}

void arena_pop_packed(struct arena *ar, const u64 mem_to_pop)
{
	kas_assert_string(ar->mem_size - ar->mem_left >= mem_to_pop, "Trying to pop memory outside of arena");

	ar->stack_ptr -= mem_to_pop;
	ar->mem_left += mem_to_pop;
	POISON_ADDRESS(ar->stack_ptr, mem_to_pop);
}

void *arena_push_aligned(struct arena *ar, const u64 size, const u64 alignment)
{
	kas_assert(is_power_of_two(alignment) == 1);

	void* alloc_addr = NULL;
	if (size) 
	{ 
		const u64 mod = ((u64) ar->stack_ptr) & (alignment - 1);
		const u64 push_alignment = (!!mod) * (alignment - mod);

		if (ar->mem_left >= size + push_alignment) 
		{
			UNPOISON_ADDRESS(ar->stack_ptr + push_alignment, size);
			alloc_addr = ar->stack_ptr + push_alignment;
			ar->mem_left -= size + push_alignment;
			ar->stack_ptr += size + push_alignment;
		}
	}

	return alloc_addr;
}


void *arena_push_aligned_memcpy(struct arena *ar, const void *copy, const u64 size, const u64 alignment)
{
	void *addr = arena_push_aligned(ar, size, alignment);
	if (addr)
	{
		memcpy(addr, copy, size);
	}
	return addr;
}

void *arena_push_aligned_zero(struct arena *ar, const u64 size, const u64 alignment)
{
	void *addr = arena_push_aligned(ar, size, alignment);
	if (addr)
	{
		memset(addr, 0, size);
	}
	return addr;
}

struct allocation_array arena_push_aligned_all(struct arena *ar, const u64 slot_size, const u64 alignment)
{
	kas_assert(is_power_of_two(alignment) == 1 && slot_size > 0);

	struct allocation_array array = { .len = 0, .addr = NULL, .mem_pushed = 0 };
	const u64 mod = ((u64) ar->stack_ptr) & (alignment - 1);
	const u64 push_alignment = (!!mod) * (alignment - mod);
	if (push_alignment + slot_size <= ar->mem_left)
	{
		array.len = (ar->mem_left - push_alignment) / slot_size;
		array.addr = ar->stack_ptr + push_alignment;
		UNPOISON_ADDRESS(ar->stack_ptr + push_alignment, array.len * slot_size);
		array.mem_pushed = push_alignment + array.len * slot_size;
		ar->mem_left  -= push_alignment + array.len * slot_size;
		ar->stack_ptr += push_alignment + array.len * slot_size;
	}

	return array;
}

enum thread_alloc_ret
{
	ALLOCATOR_SUCCESS,
	ALLOCATOR_FAILURE,
	ALLOCATOR_OUT_OF_MEMORY,
	ALLOCATOR_COUNT
};

struct thread_block_header
{
	u64 	id;
	u64 	next;
};

#define LOCAL_MAX_COUNT		32
#define LOCAL_FREE_LOW  	16
#define LOCAL_FREE_HIGH 	31
static kas_thread_local u32 local_count = 1;	/* local_next[0] is dummy */
static kas_thread_local u64 local_next[LOCAL_MAX_COUNT];

struct thread_block_allocator *g_block_allocator_256B;
struct thread_block_allocator *g_block_allocator_1MB;

void global_thread_block_allocators_alloc(const u32 count_256B, const u32 count_1MB)
{
	g_block_allocator_256B = thread_block_allocator_alloc(count_256B, 256);
	g_block_allocator_1MB = thread_block_allocator_alloc(count_1MB, 1024*1024);
}

void global_thread_block_allocators_free()
{
	thread_block_allocator_free(g_block_allocator_256B);
	thread_block_allocator_free(g_block_allocator_1MB);
}

struct thread_block_allocator *thread_block_allocator_alloc(const u64 block_count, const u64 block_size)
{
	kas_static_assert(LOCAL_MAX_COUNT - 1 == LOCAL_FREE_HIGH, "");
	kas_static_assert(LOCAL_FREE_LOW <= LOCAL_FREE_HIGH, "");
	kas_static_assert(1 <= LOCAL_FREE_LOW, "");

	kas_assert(block_count && block_size);
	struct thread_block_allocator *allocator = NULL;
       	memory_alloc_aligned(&allocator, sizeof(struct thread_block_allocator), g_arch_config->cacheline);
	if (!allocator)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to allocate block allocator");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}

	allocator->max_count = block_count;
	const u64 mod = (block_size % g_arch_config->cacheline);
	u64 actual_block_size = (mod)
		? g_arch_config->cacheline + block_size + (g_arch_config->cacheline - mod)
		: g_arch_config->cacheline + block_size;

	allocator->block_size = actual_block_size;
	allocator->block = virtual_memory_reserve(block_count * allocator->block_size);
	kas_assert_string(((u64) allocator->block & (g_arch_config->cacheline-1)) == 0, "allocator block array should be cacheline aligned");
	if (!allocator->block)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to allocate block allocator->block");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}
	/* sync point (gen, index) = (0,0) */
	atomic_store_rel_64(&allocator->a_next, 0);
	return allocator;
}

void thread_block_allocator_free(struct thread_block_allocator *allocator)
{
	virtual_memory_release(allocator->block, allocator->max_count*allocator->block_size);
}

static enum thread_alloc_ret thread_block_try_alloc(void **addr, u64 *a_next, struct thread_block_allocator *allocator)
{
	/* a_next has been AQUIRED, so no local write/reads can move above (compiler and cpu), 
	 * stores by other threads that have released the address is now visible locally.  */
	const u64 gen = *a_next >> 32;
	const u64 index = *a_next & U32_MAX;
	if (index == allocator->max_count) { return ALLOCATOR_OUT_OF_MEMORY; }
	
	/* This block may already be owned by another thread, or undefined. Thus, we view the header values
	 * as garbage if the generation is 0 OR if the allocator has been tampered with since our AQUIRE of
	 * a_next. */
	struct thread_block_header *header = (struct thread_block_header *) (allocator->block + index*allocator->block_size);

	/* Unallocated blocks always start on generation 0, that we we can identify if the free list is empty */
	const u64 new_next = (gen == 0)
		? index+1
		: atomic_load_rlx_64(&header->next);

	/* Relaxed store on success, Aquire read on failure: 
	 * 	If we succeed in the exchange, no tampering of the allocator has happened and we make 
	 * 	can make a relaxed store; The only store that other threads need to read from us is
	 * 	the sync point itself, so it may be relaxed. (GCC) Since the failure memorder may not 
	 * 	be weaker than the success order, we must use ACQ, ACQ.
	 *
	 * 	If we fail in the exchange, we need to make an aquired read of a_next again; The allocator
	 * 	may be in a state in which we will read thread written headers.  */
	if (atomic_compare_exchange_acq_64(&allocator->a_next, a_next, new_next))
	{
		*addr = (u8 *) header + g_arch_config->cacheline;
		/* update generation */
		header->id = *a_next + ((u64) 1 << 32);
		return ALLOCATOR_SUCCESS;
	}
	else
	{
		return ALLOCATOR_FAILURE;
	}
}

static enum thread_alloc_ret thread_block_try_free(struct thread_block_header *header, struct thread_block_allocator *allocator, const u64 new_next)
{
	/*
	 * On success, we release store, making our local header changes visible for any thread trying to allocate
	 * this block again. 
	 *
	 * On failure, we may do a relaxed load of the next allocation identifier. We are never dereferencing it
	 * in our pop procedure, so this is okay.
	 */
	return (atomic_compare_exchange_rel_64(&allocator->a_next, &header->next, new_next))
		? ALLOCATOR_SUCCESS
		: ALLOCATOR_FAILURE;
}

void *thread_block_alloc(struct thread_block_allocator *allocator)
{
	void *addr;
	enum thread_alloc_ret ret;

	u64 a_next = atomic_load_acq_64(&allocator->a_next);
	while ((ret = thread_block_try_alloc(&addr, &a_next, allocator)) == ALLOCATOR_FAILURE);

	kas_assert(ret != ALLOCATOR_OUT_OF_MEMORY);

	return (ret != ALLOCATOR_OUT_OF_MEMORY) 
		? addr 
		: NULL;
}


void thread_block_free(struct thread_block_allocator *allocator, void *addr)
{
	struct thread_block_header *header = (struct thread_block_header *) ((u8 *) addr - g_arch_config->cacheline);
	header->next = atomic_load_rlx_64(&allocator->a_next);
	while (thread_block_try_free(header, allocator, header->id) == ALLOCATOR_FAILURE);
}

void *thread_block_alloc_256B(struct thread_block_allocator *allocator)
{
	void *addr;
	enum thread_alloc_ret ret;

	if (local_count > 1)
	{
		const u64 next = local_next[--local_count];
		const u32 index = next & U32_MAX;
		struct thread_block_header *header = (struct thread_block_header *) (allocator->block + index*allocator->block_size);
		header->id = next + ((u64) 1 << 32);
		addr = (u8 *) header + g_arch_config->cacheline;
		ret = ALLOCATOR_SUCCESS;
	}
	else
	{
		u64 a_next = atomic_load_acq_64(&allocator->a_next);
		while ((ret = thread_block_try_alloc(&addr, &a_next, allocator)) == ALLOCATOR_FAILURE);
	}

	return (ret != ALLOCATOR_OUT_OF_MEMORY) 
		? addr 
		: NULL;
}

void thread_block_free_256B(struct thread_block_allocator *allocator, void *addr)
{
	struct thread_block_header *header;
	if (local_count == LOCAL_MAX_COUNT)
	{
		/* local_next[0] (DUMMY)  <- local_next[1] <- ... <- local_next[LOCAL_FREE_HIGH] */
		u64 head = local_next[LOCAL_FREE_HIGH];
		u64 tail = local_next[LOCAL_FREE_LOW];

		header = (struct thread_block_header *) (allocator->block + (tail & U32_MAX)*allocator->block_size);
		header->next = atomic_load_rlx_64(&allocator->a_next);
		while (thread_block_try_free(header, allocator, head) == ALLOCATOR_FAILURE);
		local_count = LOCAL_FREE_LOW;
	}

	/* local_next[0] (DUMMY)  <- local_next[1] <- ... <- local_next[local_count] */
	header = (struct thread_block_header *) ((u8 *) addr - g_arch_config->cacheline);
	atomic_store_rel_32(&header->next, local_next[local_count-1]);
	local_next[local_count++] = header->id;
}

void *thread_alloc_256B(void)
{
	return thread_block_alloc_256B(g_block_allocator_256B);
}

void *thread_alloc_1MB(void)
{
	return thread_block_alloc(g_block_allocator_1MB);
}

void thread_free_256B(void *addr)
{
	thread_block_free_256B(g_block_allocator_256B, addr);
}

void thread_free_1MB(void *addr)
{
	thread_block_free(g_block_allocator_1MB, addr);
}

struct ring ring_empty()
{
	return (struct ring) { .mem_total = 0, .mem_left = 0, .offset = 0, .buf = NULL };
}

#if __OS__ == __LINUX__

#include <fcntl.h>
#include "kas_string.h"

struct ring ring_alloc(const u64 mem_hint)
{
	kas_assert(mem_hint);

	const char shm_suffix[6] = "_Rbuf";
	static u64 id = 0;
	char shm_str[39]; /* posix IPC shared memory id */
	shm_str[0] = '\\';
	utf8 shm_id = utf8_u64_buffered((u8 *) shm_str + 1, sizeof(shm_str) - 1 - sizeof(shm_suffix), id++);	

	memcpy(shm_str + 1 + shm_id.size, shm_suffix, sizeof(shm_suffix));

	u8 *alias = NULL;
	const u64 mod = mem_hint % g_arch_config->pagesize;
	struct ring ring =
	{
		ring.mem_total = mem_hint + (!!mod) * (g_arch_config->pagesize - mod),
		ring.mem_left = ring.mem_total,
		ring.offset = 0,
		ring.buf = MAP_FAILED,
	};

	/* (1) open shared fd with 0 mem */
	i32 shm_fd = shm_open(shm_str, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (shm_fd != -1)
	{
		/* (2) we got the shared fd, so me way close the shared object again. All of this can apparently be
		 * done using ordinary open(), but that has implications for how the OS handles things, so we go for
		 * the "simpler and faster" way using IPC. */
		shm_unlink(shm_str);

		/* (3) allocate memory to shared fd */
		if (ftruncate(shm_fd, ring.mem_total) != -1)
		{
			/* (4) map contiguous virtual memory to fd 2x times */
			for (;;)
			{
				/* Get any virtual memory the kernel sees fit */
				if ((alias = mmap(NULL, ring.mem_total, 
								PROT_READ | PROT_WRITE,
							       	MAP_SHARED,
							       	shm_fd, 0)) == MAP_FAILED)
				{
					break;
				}

				/* try to allocate consecutive memory, if it fails, redo loop */
				if ((ring.buf = mmap(alias - ring.mem_total, ring.mem_total,
						       	PROT_READ | PROT_WRITE,
						       	MAP_FIXED | MAP_SHARED,
						       	shm_fd, 0)) != alias - ring.mem_total)
				{
					if (munmap(alias, ring.mem_total) == 0)
					{
						continue;
					}
				}

				break;
			}
		}
	}

	if (ring.buf == MAP_FAILED)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to allocate ring allocator: %s", strerror(errno));
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}

	/* (5) close fd, not needed anymore, we have the memory virtually mapped now */
	close(shm_fd);

	kas_assert_string(ring.buf <= alias && (u64) alias - (u64) ring.buf == ring.mem_total 
			, "alias virtual memory should come directly after buffer memory");

	return ring;
}

void ring_free(struct ring *ring)
{
	if (munmap(ring->buf, 2*ring->mem_total) == -1)
	{
		log(T_SYSTEM, S_ERROR, "%s:%d - %s", __FILE__, __LINE__, strerror(errno));
	}
	*ring = ring_empty();
}

#elif __OS__ == __WIN64__

#include <memoryapi.h>

struct ring ring_alloc(const u64 mem_hint)
{
	kas_assert(mem_hint);

	SYSTEM_INFO info;
	GetSystemInfo(&info);

	u64 bufsize = power_of_two_ceil(mem_hint);
	if (bufsize < info.dwAllocationGranularity)
	{
		bufsize = info.dwAllocationGranularity;
	}
	u8 *alloc = VirtualAlloc2(NULL, NULL, 2*bufsize, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
	if (alloc == NULL)
	{
		log_system_error(S_ERROR);
		return ring_empty();
	}

	if (!VirtualFree(alloc, bufsize, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
	{
		log_system_error(S_ERROR);
		return ring_empty();
	}

	HANDLE map = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, (DWORD) (bufsize >> 32), (DWORD) ((u32) bufsize), NULL);
	if (map == INVALID_HANDLE_VALUE)
	{
		log_system_error(S_ERROR);
		return ring_empty();
	}

	u8 *buf = MapViewOfFile3(map, INVALID_HANDLE_VALUE, alloc, 0, bufsize, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
	if (buf == NULL)
	{
		log_system_error(S_ERROR);
		return ring_empty();
	}

	if (MapViewOfFile3(map, INVALID_HANDLE_VALUE, alloc + bufsize, 0, bufsize, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0) == NULL)
	{
		log_system_error(S_ERROR);
		return ring_empty();
	}

	CloseHandle(map);

	return (struct ring) { .mem_total = bufsize, .mem_left = bufsize, .offset = 0, .buf = buf };
}

void ring_free(struct ring *ring)
{
	if (!UnmapViewOfFile(ring->buf))
	{
		log_system_error(S_ERROR);
	}
	if (!UnmapViewOfFile(ring->buf + ring->mem_total))
	{
		log_system_error(S_ERROR);
	}
	*ring = ring_empty();
}

#endif

void ring_flush(struct ring *ring)
{
	ring->mem_left = ring->mem_total;
	ring->offset = 0;
}

struct kas_buffer ring_push_start(struct ring *ring, const u64 size)
{
	kas_assert_string(size <= ring->mem_left, "ring allocator OOM, implement growable ones...");

	struct kas_buffer buf = kas_buffer_empty;
	if (size <= ring->mem_left)
	{
		ring->mem_left -= size;
		buf.data = ring->buf + ((ring->offset + ring->mem_left) % ring->mem_total);
		buf.size = size;
		buf.mem_left = size;
	}

	return buf;
}

struct kas_buffer ring_push_end(struct ring *ring, const u64 size)
{
	kas_assert_string(size <= ring->mem_left, "ring allocator OOM, implement growable ones...");

	struct kas_buffer buf = kas_buffer_empty;
	if (size <= ring->mem_left)
	{
		buf.data = ring->buf + ring->offset;
		buf.size = size;
		buf.mem_left = size;
		ring->mem_left -= size;
		ring->offset = (ring->offset + size) % ring->mem_total;
	}

	return buf;
}

void ring_pop_start(struct ring *ring, const u64 size)
{
	kas_assert(size + ring->mem_left <= ring->mem_total);
	ring->mem_left += size;
}

void ring_pop_end(struct ring *ring, const u64 size)
{
	kas_assert(size + ring->mem_left <= ring->mem_total);
	ring->mem_left += size;
	ring->offset = (ring->mem_total + ring->offset - size) % ring->mem_total;
}

/* internal allocation of pool, use pool_alloc macro instead */
struct pool pool_alloc_internal(struct arena *mem, const u32 length, const u64 slot_size, const u64 slot_allocation_offset, const u64 slot_generation_offset, const u32 growable)
{
	kas_assert(!growable || !mem);

	struct pool pool = { 0 };

	u32 heap_allocated;
	void *buf;
	if (mem)
	{
		buf = arena_push(mem, slot_size * length);
		heap_allocated = 0;
	}
	else
	{
		buf = malloc(slot_size * length);
		heap_allocated = 1;
	}

	if (buf)
	{
		pool.slot_size = slot_size;
		pool.slot_allocation_offset = slot_allocation_offset;
		pool.slot_generation_offset = slot_generation_offset;
		pool.buf = buf;
		pool.length = length;
		pool.count = 0;
		pool.count_max = 0;
		pool.next_free = POOL_NULL;
		pool.growable = growable;
		pool.heap_allocated = heap_allocated;
		POISON_ADDRESS(pool.buf, pool.slot_size * pool.length);
	}

	return pool;
}

void pool_dealloc(struct pool *pool)
{
	if (pool->heap_allocated)
	{
		free(pool->buf);
	}	
}

void pool_flush(struct pool *pool)
{
	pool->count = 0;
	pool->count_max = 0;
	pool->next_free = POOL_NULL;
	POISON_ADDRESS(pool->buf, pool->slot_size * pool->length);
}

static void internal_pool_realloc(struct pool *pool)
{
	const u32 length_max = (U32_MAX >> 1);
	if (pool->length == length_max)
	{
		log_string(T_SYSTEM, S_FATAL, "pool allocator full, exiting");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}
	
	u32 old_length = pool->length;
	pool->length <<= 1;
	if (pool->length > length_max)
	{
		pool->length = length_max;
	}

	pool->buf = realloc(pool->buf, pool->length*pool->slot_size);
	if (!pool->buf)
	{
		log_string(T_SYSTEM, S_FATAL, "pool reallocation failed, exiting");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}

	UNPOISON_ADDRESS(pool->buf, pool->slot_size*old_length);
	POISON_ADDRESS(pool->buf + old_length*pool->slot_size, (pool->length-old_length)*pool->slot_size);
}

struct slot pool_add(struct pool *pool)
{
	kas_assert(pool->slot_generation_offset == U64_MAX);

	struct slot allocation = { .address = NULL, .index = POOL_NULL };

	u32* slot_state;
	if (pool->count < pool->length)
	{
		if (pool->next_free != POOL_NULL)
		{
			UNPOISON_ADDRESS(pool->buf + pool->next_free*pool->slot_size, pool->slot_size);
			allocation.address = pool->buf + pool->next_free*pool->slot_size;
			allocation.index = pool->next_free;

			slot_state = (u32 *) ((u8 *) allocation.address + pool->slot_allocation_offset);
			pool->next_free = *slot_state & 0x7fffffff;
			kas_assert((*slot_state & 0x80000000) == 0);
		}
		else
		{
			UNPOISON_ADDRESS(pool->buf + pool->count_max*pool->slot_size, pool->slot_size);
			allocation.address = (u8 *) pool->buf + pool->slot_size * pool->count_max;
			allocation.index = pool->count_max;
			slot_state = (u32 *) ((u8 *) allocation.address + pool->slot_allocation_offset);
			pool->count_max += 1;
		}	
		*slot_state = 0x80000000;
		pool->count += 1;
	}
	else if (pool->growable)
	{
		internal_pool_realloc(pool);
		UNPOISON_ADDRESS(pool->buf + pool->count_max*pool->slot_size, pool->slot_size);
		allocation.address = pool->buf + pool->slot_size*pool->count_max;
		allocation.index = pool->count_max;
		slot_state = (u32 *) ((u8 *) allocation.address + pool->slot_allocation_offset);
		*slot_state = 0x80000000;
		pool->count_max += 1;
		pool->count += 1;
	}

	return allocation;
}

struct slot gpool_add(struct pool *pool)
{
	kas_assert(pool->slot_generation_offset != U64_MAX);

	struct slot allocation = { .address = NULL, .index = POOL_NULL };

	u32* slot_state;
	if (pool->count < pool->length)
	{
		if (pool->next_free != POOL_NULL)
		{
			UNPOISON_ADDRESS(pool->buf + pool->next_free*pool->slot_size, pool->slot_size);
			allocation.address = pool->buf + pool->next_free*pool->slot_size;
			allocation.index = pool->next_free;

			slot_state = (u32 *) ((u8 *) allocation.address + pool->slot_allocation_offset);
			pool->next_free = *slot_state & 0x7fffffff;
			u32 *gen_state = (u32 *) ((u8 *) allocation.address + pool->slot_generation_offset);
			*gen_state += 1;
			kas_assert((*slot_state & 0x80000000) == 0);
		}
		else
		{
			UNPOISON_ADDRESS(pool->buf + pool->count_max*pool->slot_size, pool->slot_size);
			allocation.address = (u8 *) pool->buf + pool->slot_size * pool->count_max;
			allocation.index = pool->count_max;
			slot_state = (u32 *) ((u8 *) allocation.address + pool->slot_allocation_offset);
			u32 *gen_state = (u32 *) ((u8 *) allocation.address + pool->slot_generation_offset);
			*gen_state = 0;
			pool->count_max += 1;
		}	
		*slot_state = 0x80000000;
		pool->count += 1;
	}
	else if (pool->growable)
	{
		internal_pool_realloc(pool);
		UNPOISON_ADDRESS(pool->buf + pool->count_max*pool->slot_size, pool->slot_size);
		allocation.address = pool->buf + pool->slot_size*pool->count_max;
		allocation.index = pool->count_max;
		slot_state = (u32 *) ((u8 *) allocation.address + pool->slot_allocation_offset);
		u32 *gen_state = (u32 *) ((u8 *) allocation.address + pool->slot_generation_offset);
		*slot_state = 0x80000000;
		*gen_state = 0;
		pool->count_max += 1;
		pool->count += 1;
	}

	return allocation;
}

void pool_remove(struct pool *pool, const u32 index)
{
	kas_assert(index < pool->length);

	u8 *address = pool->buf + index*pool->slot_size;
	u32 *slot_state = (u32 *) (address + pool->slot_allocation_offset);
	kas_assert(*slot_state);

	*slot_state = pool->next_free;
	pool->next_free = index;
	pool->count -= 1;
	POISON_ADDRESS(pool->buf + index*pool->slot_size, pool->slot_size);
}

void pool_remove_address(struct pool *pool, void *slot)
{
	const u32 index = pool_index(pool, slot);
	pool_remove(pool, index);
}

void *pool_address(const struct pool *pool, const u32 index)
{
	kas_assert(index <= pool->count_max);
	return (u8 *) pool->buf + index*pool->slot_size;
}

u32 pool_index(const struct pool *pool, const void *slot)
{
	kas_assert((u64) slot >= (u64) pool->buf);
	kas_assert((u64) slot < (u64) pool->buf + pool->length*pool->slot_size);
	kas_assert(((u64) slot - (u64) pool->buf) % pool->slot_size == 0); 
	return (u32) (((u64) slot - (u64) pool->buf) / pool->slot_size);
}

struct pool_external_slot
{
	POOL_SLOT_STATE;
};

struct pool_external pool_external_alloc(void **external_buf, const u32 length, const u64 slot_size, const u32 growable)
{
	kas_static_assert(sizeof(struct pool_external_slot) == 4, "Expect size of pool_external_slot is 4");

	*external_buf = NULL;
	struct pool_external ext = { 0 };

	struct pool pool = pool_alloc(NULL, length, struct pool_external_slot, growable);
	if (pool.length)
	{
		*external_buf = malloc(length * slot_size);
		if (*external_buf)
		{
			ext.slot_size = slot_size;
			ext.external_buf = external_buf;
			ext.pool = pool;
			POISON_ADDRESS(*external_buf, ext.slot_size * ext.pool.length);
		}
		else
		{
			pool_dealloc(&pool);
		}
	}

	return ext;
}

void pool_external_dealloc(struct pool_external *pool)
{
	pool_dealloc(&pool->pool);
	free(*pool->external_buf);
}

void pool_external_flush(struct pool_external *pool)
{
	pool_flush(&pool->pool);
	POISON_ADDRESS(*pool->external_buf, pool->slot_size * pool->pool.length);
}

struct slot pool_external_add(struct pool_external *pool)
{
	const u32 old_length = pool->pool.length;
	struct slot slot = pool_add(&pool->pool);

	if (slot.index != POOL_NULL)
	{
		if (old_length != pool->pool.length)
		{
			*pool->external_buf = realloc(*pool->external_buf, pool->pool.slot_size*pool->pool.length);
			if (*pool->external_buf == NULL)
			{
				log_string(T_SYSTEM, S_FATAL, "Failed to reallocate external pool buffer");
				fatal_cleanup_and_exit(kas_thread_self_tid());
			}
			UNPOISON_ADDRESS(*pool->external_buf, pool->slot_size*old_length);
			POISON_ADDRESS(((u8 *)(*pool->external_buf) + pool->slot_size*old_length), pool->slot_size*(pool->pool.length - old_length)); 
		}
			
		UNPOISON_ADDRESS((u8*) *pool->external_buf + pool->slot_size*old_length, pool->slot_size);
	}

	return slot;
}

void pool_external_remove(struct pool_external *pool, const u32 index)
{
	pool_remove(&pool->pool, index);
	POISON_ADDRESS(((u8*)*pool->external_buf) + index*pool->slot_size, pool->slot_size);
}

void pool_external_remove_address(struct pool_external *pool, void *slot)
{
	const u32 index = pool_index(&pool->pool, slot);
	pool_remove(&pool->pool, index);
	POISON_ADDRESS(((u8*)*pool->external_buf) + index*pool->slot_size, pool->slot_size);
}

void *pool_external_address(const struct pool_external *pool, const u32 index)
{
	kas_assert(index <= pool->pool.count_max);
	return ((u8*)*pool->external_buf) + index*pool->slot_size;
}

u32 pool_external_index(const struct pool_external *pool, const void *slot)
{
	kas_assert((u64) slot >= (u64) (*pool->external_buf));
	kas_assert((u64) slot < (u64) *pool->external_buf + pool->pool.length*pool->slot_size);
	kas_assert(((u64) slot - (u64) *pool->external_buf) % pool->slot_size == 0); 
	return (u32) (((u64) slot - (u64) *pool->external_buf) / pool->slot_size);
}
