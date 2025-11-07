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

#ifndef __SYS_INFO_H__
#define __SYS_INFO_H__

#include <stdio.h>
#include "sys_common.h"
#include "kas_common.h"
#include "memory.h"
#include "kas_math.h"
#include "bit_vector.h"
#include "kas_string.h"
#include "hash_map.h"
#include "hierarchy_index.h"
#include "kas_vector.h"

#if __OS__ == __LINUX__
#include "linux_public.h"
#elif __OS__ == __WIN64__
#include "win_public.h"
#elif __OS__ == __WEB__
#include "wasm_public.h"
#endif

/************************************************************************/
/* 				Memory Allocation			*/
/************************************************************************/

/* returns reserved page aligned virtual memory on success, NULL on failure. */
void *	virtual_memory_reserve(const u64 size);
/* free reserved virtual memory */
void 	virtual_memory_release(void *addr, const u64 size);

/************************************************************************/
/* 				System Environment			*/
/************************************************************************/

struct kas_sys_env
{
	struct file	cwd;			/* current working directory, SHOULD ONLY BE SET ONCE 	*/
	u32 		user_privileged;	/* 1 if privileged user 			 	*/ 
};

extern struct kas_sys_env *g_sys_env;

/* allocate utf8 on arena */
extern utf8 	(*utf8_get_clipboard)(struct arena *mem);
extern void 	(*cstr_set_clipboard)(const char *utf8);

/************************************************************************/
/* 			Graphics abstraction layer 			*/
/************************************************************************/

/*
 * system window coordinate system:
 *
 *  (0,Y) ------------------------- (X,Y)
 *    |				      |
 *    |				      |
 *    |				      |
 *    |				      |
 *    |				      |
 *  (0,0) ------------------------- (X,0)
 *
 *  Since we are using a right handed coordinate system to describe the world, and the camera
 *  looks down the +Z axis, an increase in X or Y in the screen space means an "increase" from
 *  the camera's perspective as well. We must ensure that the underlying platform events that
 *  contains window coordinates are translated into this format.
 *
 *		A (Y)
 *		|
 *		|	(X)
 *		|------->
 *	       /
 *            / 
 *	     V (Z)
 */
#include "ui_public.h"
#include "array_list.h"
#include "cmd.h"

struct ui;
struct r_scene;
struct native_window;

/* init function pointers */
void 	system_graphics_init(void);
/* free any graphics resources */
void 	system_graphics_destroy(void);

extern struct hierarchy_index *	g_window_hierarchy;
extern u32 			g_process_root_window;
extern u32 			g_window;

struct system_window
{
	struct hierarchy_index_node 	header;			/* DO NOT MOVE */
	struct native_window *		native;			/* native graphics handle */
	struct ui *			ui;			/* local ui */
	struct cmd_queue *		cmd_queue;		/* local command queue */
	struct cmd_console *		cmd_console;		/* console */
	struct r_scene *		r_scene;
	struct arena 			mem_persistent;		/* peristent 1MB arena */

	u32				tagged_for_destruction; /* If tagged, free on next start of frame */
	u32				text_input_mode;	/* If on, window is receiving text input events */ 
	vec2u32				position;
	vec2u32				size;

	u32				gl_state;
};

/* alloc system_window resources, if no gl context exist, allocate context as well. */
u32 			system_window_alloc(const char *title, const vec2u32 position, const vec2u32 size, const u32 parent);
/* alloc system_window resources AND set window to global root process window, if no gl context exist, allocate context as well. */
u32 			system_process_root_window_alloc(const char *title, const vec2u32 position, const vec2u32 size);
/* Tag sub-hierachy of root (including root itself) for destruction on next frame. */
void system_window_tag_sub_hierarchy_for_destruction(const u32 root);
/* free system windows tagged for destruction */
void			system_free_tagged_windows(void);
/* get system window address  */
struct system_window *	system_window_address(const u32 index);
/* Return system window containing the given native window handle, or an empty allocation slot if no window found */
struct slot		system_window_lookup(const u64 native_handle);
/* Set window to current (global pointers to window, ui and cmd_queue is set) */
void			system_window_set_global(const u32 window);
/* enable text input mode for current window */
void 			system_window_text_input_mode_enable(void);
/* disable text input mode for current window */
void 			system_window_text_input_mode_disable(void);
/* Set system window to be the current gl context */
void			system_window_set_current_gl_context(const u32 window);
/* opengl swap buffers */
void 			system_window_swap_gl_buffers(const u32 window);
/* update system_window configuration */
void			system_window_config_update(const u32 window);
/* get system_window size */
void			system_window_size(vec2u32 size, const u32 window);


