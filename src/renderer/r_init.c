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

#include "r_local.h"
#include "r_public.h"

#include <string.h>

//TODO MOVE into asset_manager
#if __OS__ == __WIN64__ || __OS__ ==  __LINUX__
#define vertex_ui		"../assets/shaders/ui.vert"
#define fragment_ui		"../assets/shaders/ui.frag"
#define vertex_proxy3d		"../assets/shaders/proxy3d.vert"
#define fragment_proxy3d	"../assets/shaders/proxy3d.frag"
#define vertex_color		"../assets/shaders/color.vert"
#define fragment_color		"../assets/shaders/color.frag"
#elif __OS__ == __WEB__
#define vertex_ui		"../assets/shaders/gles_ui.vert"
#define fragment_ui		"../assets/shaders/gles_ui.frag"
#define vertex_proxy3d		"../assets/shaders/gles_proxy3d.vert"
#define fragment_proxy3d	"../assets/shaders/gles_proxy3d.frag"
#define vertex_color		"../assets/shaders/gles_color.vert"
#define fragment_color		"../assets/shaders/gles_color.frag"
#endif

static void shader_source_and_compile(GLuint shader, const char *filepath)
{
	FILE *file = fopen(filepath, "r");

	char buf[4096];
	memset(buf, 0, sizeof(buf));
	fseek(file, 0, SEEK_END);
	long int size = ftell(file);
	fseek(file, 0, SEEK_SET);
	fread(buf, 1, size, file);

	const GLchar *buf_ptr = buf;
	kas_glShaderSource(shader, 1, &buf_ptr, 0);

	fclose(file);

	kas_glCompileShader(shader);	

	GLint compiled;
	kas_glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled == GL_FALSE)
	{
		GLsizei len = 0;
		kas_glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
		kas_glGetShaderInfoLog(shader, len, &len, buf);
		log(T_RENDERER, S_FATAL, "Failed to compile %s, %s", filepath, buf);
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}
}

void r_compile_shader(u32 *prg, const char *v_filepath, const char *f_filepath)
{
	GLuint v_sh = kas_glCreateShader(GL_VERTEX_SHADER);
	GLuint f_sh = kas_glCreateShader(GL_FRAGMENT_SHADER);

	shader_source_and_compile(v_sh, v_filepath);
	shader_source_and_compile(f_sh, f_filepath);

	*prg = kas_glCreateProgram();

	kas_glAttachShader(*prg, v_sh);
	kas_glAttachShader(*prg, f_sh);

	kas_glLinkProgram(*prg);
	GLint success;
	kas_glGetProgramiv(*prg, GL_LINK_STATUS, &success);
	if (!success)
	{
		char buf[4096];
		GLsizei len = 0;
		kas_glGetProgramiv(*prg, GL_INFO_LOG_LENGTH, &len);
		kas_glGetProgramInfoLog(*prg, len, &len, buf);
		log(T_RENDERER, S_FATAL, "Failed to Link program: %s", buf);
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}

	kas_glDetachShader(*prg, v_sh);
	kas_glDetachShader(*prg, f_sh);

	kas_glDeleteShader(v_sh);
	kas_glDeleteShader(f_sh);
}

void r_color_buffer_layout_setter(void)
{
	kas_glEnableVertexAttribArray(0);
	kas_glEnableVertexAttribArray(1);

	const u64 stride = sizeof(vec3) + sizeof(vec4);

	kas_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (GLsizei)  stride, 0);
	kas_glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, (GLsizei)  stride, (void *)(sizeof(vec3)));

}

