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

#ifndef __KAS_CMD_H__
#define __KAS_CMD_H__

/*
   ========================================== CMP API ==========================================

   User submit commands either by utf8 command string or by the command's registered index. For the second case, we
   also have to push register values before submitting the command. command_submit then reads for each of it's 
   arguments the corresponding register, beginning with reg_0.

	Suppose we have the triple  { cmd_func : u32, str(func) : utf8, func() : void(*)(void) } where func() 
	normally would take in 2 arguments. For utf8, we format and submit a command executing the function by
	calling either one of the two following methods:

   		cmd_submit_utf8_f("func %arg0 %arg1", arg0, arg1)
   		cmd_submit_utf8("func str(arg0) str(arg1)")
	
	The format string should, if the command in registered at compile time, determined at the same time. So
	we extend our triple to 

	{ cmd_func : u32, str(func) : utf8, str(func_format) : utf8,  func() : void(*)(void) }
		
	If we wish to sumbit a command using function's index, we manually "push" arguments by
	setting the registers to their corresponding arguments before calling cmd_submit:

		g_cmd_q->reg[0].arg0_type = arg0;
		g_cmd_q->reg[1].arg1_type = arg1;
		cmd_submit(cmd_index)
 */

#include "allocator.h"
#include "array_list.h"
#include "kas_string.h"

enum cmd_args_type
{
	CMD_ARGS_TOKEN,		/* cmd is tokenized and matched against registered commands */
	CMD_ARGS_REGISTER,	/* cmd is identifiable */
	CMD_ARGS_COUNT
};

enum cmd_id
{
	CMD_STATIC_COUNT
};

#define CMD_REGISTER_COUNT	4
union cmd_register
{
	u8	u8;
	u16	u16;
	u32	u32;
	u64	u64;
	i8	i8;
	i16	i16;
	i32	i32;
	i64	i64;
	f32	f32;
	f64	f64;
	void *	ptr;
	utf8	utf8;
	utf32	utf32;
	intv	intv;
};

typedef struct cmd_function
{
	utf8	name;
	u32 	args_count;
	void	(*call)(void);
} cmd_function;

struct cmd
{
	struct array_list_intrusive_node	header;

	const struct cmd_function*	function;
	utf8				string;				/* defined if args_type == TOKEN */
	union cmd_register		arg[CMD_REGISTER_COUNT];	/* defined if args_type == REGISTER */
	enum cmd_args_type		args_type;	
};

struct cmd_queue
{
	struct array_list_intrusive_node header;

	struct array_list_intrusive *	cmd_list;
	u32				cmd_first;
	u32				cmd_last;

	u32				cmd_first_next_frame;
	u32				cmd_last_next_frame;

	struct cmd *			cmd_exec;

	union cmd_register		regs[CMD_REGISTER_COUNT];	/* defined if args_type == REGISTER */
};

extern struct cmd_queue *	g_queue;

/* init cmd infrastrucutre */
void 			cmd_alloc(void);
/* free cmd infrastrucutre */
void 			cmd_free(void);

struct cmd_queue *	cmd_queue_alloc(void);
void 			cmd_queue_free(struct cmd_queue *queue);
/* set queue to current global queue */
void			cmd_queue_set(struct cmd_queue *queue);
/* Execute commands in global queue */
void 			cmd_queue_execute(void);
/* flush any commands in queue */
void 			cmd_queue_flush(struct cmd_queue *queue);

/* Register the command function (or overwrite existing one) */
struct allocation_slot	cmd_function_register(const utf8 name, const u32 args_count, void (*call)(void));
/* Lookup command function; if function not found, return { .index = U32_MAX, .address = NULL } */
struct allocation_slot 	cmd_function_lookup(const utf8 name);

/* push current global register values as command arguments and submit the command */
void			cmd_submit(const u32 cmd_function);
/* format a cmd string and submit the command */
void			cmd_submit_f(struct arena *mem, const char *format, ...);
/* submit a cmd string */
void			cmd_submit_utf8(const utf8 string);

/*
 * As above, but sumbit command for next frame being built. This is useful when we want to continuously spawn
 * a command from itself, but only once a frame; Thus, during command execution, we are still building the
 * current frame, so any calls to these functions will construct a command for the next wave of commands,
 * as to not fall into an infinite loop.
 */
void			cmd_submit_next_frame(const u32 cmd_function);
void			cmd_submit_f_next_frame(struct arena *mem, const char *format, ...);
void			cmd_submit_utf8_next_frame(const utf8 string);

/* push current local register values as command arguments and submit the command */
void			cmd_queue_submit(struct cmd_queue *queue, const u32 cmd_function);
/* format a cmd string and submit the command to the given command queue */
void 			cmd_queue_submit_f(struct arena *mem, struct cmd_queue *queue, const char *format, ...);
/* submit a cmd string to the given command queue */
void			cmd_queue_submit_utf8(struct cmd_queue *queue, const utf8 string);

/* similar to above but for next frame command submission */
void			cmd_queue_submit_next_frame(struct cmd_queue *queue, const u32 cmd_function);
void			cmd_queue_submit_f_next_frame(struct arena *mem, struct cmd_queue *queue, const char *format, ...);
void			cmd_queue_submit_utf8_next_frame(struct cmd_queue *queue, const utf8 string);


#endif
