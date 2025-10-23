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

#ifndef __R_LOCAL_H__
#define __R_LOCAL_H__

#include "sys_public.h"
#include "log.h"
#include "r_public.h"
#include "hierarchy_index.h"
#include "bit_vector.h"
#include "asset_public.h"
#include "string_database.h"
#include "kas_profiler.h"
#include "array_list.h"
#include "list.h"
#include "sys_gl.h"

/********************************************************
 *			r_ui.c				*
 ********************************************************/

#define S_NODE_RECT_OFFSET		(0*sizeof(vec4))
#define S_VISIBLE_RECT_OFFSET		(1*sizeof(vec4))
#define S_UV_RECT_OFFSET		(2*sizeof(vec4))
#define S_BACKGROUND_COLOR_OFFSET	(3*sizeof(vec4))
#define S_BORDER_COLOR_OFFSET 		(4*sizeof(vec4))
#define S_SPRITE_COLOR_OFFSET 		(5*sizeof(vec4))
#define S_EXTRA_OFFSET			(6*sizeof(vec4))
#define S_GRADIENT_COLOR_BR_OFFSET	(6*sizeof(vec4) + sizeof(vec3))
#define S_GRADIENT_COLOR_TR_OFFSET	(7*sizeof(vec4) + sizeof(vec3))
#define S_GRADIENT_COLOR_TL_OFFSET	(8*sizeof(vec4) + sizeof(vec3))
#define S_GRADIENT_COLOR_BL_OFFSET	(9*sizeof(vec4) + sizeof(vec3))
#define S_UI_STRIDE 			(10*sizeof(vec4) + sizeof(vec3))

#define L_UI_STRIDE 			(0)

void r_ui_draw(struct ui *ui);
/* ui program gl buffer shared instace data layout setter  */
void r_ui_buffer_shared_layout_setter(void);
/* ui program gl buffer local vertex layout setter  */
void r_ui_buffer_local_layout_setter(void);

/********************************************************
 *			r_init.c			*
 ********************************************************/

//TODO getting/loading shaders should be done through asset_system later 
/* compile shader program */
void 	r_compile_shader(u32 *prg, const char *v_filepath, const char *f_filepath);

/********************************************************
 *			r_camera.c			*
 ********************************************************/

/********************************************************
 *			r_core.c			*
 ********************************************************/

/* allocate and initialize r_static range */
struct r_static_range *r_static_range_init(struct arena *mem, const u64 vertex_offset, const u64 index_offset);

/*
 * r_program - gl program related info. Indexable by r_program_id and initalized at startup
 */
struct r_program
{
	u32	gl_program;				/* opengl program id */
	void	(* buffer_shared_layout_setter)(void);	/* opengl buffer shared (instanced) layout setter */
	void	(* buffer_local_layout_setter)(void);	/* opengl buffer local layout setter */
};

/*
 * r_program - gl texture related info. Indexable by r_texture_id and initalized at startup
 */
struct r_texture
{
	GLuint	handle;	
};

 /*
 * r_core - core render state; 
 */
struct r_core
{
	u64 		frames_elapsed;		/* frames elapsed or drawn */
	u64 		ns_elapsed;		/* process time (ns) */
	u64 		ns_tick;		/* ns per render frame; if set to 0, we redraw on each r_main entry */
	struct arena 	frame;

	struct r_program	program[PROGRAM_COUNT];
	struct r_texture	texture[TEXTURE_COUNT];

	struct bit_vec		unit_allocation;	/* check allocated indices */
	struct hierarchy_index *unit_hierarchy;		/* render unit storage 	   */

	struct string_database	mesh_database;		/* mesh storage   */
	struct hierarchy_index *proxy3d_hierarchy;	/* proxy3d storage */
	struct array_list_intrusive *static_list;	/* static storage */

#ifdef 	KAS_PHYSICS_DEBUG
	/* TODO: REMOVE */
	struct r_physics_debug	physics_debug;
#endif
	/* Speculative frame data. is set/filled using r_proxy3d_hierarchy_speculate */
	vec3ptr	frame_proxy3d_position;
	quatptr	frame_proxy3d_rotation;
};
extern struct r_core *g_r_core;

/* generate speculative positions of r_core proxy3d's, indexable by type_index */
void 			r_proxy3d_hierarchy_speculate(vec3ptr *position, quatptr *rotation, struct arena *mem, const u64 ns_time);

/********************************************************
 *			r_proxy3d.c			*
 ********************************************************/

#define R_PROXY3D_V_POSITION_OFFSET		(0)
#define R_PROXY3D_V_COLOR_OFFSET		(sizeof(vec3))
#define R_PROXY3D_V_NORMAL_OFFSET		(sizeof(vec3) + sizeof(vec4))
#define R_PROXY3D_V_TRANSLATION_OFFSET		(sizeof(vec3) + sizeof(vec4) + sizeof(vec3))
#define R_PROXY3D_V_ROTATION_OFFSET		(sizeof(vec3) + sizeof(vec4) + sizeof(vec3) + sizeof(vec3))
#define R_PROXY3D_V_PACKED_SIZE			(sizeof(vec3) + sizeof(vec4) + sizeof(vec3) + sizeof(vec3) + sizeof(vec4))