/************************************************************************/
/* 				System Inititation			*/
/************************************************************************/

/* Initiate/cleanup system resources such as timers, input handling, system events, ... */
void 		system_resources_init(struct arena *mem);
void 		system_resources_cleanup(void);

/************************************************************************/
/* 				System Events 				*/
/************************************************************************/

/* process native window events and update corresponding system window states */
void		system_process_events(void);

/************************************************************************/
/* 			system mouse/keyboard handling 			*/
/************************************************************************/

extern u32	(*system_key_modifiers)(void);
	
const char *	kas_keycode_to_string(const enum kas_keycode key);
const char *	kas_button_to_string(const enum mouse_button button);

/************************************************************************/
/* 			permissions and priviliege  			*/
/************************************************************************/

/* returns 1 if user running the process had root/administrator privileges */
extern u32 	(*system_user_is_admin)(void);

/************************************************************************/
/* 		filesystem navigation and manipulaiton			*/
/************************************************************************/

/****************************************** path operations  ****************************************/

/* returns 1 if the path is relative, otherwise 0.  */
extern u32			(*cstr_path_is_relative)(const char *path);
extern u32			(*utf8_path_is_relative)(const utf8 path);

/**************************** file opening, creating, closing and dumping *************************/

/* try close file if it is open and set to file_null  */
extern void 			(*file_close)(struct file *file);
/* Try create and open a file at the given directory; If the file already exist, error is returned. */
extern enum fs_error 		(*file_try_create)(struct arena *mem, struct file *file, const char *filename, const struct file *dir, const u32 truncate);
/* Try create and open a file at the cwd; If the file already exist, error is returned. */
extern enum fs_error 		(*file_try_create_at_cwd)(struct arena *mem, struct file *file, const char *filename, const u32 truncate);
/* Try open a file at the given directory; If the file does not exist, error is returned. */
extern enum fs_error 		(*file_try_open)(struct arena *mem, struct file *file, const char *filename, const struct file *dir, const u32 writeable);
/* Try open a file at the cwd; If the file does not exist, error is returned. */
extern enum fs_error 		(*file_try_open_at_cwd)(struct arena *mem, struct file *file, const char *filename, const u32 writeable);

/* On success, return filled buffer. On failure, set buffer to empty buffer */
extern struct kas_buffer 	(*file_dump)(struct arena *mem, const char *path, const struct file *dir);
extern struct kas_buffer 	(*file_dump_at_cwd)(struct arena *mem, const char *path);

/*********************************** file writing and  memory mapping *********************************/

/* return number of bytes written  */
extern u64 			(*file_write_offset)(const struct file *file, const u8 *buf, const u64 bufsize, const u64 file_offset);
/* return number of bytes written  */
extern u64 			(*file_write_append)(const struct file *file, const u8 *buf, const u64 bufsize);
/* flush kernel io buffers -> up to hardware to actually store flushed io. NOTE: EXTREMELY SLOW OPERATION  */
extern void 			(*file_sync)(const struct file *file);
/* returns 1 on successful size change, 0 or failure */
extern u32			(*file_set_size)(const struct file *file, const u64 size);

/* return memory mapped address of file and size of file, or NULL on failure. */
extern void *			(*file_memory_map)(u64 *size, const struct file *file, const u32 prot, const u32 flags);
/* return memory mapped address of file, or NULL on failure   */
extern void *			(*file_memory_map_partial)(const struct file *file, const u64 length, const u64 offset, const u32 prot, const u32 flags);
extern void 			(*file_memory_unmap)(void *addr, const u64 length);
/* sync mmap before unmapping.  NOTE: EXTREMELY SLOW OPERATION */
extern void 			(*file_memory_sync_unmap)(void *addr, const u64 length);