void r_init(struct arena *mem_persistent, const u64 ns_tick, const u64 frame_size, const u64 core_unit_count, struct string_database *mesh_database)
{
	g_r_core->frames_elapsed = 0;	
	g_r_core->ns_elapsed = 0;	
	g_r_core->ns_tick = ns_tick;	

	r_compile_shader(&g_r_core->program[PROGRAM_UI].gl_program, vertex_ui, fragment_ui);
	g_r_core->program[PROGRAM_UI].buffer_shared_layout_setter = r_ui_buffer_shared_layout_setter;
	g_r_core->program[PROGRAM_UI].buffer_local_layout_setter = r_ui_buffer_local_layout_setter;

	r_compile_shader(&g_r_core->program[PROGRAM_PROXY3D].gl_program, vertex_proxy3d, fragment_proxy3d);
	g_r_core->program[PROGRAM_PROXY3D].buffer_shared_layout_setter = r_proxy3d_buffer_shared_layout_setter;
	g_r_core->program[PROGRAM_PROXY3D].buffer_local_layout_setter = r_proxy3d_buffer_local_layout_setter;

	r_compile_shader(&g_r_core->program[PROGRAM_COLOR].gl_program, vertex_color, fragment_color);
	g_r_core->program[PROGRAM_COLOR].buffer_shared_layout_setter = NULL;
	g_r_core->program[PROGRAM_COLOR].buffer_local_layout_setter = r_color_buffer_layout_setter;

	g_r_core->frame = arena_alloc(frame_size); 
	if (g_r_core->frame.mem_size == 0)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to allocate renderer frame, exiting.");
		fatal_cleanup_and_exit(0);
	}

	g_r_core->proxy3d_hierarchy = hierarchy_index_alloc(NULL, core_unit_count, sizeof(struct r_proxy3d), HI_GROWABLE);
	if (g_r_core->proxy3d_hierarchy == NULL)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to allocate r_core unit hierarchy, exiting.");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}

	struct slot slot3d = hierarchy_index_add(g_r_core->proxy3d_hierarchy, HI_NULL_INDEX);
	g_r_core->proxy3d_root = slot3d.index;
	kas_assert(g_r_core->proxy3d_root == PROXY3D_ROOT);
	struct r_proxy3d *stub3d = slot3d.address;
	vec3_set(stub3d->position, 0.0f, 0.0f, 0.0f);
	vec3_set(stub3d->spec_position, 0.0f, 0.0f, 0.0f);
	const vec3 axis = { 0.0f, 1.0f, 0.0f };
	unit_axis_angle_to_quaternion(stub3d->rotation, axis, 0.0f);
	quat_copy(stub3d->spec_rotation, stub3d->rotation);
	vec3_set(stub3d->linear.linear_velocity, 0.0f, 0.0f, 0.0f);
	vec3_set(stub3d->linear.angular_velocity, 0.0f, 0.0f, 0.0f);
	stub3d->flags = 0;

	g_r_core->mesh_database = mesh_database; 
	struct r_mesh *stub = string_database_address(g_r_core->mesh_database, STRING_DATABASE_STUB_INDEX);
	r_mesh_set_stub_box(stub);

	const vec3 position = {3.0f, 1.0f, -3.0f};
	const vec3 left = {1.0f, 0.0f, 0.0f};
	const vec3 up = {0.0f, 1.0f, 0.0f};
	const vec3 dir = {0.0f, 0.0f, 1.0f};
	vec2 size = { 1280.0f, 720.0f };
	r_camera_construct(&g_r_core->cam, 
			position, 
			left,
			up,
			dir,
			0.0f,
			0.0f,
			0.0250f,
			1024.0f,
			(f32) size[0] / size[1],
			2.0f * MM_PI_F / 3.0f );





	g_r_core->texture[TEXTURE_STUB].handle = 0;

	arena_push_record(mem_persistent);
	const struct asset_font *a_f = asset_database_request_font(&g_r_core->frame, FONT_DEFAULT_SMALL);
	u32 w = a_f->font->pixmap_width;
	u32 h = a_f->font->pixmap_height;
	u8 *pixel8 = a_f->font->pixmap;
	u32 *pixel32 = arena_push(mem_persistent, w*h*sizeof(u32));
	for (u32 i = 0; i < w*h; ++i)
	{
		pixel32[i] = ((u32) pixel8[i] << 24) + 0xffffff;
	}
	kas_glGenTextures(1, &g_r_core->texture[TEXTURE_FONT_DEFAULT_SMALL].handle);
	kas_glActiveTexture(GL_TEXTURE0 + 0);
	kas_glBindTexture(GL_TEXTURE_2D, g_r_core->texture[TEXTURE_FONT_DEFAULT_SMALL].handle);
	kas_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel32);
	kas_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	kas_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	kas_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	kas_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	kas_glGenerateMipmap(GL_TEXTURE_2D);

	a_f = asset_database_request_font(&g_r_core->frame, FONT_DEFAULT_MEDIUM);
	w = a_f->font->pixmap_width;
	h = a_f->font->pixmap_height;
	pixel8 = a_f->font->pixmap;
	pixel32 = arena_push(mem_persistent, w*h*sizeof(u32));
	for (u32 i = 0; i < w*h; ++i)
	{
		pixel32[i] = ((u32) pixel8[i] << 24) + 0xffffff;
	}
	kas_glGenTextures(1, &g_r_core->texture[TEXTURE_FONT_DEFAULT_MEDIUM].handle);
	kas_glActiveTexture(GL_TEXTURE0 + 1);
	kas_glBindTexture(GL_TEXTURE_2D, g_r_core->texture[TEXTURE_FONT_DEFAULT_MEDIUM].handle);
	kas_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel32);
	kas_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	kas_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	kas_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	kas_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	kas_glGenerateMipmap(GL_TEXTURE_2D);

	arena_pop_record(mem_persistent);

	struct asset_ssff *asset = asset_database_request_ssff(&g_r_core->frame, SSFF_LED_ID);
	kas_glGenTextures(1, &g_r_core->texture[TEXTURE_LED].handle);
	kas_glActiveTexture(GL_TEXTURE0 + 2);
	kas_glBindTexture(GL_TEXTURE_2D, g_r_core->texture[TEXTURE_LED].handle);
	kas_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, asset->width, asset->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, asset->pixel);
	kas_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	kas_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	kas_glGenerateMipmap(GL_TEXTURE_2D);

	asset = asset_database_request_ssff(&g_r_core->frame, SSFF_NONE_ID);
	kas_glGenTextures(1, &g_r_core->texture[TEXTURE_NONE].handle);
	kas_glActiveTexture(GL_TEXTURE0 + 3);
	kas_glBindTexture(GL_TEXTURE_2D, g_r_core->texture[TEXTURE_NONE].handle);
	kas_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, asset->width, asset->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, asset->pixel);
	kas_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	kas_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	kas_glGenerateMipmap(GL_TEXTURE_2D);
}

void r_core_flush(void)
{
	g_r_core->frames_elapsed = 0;	
	g_r_core->ns_elapsed = 0;	

	hierarchy_index_flush(g_r_core->proxy3d_hierarchy);
	struct slot slot3d = hierarchy_index_add(g_r_core->proxy3d_hierarchy, HI_NULL_INDEX);
	g_r_core->proxy3d_root = slot3d.index;
	kas_assert(g_r_core->proxy3d_root == PROXY3D_ROOT);
	struct r_proxy3d *stub3d = slot3d.address;
	vec3_set(stub3d->position, 0.0f, 0.0f, 0.0f);
	vec3_set(stub3d->spec_position, 0.0f, 0.0f, 0.0f);
	const vec3 axis = { 0.0f, 1.0f, 0.0f };
	unit_axis_angle_to_quaternion(stub3d->rotation, axis, 0.0f);
	quat_copy(stub3d->spec_rotation, stub3d->rotation);
	vec3_set(stub3d->linear.linear_velocity, 0.0f, 0.0f, 0.0f);
	vec3_set(stub3d->linear.angular_velocity, 0.0f, 0.0f, 0.0f);
	stub3d->flags = 0;

	gpool_flush(&g_r_core->unit_pool);
}