struct r_proxy3d_v
{
	vec3	position;
	vec4	color;
	vec3	normal;
	vec3	translation;
	quat	rotation;
};

/* proxy3d opengl buffer layout setter */
void 			r_proxy3d_buffer_layout_setter(void);

/********************************************************
 *			r_mesh.c			*
 ********************************************************/

/*
 * triangle layouts are described by the following attributes. The vertex data comes in the order of the attributes;
 * low valued attributes before higher valued attributes (POSITION | COLOR | UV | NORMAL)
 */
enum r_mesh_attribute
{
	R_MESH_ATTRIBUTE_POSITION 	= (1 << 0),	/* vec3 */
	R_MESH_ATTRIBUTE_COLOR 	  	= (1 << 1),	/* vec4 */
	R_MESH_ATTRIBUTE_UV       	= (1 << 2),	/* vec2 */
	R_MESH_ATTRIBUTE_NORMAL   	= (1 << 3),	/* vec3 */
};

struct r_mesh
{
	STRING_DATABASE_SLOT_STATE;				/* internal header, MAY NOT BE MOVED */
	u32				attribute_flags;	/* attribute flags describing layout of triangles */
	u32				index_count;		
	u32 *				index_data; 		/* index_data[index_count] */
	u32				index_max_used;		/* max used index */
	u32				vertex_count;   	
	void *				vertex_data;		/* vertex_data[vertex_count] : layout according to attributes */
};

/**************** TEMPORARY: quick and dirty mesh generation *****************/

/* setup stub box mesh */

#include "collision.h"

void 	r_mesh_set_stub_box(struct r_mesh *mesh_stub);

/*************************** opengl context state ****************************/

struct gl_limits
{
	/* shader limits */
	GLuint 			max_tx_units_vertex;
	GLuint 			max_tx_units_fragment;
	GLuint 			max_vertex_attributes;
	GLuint			max_varying_vectors;

	/* textures limits */
	GLsizei			max_2d_tx_size;
	GLsizei			max_cube_map_tx_size;

	/* texture units */
	GLuint 			tx_unit_count;

	/* buffers */
	GLuint			max_element_index;
};

extern struct gl_limits *g_gl_limits;

struct gl_state
{
	struct array_list_intrusive_node	header;

	/* texture units */
	GLenum 			tx_unit_active;
	struct gl_texture_unit *tx_unit;

	/* depth testing */
	u32			depth;

	/* program */
	u32 			program;

	/* culling */
	u32			cull_face;
	GLenum			cull_mode;
	GLenum			face_front;

	/* blending  */
	u32			blend;
	GLenum			eq_rgb;
	GLenum			eq_a;
	GLenum			func_s_rgb;
	GLenum			func_s_a;
	GLenum			func_d_rgb;
	GLenum			func_d_a;

	struct gl_functions	func;
};

struct texture_unit_binding
{
	u32 			context;
	GLuint 			tx_unit;

	DLL_SLOT_STATE;
	POOL_SLOT_STATE;
};

struct gl_texture
{
	GLuint 		name;
	struct dll 	binding_list;

	GLenum 		target;
	GLint 		wrap_s;
	GLint 		wrap_t;
	GLint 		min_filter;
	GLint 		mag_filter;

	GLint 		level;
	GLint		internalformat;
	GLsizei		width;
	GLsizei		height;
	GLenum		format;
	GLenum		type;
};

struct gl_texture_unit
{
	u32	binding;
	u32	gl_tx_2d_index;		/* index 0 is reserved for no texture (as opengl expects)  */
	u32	gl_tx_cube_map_index;	/* index 0 is reserved for no texture (as opengl expects)  */
};

GLboolean	kas_glIsEnabled(GLenum cap);
void 		kas_glGetIntegerv(GLenum pname, GLint *data);
const GLubyte * kas_glGetString(GLenum name);
void		kas_glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params);
void		kas_glGetTexParameteriv(GLenum target, GLenum pname, GLint *params);

void		kas_glClear(GLbitfield mask);
void		kas_glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);

void		kas_glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void		kas_glPolygonMode(GLenum face, GLenum mode);

void 	kas_glGenBuffers(GLsizei n, GLuint *buffers);
void 	kas_glBindBuffer(GLenum target, GLuint buffer);
void 	kas_glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
void 	kas_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size,  const void *data);
void 	kas_glDeleteBuffers(GLsizei n, const GLuint *buffers);

GLuint 	kas_glCreateProgram(void);
void 	kas_glLinkProgram(GLuint program);
void 	kas_glUseProgram(GLuint program);
void 	kas_glDeleteProgram(GLuint program);
void	kas_glGetProgramiv(GLuint program, GLenum pname, GLint *params);
void	kas_glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);

void 	kas_glDrawArrays(GLenum mode, GLint first, GLsizei count);
void 	kas_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);
void 	kas_glDrawArraysInstanced(GLenum mode, GLint first, GLint count, GLsizei primcount);
void 	kas_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount);