/***************************** directory creation, reading and navigation *****************************/

/* Try create and open a directory at the given directory; If the file already exist, error is returned. */
extern enum fs_error 	(*directory_try_create)(struct arena *mem, struct file *dir, const char *filename, const struct file *parent_dir);
/* Try create and open a directory at the cwd; If the file already exist, error is returned. */
extern enum fs_error 	(*directory_try_create_at_cwd)(struct arena *mem, struct file *dir, const char *filename);
/* Try open a directory at the given directory; If the file does not exist, error is returned. */
extern enum fs_error 	(*directory_try_open)(struct arena *mem, struct file *dir, const char *filename, const struct file *parent_dir);
/* Try open a directory at the cwd; If the file does not exist, error is returned. */
extern enum fs_error 	(*directory_try_open_at_cwd)(struct arena *mem, struct file *dir, const char *filename);

/*
 * directory navigator: navigation utility for reading and navigating current directory contents.
 */
struct directory_navigator
{
	utf8			path;				/* directory path  		*/ 
	struct hash_map * 	relative_path_to_file_map;	/* relative_path -> file index 	*/
	struct arena		mem_string;			/* path memory			*/
	struct vector		files;				/* file information 		*/
};

/* allocate initial memory */
struct directory_navigator 	directory_navigator_alloc(const u32 initial_memory_string_size, const u32 hash_size, const u32 initial_hash_index_size);
/* deallocate  memory */
void				directory_navigator_dealloc(struct directory_navigator *dn);	
/* flush memory and reset data structure  */
void				directory_navigator_flush(struct directory_navigator *dn);
/* returns number of paths containing substring. *index is set to u32[count] containing the matched indices.  */
u32 				directory_navigator_lookup_substring(struct arena *mem, u32 **index, struct directory_navigator *dn, const utf8 substring);
/* returns file index, or if no file found, return HASH_NULL (=U32_MAX) */
u32				directory_navigator_lookup(const struct directory_navigator *dn, const utf8 filename);
/* enter given folder and update the directory_navigator state. 
 * WARNING: aliases input path.
 * RETURNS:
 * 	KAS_FS_SUCCESS (= 0) on success,
 * 	KAS_FS_TYPE_INVALID if specified file is not a directory,
 * 	KAS_FS_PATH_INVALID if the given file does not exist,
 * 	KAS_FS_PERMISSION_DENIED if user to permitted.
 */
enum fs_error			directory_navigator_enter_and_alias_path(struct directory_navigator *dn, const utf8 path);

/**************************************** file status operations ****************************************/

/* on success, set status, on error (ret_value != FS_SUCCESS), status becomes undefined */
extern void			(*file_status_debug_print)(const file_status *stat);
extern enum file_type		(*file_status_type)(const file_status *status);
extern enum fs_error		(*file_status_file)(file_status *status, const struct file *file);
extern enum fs_error		(*file_status_path)(file_status *status, const char *path, const struct file *dir);

/************************************* process directory operations *************************************/

/* Return the absolute path of the current working directory; string is set to empty on error. */
extern utf8			(*cwd_get)(struct arena *mem);

/* Set g_sys_env->cwd and update process' internal current working directory.
 *
 * RETURNS:
 *	KAS_FS_SUCCESS on success,
 *	KAS_FS_PATH_INVALID if given file does not exist,
 * 	KAS_FS_TYPE_INVALID if given file is not a normal directory,
 *	KAS_FS_PERMISSION_DENIED if bad permissions.
 *	KAS_FS_UNSPECIFIED on unexpected error.
 */
extern enum fs_error		(*cwd_set)(struct arena *mem, const char *path);

/************************************************************************/
/* 			system timers and clocks			*/
/************************************************************************/

#if (__COMPILER__ == __EMSCRIPTEN__)
#elif (__COMPILER__ == __GCC__)
	#include <x86intrin.h>
	#define	rdtsc()			__rdtsc()
	/* RDTSC + Read OS dependent IA32_TSC_AUX. All previous instructions finsish (RW finish?) before rdtscp is run. */
	#define	rdtscp(core_addr)	__rdtscp(core_addr)
#elif (__COMPILER__ == __MSVC__)
	#include <intrin.h>
	#define	rdtsc()			__rdtsc()
	/* RDTSC + Read OS dependent IA32_TSC_AUX. All previous instructions finsish (RW finish?) before rdtscp is run. */
	#define	rdtscp(core_addr)	__rdtscp(core_addr)
#endif

void		time_init(struct arena *persistent);
extern u64	(*time_ns_start)(void);					/* return origin of process time in sys time */
extern u64	(*time_s)(void);					/* seconds since start */
extern u64	(*time_ms)(void); 					/* milliseconds since start */
extern u64	(*time_us)(void); 					/* microseconds since start */
extern u64	(*time_ns)(void); 					/* nanoseconds since start */
extern u64 	(*time_ns_from_tsc)(const u64 tsc);			/* determine time elapsed from timer initialisation start in ns using hw tsc */
extern u64	(*time_tsc_from_ns)(const u64 ns);			/* determine time elapsed from timer initialisation start in hw tsc using ns */
extern u64	(*time_ns_from_tsc_truth_source)(const u64 tsc, const u64 ns_truth, const u64 cc_truth); /* determine time elapsed from timer initialisation start in ns using hw tsc, with additional truth pair (ns, tsc) in order to reduce error) */
extern u64	(*time_tsc_from_ns_truth_source)(const u64 ns, const u64 ns_truth, const u64 cc_truth);  /* determine time elapsed from timer initialisation start in hw tsc using ns  with additional truth pair (ns, tsc) in order to reduce error) */
extern u64 	(*ns_from_tsc)(const u64 tsc);				/* transform tsc to corresponding ns */
extern u64	(*tsc_from_ns)(const u64 ns);				/* transform ns to corresponding tsc */
extern u64 	(*tsc_from_kt)(const u64 tsc);				/* transform kernel trace value to corresponding tsc */
extern u64	(*kt_from_tsc)(const u64 kt);				/* transform tsc to corresponding kernel trace value */
extern void 	(*time_set_kt_transform_parameters)(const u64 time_mult, const u64 time_zero, const u64 time_shift); /* set parameters provided by kernel to transform os_ns <-> tsc */
extern u64	(*time_ns_per_tick)(void);
extern u64 	(*freq_rdtsc)(void);
extern f64 	(*time_seconds_from_rdtsc)(const u64 ticks);

/* g_tsc_skew[logical_core_count]: estimated skew from core 0.
 * given a tsc value from core c, its corresponding tsc value on core 0 is t_0 = t_c + skew,
 * so in code we get 
 * 			tsc_c = rdtscp(&core_c)
 * 			tsc_0 = core_c + g_tsc_skew[c];
 */
extern u64 *	g_tsc_skew;

/************************************************************************/
/* 			Threads and Synchronization			*/
/************************************************************************/

/* Initiate thread local storage for master thread; should only be called once! */
void 	kas_thread_master_init(struct arena *mem);
/* Alloc thread space on arena (or heap if mem=NULL) and initialize thread */
void 	kas_thread_clone(struct arena *mem, void (*start)(kas_thread *), void *args, const u64 stack_size);
/* Exit calling thread */
void	kas_thread_exit(kas_thread *thr);
/* Wait for given thread to finish execution */
void 	kas_thread_wait(const kas_thread *thr);
/* retrieve ret value adress */
void *	kas_thread_ret_value(const kas_thread *thr);
/* retrieve ret value size */
u64	kas_thread_ret_value_size(const kas_thread *thr);
/* retrieve thread function arguments */
void *  kas_thread_args(const kas_thread *thr);
/* Release any thread allocated memory from finished thread.  NOTE: Must be called from main thread when running emscripten/wasm */
void 	kas_thread_release(kas_thread *thr);	
/* return tid (native id, thread<->process id on linux pid_t)*/
tid	kas_thread_tid(const kas_thread *thr);
/* return tid of caller */
tid 	kas_thread_self_tid(void);