void 	kas_glGenVertexArrays(GLsizei n, GLuint *arrays);
void 	kas_glDeleteVertexArrays(GLsizei n, const GLuint *arrays);
void 	kas_glBindVertexArray(GLuint array);

void 	kas_glEnableVertexAttribArray(GLuint index);
void 	kas_glDisableVertexAttribArray(GLuint index);
void 	kas_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
void 	kas_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);
void 	kas_glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);
void	kas_glVertexAttribDivisor(GLuint index, GLuint divisor);

GLuint	kas_glGetUniformLocation(GLuint program, const GLchar *name);
void	kas_glUniform1f(GLint location, GLfloat v0);
void	kas_glUniform2f(GLint location, GLfloat v0, GLfloat v1);
void	kas_glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void	kas_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void	kas_glUniform1i(GLint location, GLint v0);
void	kas_glUniform2i(GLint location, GLint v0, GLint v1);
void	kas_glUniform3i(GLint location, GLint v0, GLint v1, GLint v2);
void	kas_glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
void	kas_glUniform1ui(GLint location, GLuint v0);
void	kas_glUniform2ui(GLint location, GLuint v0, GLuint v1);
void	kas_glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2);
void	kas_glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
void	kas_glUniform1fv(GLint location, GLsizei count, const GLfloat *value);
void	kas_glUniform2fv(GLint location, GLsizei count, const GLfloat *value);
void	kas_glUniform3fv(GLint location, GLsizei count, const GLfloat *value);
void	kas_glUniform4fv(GLint location, GLsizei count, const GLfloat *value);
void	kas_glUniform1iv(GLint location, GLsizei count, const GLint *value);
void	kas_glUniform2iv(GLint location, GLsizei count, const GLint *value);
void	kas_glUniform3iv(GLint location, GLsizei count, const GLint *value);
void	kas_glUniform4iv(GLint location, GLsizei count, const GLint *value);
void	kas_glUniform1uiv(GLint location, GLsizei count, const GLuint *value);
void	kas_glUniform2uiv(GLint location, GLsizei count, const GLuint *value);
void	kas_glUniform3uiv(GLint location, GLsizei count, const GLuint *value);
void	kas_glUniform4uiv(GLint location, GLsizei count, const GLuint *value);
void	kas_glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void	kas_glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void	kas_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);

/* gles2 wrappers with state checks to reduce / get statistics on calls */
void 	kas_glEnableBlending(void);
void 	kas_glDisableBlending(void);
void 	kas_glBlendEquation(const GLenum eq);
void 	kas_glBlendEquationSeparate(const GLenum eq_rgb, const GLenum eq_a);
void 	kas_glBlendFunc(const GLenum sfactor, const GLenum dfactor);
void 	kas_glBlendFuncSeparate(const GLenum srcRGB, const GLenum dstRGB, const GLenum srcAlpha, const GLenum dstAlpha);

void 	kas_glEnableFaceCulling(void);
void 	kas_glDisableFaceCulling(void);
void 	kas_glCullFace(const GLenum mode);
void 	kas_glFrontFace(const GLenum mode);

void 	kas_glEnableDepthTesting(void);
void 	kas_glDisableDepthTesting(void);

/* Note: These functions work exactly how gles2 expects them too, but in reality we are getting back and using 
 * texture indices, which under the hood gets translated to actual opengl texture names. This is done so that 
 * we can do some internal book-keeping state of buffers and opengl.
 */
void 	kas_glGenTextures(const GLsizei count, GLuint tx[]);
void 	kas_glDeleteTextures(const GLsizei count, const GLuint tx[]);
void 	kas_glBindTexture(const GLenum target, const GLuint tx);
void 	kas_glTexImage2D(const GLenum target,
		      const GLint level,
		      const GLint internalformat,
		      const GLsizei width,
		      const GLsizei height,
		      const GLint border,
		      const GLenum format,
		      const GLenum type,
		      const void *data);
void 	kas_glTexParameteri(const GLenum target, const GLenum pname, const GLint param);
void 	kas_glTexParameterf(const GLenum target, const GLenum pname, const GLfloat param);
void 	kas_glTexParameteriv(const GLenum target, const GLenum pname, const GLint *params);
void 	kas_glTexParameterfv(const GLenum target, const GLenum pname, const GLfloat *params);
void 	kas_glActiveTexture(const GLenum tx_unit);
void	kas_glGenerateMipmap(GLenum target);

void	kas_glGetShaderiv(GLuint shader, GLenum pname, GLint *params);
void	kas_glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
GLuint 	kas_glCreateShader(GLenum type);
void 	kas_glShaderSource(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
void 	kas_glCompileShader(GLuint shader);
void 	kas_glAttachShader(GLuint program, GLuint shader);
void 	kas_glDetachShader(GLuint program, GLuint shader);
void 	kas_glDeleteShader(GLuint shader);

#ifdef KAS_GL_DEBUG

/* TODO Verify that gl_state actually represents the true opengl state machine */
void gl_state_assert(void);

#define GL_STATE_ASSERT		gl_state_assert();

#else

#define GL_STATE_ASSERT	

#endif



#endif