/* Initiate the semaphore with a given value; NOTE: initiating an already initiated semphore is UB */
void 	semaphore_init(semaphore *sem, const u32 val); 
/* Destroy the given semaphore; NOTE: destroying a semaphore at which threads are waiting is UB */
void 	semaphore_destroy(semaphore *sem);
/* increment semaphore */
void 	semaphore_post(semaphore *sem);	
/* return 1 on successful lock aquisition, 0 otherwise. */
u32 	semaphore_wait(semaphore *sem);	
/* return 1 on successful lock aquisition, 0 otherwise. */
u32 	semaphore_trywait(semaphore *sem);	

/************************************************************************/
/* 			       Task System				*/
/************************************************************************/

#include "fifo_spmc.h"

/* NOTE: WE ASSUME MASTER THREAD/WORKER HAS ID AND INDEX 0. */

extern struct task_context *g_task_ctx;

typedef void (*TASK)(void *);

struct worker
{
	//TODO Cacheline alignment 
	struct arena	mem_frame;		/* Cleared at start of every frame */	
	u64 		usr_id;			/* worker id / index into g_task_ctx->worker[] */
	kas_thread *	thr;
	u32 		a_mem_frame_clear;	/* atomic sync-point: if set, on next task run flush mem_frame. */
};

/* Task bundle: set of tasks commited at the same time. */
struct task_bundle 
{
	semaphore 	bundle_completed;
	struct task *	tasks;
	u32 		task_count;
	u32 		a_tasks_left;
};

struct task_range 
{
	void *base;
	u64 count;
};

enum task_batch_type
{
	TASK_BATCH_BUNDLE,
	TASK_BATCH_STREAM,
};

struct task
{
	struct worker *executor;
	TASK task;
	void *input;			/* Possibly shared arguments between tasks */
	void *output;
	struct task_range *range; 	/* 	If task_range, Set if task is to run over a specific local 
					 * 	interval of range input.
					 */

	enum task_batch_type batch_type;
	void *batch;			/* pointer to bundle or stream.
					 * If task_bundle, if set, we keep track of when it is done.
					 * If task_stream, increment stream->a_completed at end.
					 * */
};

/* TODO Beware: Make sure to not false-share data between threads here, pad any structs if needed. */
struct task_context
{
	struct task_bundle bundle; /* TODO: Temporary */
	struct fifo_spmc *tasks;
	struct worker *workers;
	u32 worker_count;
};

/* Init task_context, setup threads inside task_main */
void 	task_context_init(struct arena *mem_persistent, const u32 thread_count);
/* Destory resources */
void 	task_context_destroy(struct task_context *ctx);
/* Clear any frame resources held by the task context and it's workers */
void	task_context_frame_clear(void);
/* main loop for slave workers */
void  	task_main(kas_thread *thr);
/* master worker runs any available work */
void 	task_main_master_run_available_jobs(void);

/*********************** Task Streams ***********************/ 

/*
 * Simple lock-free data structure for continuously dispatching and keeping track of work. Every task dispatched
 * using api will increment a_completed on completion. 
 */
struct task_stream
{
	u32 a_completed;	/* atomic completed tasks counter */
	u32 task_count;		/* owned by main-thread 	  */
};

/* acquire resources (if any) */
struct task_stream *	task_stream_init(struct arena *mem);
/* Dispatch task for workers to immediately pick up */
void 			task_stream_dispatch(struct arena *mem, struct task_stream *stream, TASK func, void *args);
/* spin inside method until  a_completed == total */
void			task_stream_spin_wait(struct task_stream *stream);	
/* cleanup resources (if any) */
void			task_stream_cleanup(struct task_stream *stream);

/*********************** Task Bundles ***********************/ 

/* Split input range into split_count iterable intervals. task is then run iterably, and TODO: what do we return? */
struct task_bundle *	task_bundle_split_range(struct arena *mem_task_lifetime, TASK task, const u32 split_count, void *inputs, const u64 input_count, const u64 input_element_size, void *shared_arguments);
/* Blocked wait on bundle complete */
void			task_bundle_wait(struct task_bundle *bundle);
/* Clear and release task bundle for reallocation */
void			task_bundle_release(struct task_bundle *bundle);

#endif
